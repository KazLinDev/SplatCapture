// Copyright KaepTech, Inc. All Rights Reserved.

#include "PointCloudCaptureComponent.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "CineCameraComponent.h"
#include "Async/ParallelFor.h"
#include "SplatCaptureFunctionLibrary.h"

UPointCloudCaptureComponent::UPointCloudCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Generate Point Cloud

void UPointCloudCaptureComponent::GeneratePointCloud(
	UBoxComponent* VolumeBox,
	const TArray<FTransform>& CameraTransforms,
	AActor* CameraActor,
	bool bUseSurfaceSampling,
	bool bUseCameraTracing)
{
	CapturedPositions.Reset();

	if (!bUseSurfaceSampling && !bUseCameraTracing)
	{
		UE_LOG(LogTemp, Warning, TEXT("PointCloudCapture: Both passes off, nothing to do."));
		return;
	}

	if (bUseSurfaceSampling)
	{
		if (!VolumeBox)
		{
			UE_LOG(LogTemp, Error, TEXT("PointCloudCapture: surface pass needs VolumeBox. Skipping."));
		}
		else
		{
			RunSurfaceSampling(VolumeBox, CapturedPositions);
		}
	}

	if (bUseCameraTracing)
	{
		RunCameraTracing(CameraTransforms, CameraActor, CapturedPositions);
	}

	if (CapturedPositions.Num() > MaxPoints)
	{
		CapturedPositions.SetNum(MaxPoints);
	}

	UE_LOG(LogTemp, Log, TEXT("PointCloudCapture: Final cloud = %d points (surface=%s, cameras=%s)."),
		CapturedPositions.Num(),
		bUseSurfaceSampling ? TEXT("on") : TEXT("off"),
		bUseCameraTracing ? TEXT("on") : TEXT("off"));
}

// Surface Pass

void UPointCloudCaptureComponent::RunSurfaceSampling(UBoxComponent* VolumeBox, TArray<FVector>& OutPositions)
{
	const FVector BoxCenter = VolumeBox->GetComponentLocation();
	const FVector BoxExtent = VolumeBox->GetScaledBoxExtent();
	const FVector BoxMin    = BoxCenter - BoxExtent;
	const FVector BoxMax    = BoxCenter + BoxExtent;
	const FVector VolumeSize = BoxMax - BoxMin;

	// Find all mesh components that overlap the volume.
	TArray<UStaticMeshComponent*> MeshComponents;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == GetOwner()) continue;

		TArray<UStaticMeshComponent*> Comps;
		Actor->GetComponents<UStaticMeshComponent>(Comps);

		for (UStaticMeshComponent* Comp : Comps)
		{
			if (!Comp || !Comp->GetStaticMesh() || !Comp->IsVisible()) continue;

			const FBoxSphereBounds B = Comp->Bounds;
			const FVector MMin = B.Origin - B.BoxExtent;
			const FVector MMax = B.Origin + B.BoxExtent;

			// No overlap with volume.
			if (MMin.X > BoxMax.X || MMax.X < BoxMin.X ||
				MMin.Y > BoxMax.Y || MMax.Y < BoxMin.Y ||
				MMin.Z > BoxMax.Z || MMax.Z < BoxMin.Z) continue;

			// Skip huge meshes (sky spheres etc).
			const FVector MSize = MMax - MMin;
			if (MSize.X > VolumeSize.X * MeshSizeFilterMultiplier ||
				MSize.Y > VolumeSize.Y * MeshSizeFilterMultiplier ||
				MSize.Z > VolumeSize.Z * MeshSizeFilterMultiplier) continue;

			MeshComponents.Add(Comp);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("PointCloudCapture: Surface — found %d static meshes."), MeshComponents.Num());

	if (MeshComponents.Num() == 0) return;

	// Sum surface area, then split target points by area ratio.
	float TotalArea = 0.0f;
	TArray<float> Areas;
	Areas.SetNum(MeshComponents.Num());

	for (int32 i = 0; i < MeshComponents.Num(); ++i)
	{
		Areas[i] = ComputeMeshSurfaceArea(MeshComponents[i]);
		TotalArea += Areas[i];
	}

	const float AreaSqM = TotalArea / 10000.0f;
	const int32 Target = FMath::Clamp(
		static_cast<int32>(AreaSqM * PointDensityPerSqMeter), 100, MaxPoints);

	OutPositions.Reserve(OutPositions.Num() + Target);

	const int32 PreCount = OutPositions.Num();

	for (int32 i = 0; i < MeshComponents.Num(); ++i)
	{
		const int32 N = (TotalArea > 0.0f)
			? FMath::RoundToInt((Areas[i] / TotalArea) * Target)
			: Target / MeshComponents.Num();

		if (N > 0)
			SampleMeshSurface(MeshComponents[i], N, OutPositions);
	}

	// Drop points outside the box (meshes can stick past the wall).
	for (int32 i = OutPositions.Num() - 1; i >= PreCount; --i)
	{
		const FVector& P = OutPositions[i];
		if (P.X < BoxMin.X || P.X > BoxMax.X ||
			P.Y < BoxMin.Y || P.Y > BoxMax.Y ||
			P.Z < BoxMin.Z || P.Z > BoxMax.Z)
		{
			OutPositions.RemoveAtSwap(i, 1, false);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("PointCloudCapture: Surface pass added %d points."),
		OutPositions.Num() - PreCount);
}

// Camera Pass

void UPointCloudCaptureComponent::RunCameraTracing(
	const TArray<FTransform>& CameraTransforms,
	AActor* CameraActor,
	TArray<FVector>& OutPositions)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("PointCloudCapture: Camera tracing — no world."));
		return;
	}

	if (!CameraActor)
	{
		UE_LOG(LogTemp, Error, TEXT("PointCloudCapture: Camera tracing — null CameraActor."));
		return;
	}

	if (CameraTransforms.Num() == 0 || RaysPerCamera <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PointCloudCapture: Camera tracing — no transforms or zero rays."));
		return;
	}

	UCineCameraComponent* CameraComp = CameraActor->FindComponentByClass<UCineCameraComponent>();
	if (!CameraComp)
	{
		UE_LOG(LogTemp, Error,
			TEXT("PointCloudCapture: No CineCameraComponent on actor %s. Skipping camera pass."),
			*CameraActor->GetName());
		return;
	}

	// Same fields ExportColmapCameras reads — trace frustum and exported camera stay in sync.
	const float Focal   = CameraComp->CurrentFocalLength;
	const float SensorW = CameraComp->Filmback.SensorWidth;
	const float SensorH = CameraComp->Filmback.SensorHeight;
	if (Focal <= 0.0f || SensorW <= 0.0f || SensorH <= 0.0f)
	{
		UE_LOG(LogTemp, Error,
			TEXT("PointCloudCapture: %s has bad focal/sensor (Focal=%f, SensorW=%f, SensorH=%f)."),
			*CameraActor->GetName(), Focal, SensorW, SensorH);
		return;
	}

	// UE camera-local: X = forward, Y = right, Z = up.
	// Ray for NDC (u, v) in [-1, 1]² has direction (1, u·tan(HFOV/2), v·tan(VFOV/2)).
	const float TanHalfH = (SensorW * 0.5f) / Focal;
	const float TanHalfV = (SensorH * 0.5f) / Focal;
	const float MaxDist  = FMath::Max(MaxTraceDistance, 1.0f);
	const int32 N        = FMath::Max(RaysPerCamera, 1);

	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	Params.AddIgnoredActor(GetOwner());
	Params.AddIgnoredActor(CameraActor);

	const int32 PreCount = OutPositions.Num();
	OutPositions.Reserve(PreCount + CameraTransforms.Num() * N);

	int32 TotalHits = 0;

	for (const FTransform& CamXform : CameraTransforms)
	{
		const FVector Origin = CamXform.GetLocation();

		for (int32 i = 0; i < N; ++i)
		{
			// Random NDC point inside the frustum.
			const float u = FMath::FRandRange(-1.0f, 1.0f);
			const float v = FMath::FRandRange(-1.0f, 1.0f);

			const FVector LocalDir = FVector(1.0f, u * TanHalfH, v * TanHalfV).GetSafeNormal();
			const FVector WorldDir = CamXform.TransformVectorNoScale(LocalDir);
			const FVector End      = Origin + WorldDir * MaxDist;

			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params))
			{
				OutPositions.Add(Hit.ImpactPoint);
				++TotalHits;
			}
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("PointCloudCapture: Camera pass — %d cameras × %d rays → %d hits (focal %.2f mm)."),
		CameraTransforms.Num(), N, TotalHits, Focal);
}

// Mesh Surface Sampling (barycentric, area-weighted)

void UPointCloudCaptureComponent::SampleMeshSurface(
	UStaticMeshComponent* MeshComp, int32 NumSamples,
	TArray<FVector>& OutPositions)
{
	UStaticMesh* Mesh = MeshComp->GetStaticMesh();
	if (!Mesh) return;

	const FMeshDescription* Desc = Mesh->GetMeshDescription(0);
	if (!Desc) return;

	FStaticMeshConstAttributes Attr(*Desc);
	TVertexAttributesConstRef<FVector3f> Pos = Attr.GetVertexPositions();

	const int32 NumTri = Desc->Triangles().Num();
	if (NumTri == 0) return;

	const FTransform Xform = MeshComp->GetComponentTransform();

	// Cache triangles + build area-weighted CDF.
	struct FTri { FVector3f A, B, C; };
	TArray<FTri> Tris;
	Tris.SetNumUninitialized(NumTri);

	TArray<float> CDF;
	CDF.SetNumUninitialized(NumTri);
	float Total = 0.0f;
	int32 Idx = 0;

	for (const FTriangleID TID : Desc->Triangles().GetElementIDs())
	{
		auto VI = Desc->GetTriangleVertexInstances(TID);
		Tris[Idx].A = Pos[Desc->GetVertexInstanceVertex(VI[0])];
		Tris[Idx].B = Pos[Desc->GetVertexInstanceVertex(VI[1])];
		Tris[Idx].C = Pos[Desc->GetVertexInstanceVertex(VI[2])];

		Total += 0.5f * FVector3f::CrossProduct(
			Tris[Idx].B - Tris[Idx].A,
			Tris[Idx].C - Tris[Idx].A).Size();
		CDF[Idx] = Total;
		Idx++;
	}

	if (Total <= 0.0f) return;
	for (int32 t = 0; t < NumTri; ++t) CDF[t] /= Total;

	for (int32 s = 0; s < NumSamples; ++s)
	{
		// Pick triangle by area (binary search the CDF).
		const float R = FMath::FRand();
		int32 Lo = 0, Hi = NumTri - 1;
		while (Lo < Hi)
		{
			const int32 M = (Lo + Hi) / 2;
			CDF[M] < R ? Lo = M + 1 : Hi = M;
		}

		// Random point inside the triangle (fold if outside).
		float U = FMath::FRand(), V = FMath::FRand();
		if (U + V > 1.0f) { U = 1.0f - U; V = 1.0f - V; }

		const FTri& T = Tris[Lo];
		const FVector3f LocalPos = T.A + U * (T.B - T.A) + V * (T.C - T.A);
		OutPositions.Add(Xform.TransformPosition(FVector(LocalPos)));
	}
}

// Mesh Surface  Sampling
float UPointCloudCaptureComponent::ComputeMeshSurfaceArea(
	UStaticMeshComponent* MeshComp)
{
	UStaticMesh* Mesh = MeshComp->GetStaticMesh();
	if (!Mesh) return 0.0f;

	const FMeshDescription* Desc = Mesh->GetMeshDescription(0);
	if (!Desc) return 0.0f;

	FStaticMeshConstAttributes Attr(*Desc);
	TVertexAttributesConstRef<FVector3f> Pos = Attr.GetVertexPositions();
	const FVector S = MeshComp->GetComponentTransform().GetScale3D();
	float Area = 0.0f;

	for (const FTriangleID TID : Desc->Triangles().GetElementIDs())
	{
		auto VI = Desc->GetTriangleVertexInstances(TID);
		const FVector3f P0 = Pos[Desc->GetVertexInstanceVertex(VI[0])];
		const FVector3f P1 = Pos[Desc->GetVertexInstanceVertex(VI[1])];
		const FVector3f P2 = Pos[Desc->GetVertexInstanceVertex(VI[2])];

		const FVector W0(P0.X * S.X, P0.Y * S.Y, P0.Z * S.Z);
		const FVector W1(P1.X * S.X, P1.Y * S.Y, P1.Z * S.Z);
		const FVector W2(P2.X * S.X, P2.Y * S.Y, P2.Z * S.Z);
		Area += 0.5f * FVector::CrossProduct(W1 - W0, W2 - W0).Size();
	}

	return Area;
}

// Push To Niagara
void UPointCloudCaptureComponent::PushToNiagara(UNiagaraComponent* NiagaraComp)
{
	if (!NiagaraComp || CapturedPositions.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PointCloudCapture: PushToNiagara — null component or empty cloud."));
		return;
	}
	const float Radius    = FMath::Max(HeatmapRadius, 1.0f);
	const float RadiusSq  = Radius * Radius;
	const float InvRadius = 1.0f / Radius;

	// Spatial index: voxel → indices of points in it.
	TMap<FIntVector, TArray<int32>> Index;
	Index.Reserve(CapturedPositions.Num() / 4);

	for (int32 i = 0; i < CapturedPositions.Num(); ++i)
	{
		const FVector& P = CapturedPositions[i];
		const FIntVector V(
			FMath::FloorToInt(P.X * InvRadius),
			FMath::FloorToInt(P.Y * InvRadius),
			FMath::FloorToInt(P.Z * InvRadius));
		Index.FindOrAdd(V).Add(i);
	}

	// Per-point neighbor count. Parallel: each iteration writes one slot, reads frozen index.
	TArray<int32> Counts;
	Counts.SetNumZeroed(CapturedPositions.Num());

	ParallelFor(CapturedPositions.Num(), [&](int32 i)
	{
		const FVector& P = CapturedPositions[i];
		const FIntVector V(
			FMath::FloorToInt(P.X * InvRadius),
			FMath::FloorToInt(P.Y * InvRadius),
			FMath::FloorToInt(P.Z * InvRadius));

		int32 Count = 0;
		for (int32 dz = -1; dz <= 1; ++dz)
		for (int32 dy = -1; dy <= 1; ++dy)
		for (int32 dx = -1; dx <= 1; ++dx)
		{
			const TArray<int32>* Bucket = Index.Find(FIntVector(V.X + dx, V.Y + dy, V.Z + dz));
			if (!Bucket) continue;
			for (int32 j : *Bucket)
			{
				if (FVector::DistSquared(P, CapturedPositions[j]) < RadiusSq)
				{
					++Count;
				}
			}
		}
		Counts[i] = Count;
	});

	int32 MaxCount = 0;
	for (int32 c : Counts)
	{
		if (c > MaxCount) MaxCount = c;
	}
	const float InvMax = MaxCount > 0 ? 1.0f / float(MaxCount) : 1.0f;

	TArray<float> Density;
	Density.SetNumUninitialized(CapturedPositions.Num());
	for (int32 i = 0; i < CapturedPositions.Num(); ++i)
	{
		Density[i] = float(Counts[i]) * InvMax;
	}

	// Push positions + heatmap + count, then activate.
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComp, FName("PositionArray"), CapturedPositions);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		NiagaraComp, FName("heatmap"), Density);

	NiagaraComp->SetVariableInt(FName("PointCount"), CapturedPositions.Num());
	NiagaraComp->Activate(true);

	UE_LOG(LogTemp, Log,
		TEXT("PointCloudCapture: Pushed %d points to Niagara (radius %.1f cm, max neighbors %d)."),
		CapturedPositions.Num(), Radius, MaxCount);
}

// Deactivate Niagara

void UPointCloudCaptureComponent::DeactivateNiagara(UNiagaraComponent* NiagaraComp)
{
	if (!NiagaraComp) return;

	// Clear both arrays so Niagara frees the memory.
	TArray<FVector> EmptyVec;
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComp, FName("PositionArray"), EmptyVec);

	TArray<float> EmptyFloat;
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		NiagaraComp, FName("heatmap"), EmptyFloat);

	NiagaraComp->SetVariableInt(FName("PointCount"), 0);
	NiagaraComp->Deactivate();
}

// Export

void UPointCloudCaptureComponent::ExportToPLY(const FString& SavePath)
{
	if (CapturedPositions.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PointCloudCapture: Nothing to export."));
		return;
	}

	const bool bOk = USplatCaptureFunctionLibrary::WriteBinaryPLY(
		CapturedPositions, SavePath);

	UE_LOG(LogTemp, Log, TEXT("PointCloudCapture: PLY %s -> %s"),
		bOk ? TEXT("exported") : TEXT("FAILED"), *SavePath);
}

void UPointCloudCaptureComponent::ExportToPoints3D(const FString& SavePath)
{
	if (CapturedPositions.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PointCloudCapture: Nothing to export."));
		return;
	}

	const bool bOk = USplatCaptureFunctionLibrary::ExportColmapPoints3D(
		CapturedPositions, SavePath);

	UE_LOG(LogTemp, Log, TEXT("PointCloudCapture: points3D.txt %s -> %s"),
		bOk ? TEXT("exported") : TEXT("FAILED"), *SavePath);
}

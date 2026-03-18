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
#include "SplatCaptureFunctionLibrary.h"

UPointCloudCaptureComponent::UPointCloudCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// ============================================================================
//  Generate Point Cloud
// ============================================================================

void UPointCloudCaptureComponent::GeneratePointCloud(UBoxComponent* VolumeBox)
{
	if (!VolumeBox)
	{
		UE_LOG(LogTemp, Error, TEXT("PointCloudCapture: VolumeBox is null."));
		return;
	}

	CapturedPositions.Reset();

	const FVector BoxCenter = VolumeBox->GetComponentLocation();
	const FVector BoxExtent = VolumeBox->GetScaledBoxExtent();
	const FVector BoxMin = BoxCenter - BoxExtent;
	const FVector BoxMax = BoxCenter + BoxExtent;
	const FVector VolumeSize = BoxMax - BoxMin;

	// ── Pass 1: Static Mesh Surface Sampling ───────────────────────────

	if (bPointsFromSurface)
	{
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

				// Skip if no overlap.
				if (MMin.X > BoxMax.X || MMax.X < BoxMin.X ||
					MMin.Y > BoxMax.Y || MMax.Y < BoxMin.Y ||
					MMin.Z > BoxMax.Z || MMax.Z < BoxMin.Z) continue;

				// Skip giant meshes (sky spheres, etc).
				const FVector MSize = MMax - MMin;
				if (MSize.X > VolumeSize.X * MeshSizeFilterMultiplier ||
					MSize.Y > VolumeSize.Y * MeshSizeFilterMultiplier ||
					MSize.Z > VolumeSize.Z * MeshSizeFilterMultiplier) continue;

				MeshComponents.Add(Comp);
			}
		}

		UE_LOG(LogTemp, Log, TEXT("PointCloudCapture: Found %d static meshes."),
			MeshComponents.Num());

		if (MeshComponents.Num() > 0)
		{
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

			CapturedPositions.Reserve(Target);

			for (int32 i = 0; i < MeshComponents.Num(); ++i)
			{
				const int32 N = (TotalArea > 0.0f)
					? FMath::RoundToInt((Areas[i] / TotalArea) * Target)
					: Target / MeshComponents.Num();

				if (N > 0)
					SampleMeshSurface(MeshComponents[i], N, CapturedPositions);
			}
		}
	}

	// ── Pass 2: Line Traces  ────────────────

	if (bPointsFromTraces)
	{
		SampleByLineTraces(BoxMin, BoxMax, CapturedPositions);
	}

	// ── Cull points outside volume ─────────────────────────────────────

	for (int32 i = CapturedPositions.Num() - 1; i >= 0; --i)
	{
		const FVector& P = CapturedPositions[i];
		if (P.X < BoxMin.X || P.X > BoxMax.X ||
			P.Y < BoxMin.Y || P.Y > BoxMax.Y ||
			P.Z < BoxMin.Z || P.Z > BoxMax.Z)
		{
			CapturedPositions.RemoveAtSwap(i);
		}
	}

	if (CapturedPositions.Num() > MaxPoints)
		CapturedPositions.SetNum(MaxPoints);

	UE_LOG(LogTemp, Log, TEXT("PointCloudCapture: Generated %d points."),
		CapturedPositions.Num());
}

// ============================================================================
//  Line Trace Sampling (6 directions)
// ============================================================================

void UPointCloudCaptureComponent::SampleByLineTraces(
	const FVector& BoxMin, const FVector& BoxMax,
	TArray<FVector>& OutPositions)
{
	UWorld* World = GetWorld();
	if (!World) return;

	const FVector Size = BoxMax - BoxMin;
	const int32 Res = FMath::Max(TraceGridResolution, 10);

	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	Params.AddIgnoredActor(GetOwner());

	int32 Hits = 0;

	auto TraceGrid = [&](int32 AxisA, int32 AxisB, int32 AxisTrace, bool bPositiveDir)
	{
		const float StepA = Size[AxisA] / Res;
		const float StepB = Size[AxisB] / Res;

		for (int32 ia = 0; ia < Res; ++ia)
		{
			for (int32 ib = 0; ib < Res; ++ib)
			{
				FVector Start, End;
				Start[AxisA] = BoxMin[AxisA] + ia * StepA + FMath::FRand() * StepA;
				Start[AxisB] = BoxMin[AxisB] + ib * StepB + FMath::FRand() * StepB;
				Start[AxisTrace] = bPositiveDir ? BoxMin[AxisTrace] : BoxMax[AxisTrace];

				End = Start;
				End[AxisTrace] = bPositiveDir ? BoxMax[AxisTrace] : BoxMin[AxisTrace];

				TArray<FHitResult> HitResults;
				if (World->LineTraceMultiByChannel(HitResults, Start, End,
					ECC_Visibility, Params))
				{
					for (const FHitResult& Hit : HitResults)
					{
						OutPositions.Add(Hit.ImpactPoint);
						Hits++;
					}
				}
			}
		}
	};

	TraceGrid(1, 2, 0, true);   // +X
	TraceGrid(1, 2, 0, false);  // -X
	TraceGrid(0, 2, 1, true);   // +Y
	TraceGrid(0, 2, 1, false);  // -Y
	TraceGrid(0, 1, 2, true);   // +Z
	TraceGrid(0, 1, 2, false);  // -Z

	UE_LOG(LogTemp, Log, TEXT("PointCloudCapture: %d trace hits from %d rays."),
		Hits, Res * Res * 6);
}

// ============================================================================
//  Mesh Surface Sampling (Barycentric, FMeshDescription)
// ============================================================================

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

	// Build triangle data + area-weighted CDF.
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

	// Sample random points on triangles.
	for (int32 s = 0; s < NumSamples; ++s)
	{
		// Pick triangle weighted by area (binary search CDF).
		const float R = FMath::FRand();
		int32 Lo = 0, Hi = NumTri - 1;
		while (Lo < Hi)
		{
			const int32 M = (Lo + Hi) / 2;
			CDF[M] < R ? Lo = M + 1 : Hi = M;
		}

		// Random barycentric coordinates.
		float U = FMath::FRand(), V = FMath::FRand();
		if (U + V > 1.0f) { U = 1.0f - U; V = 1.0f - V; }

		const FTri& T = Tris[Lo];
		const FVector3f LocalPos = T.A + U * (T.B - T.A) + V * (T.C - T.A);
		OutPositions.Add(Xform.TransformPosition(FVector(LocalPos)));
	}
}

// ============================================================================
//  Surface Area (FMeshDescription)
// ============================================================================

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

// ============================================================================
//  Push to Niagara
// ============================================================================

void UPointCloudCaptureComponent::PushToNiagara(UNiagaraComponent* NiagaraComp)
{
	if (!NiagaraComp || CapturedPositions.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("PointCloudCapture: Cannot push — null or empty."));
		return;
	}

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComp, FName("PositionArray"), CapturedPositions);

	NiagaraComp->SetVariableInt(FName("PointCount"), CapturedPositions.Num());
	NiagaraComp->Activate(true);

	UE_LOG(LogTemp, Log, TEXT("PointCloudCapture: Pushed %d points to Niagara."),
		CapturedPositions.Num());
}

// ============================================================================
// Deactivate Niagara
// ============================================================================

void UPointCloudCaptureComponent::DeactivateNiagara(UNiagaraComponent* NiagaraComp)
{
	if (!NiagaraComp) return;

	// Clear the array first — this frees the memory
	TArray<FVector> Empty;
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComp, FName("PositionArray"), Empty);

	NiagaraComp->SetVariableInt(FName("PointCount"), 0);
	NiagaraComp->Deactivate();
}



// ============================================================================
//  Export
// ============================================================================

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

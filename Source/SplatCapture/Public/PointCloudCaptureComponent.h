// Copyright KaepTech, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PointCloudCaptureComponent.generated.h"

class UBoxComponent;
class UNiagaraComponent;
class UCineCameraComponent;

UCLASS(BlueprintType, Blueprintable, ClassGroup = "SplatCapture",
	meta = (BlueprintSpawnableComponent, DisplayName = "Point Cloud Capture"))
class SPLATCAPTURE_API UPointCloudCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPointCloudCaptureComponent();

	// Sampling knobs.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplatCapture|PointCloud",
		meta = (ClampMin = "1.0", UIMax = "10000.0"))
	float PointDensityPerSqMeter = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplatCapture|PointCloud",
		meta = (ClampMin = "100", UIMax = "10000000"))
	int32 MaxPoints = 1000000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplatCapture|PointCloud",
		meta = (ClampMin = "1.0", UIMax = "20.0"))
	float MeshSizeFilterMultiplier = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplatCapture|PointCloud",
		meta = (ClampMin = "1", UIMax = "50000"))
	int32 RaysPerCamera = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplatCapture|PointCloud",
		meta = (ClampMin = "1.0", UIMax = "1000000.0", ToolTip = "Max ray length in cm. 100000 = 1 km."))
	float MaxTraceDistance = 100000.0f;

	// Niagara heatmap knobs.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplatCapture|Niagara",
		meta = (ClampMin = "1.0", UIMax = "500.0", ToolTip = "Neighbor radius in cm. Each point counts other points inside this radius. Smaller = sharper. Larger = smoother."))
	float HeatmapRadius = 25.0f;

	// Generate Point the cloud. Resets first, then runs surface and/or camera passes.

	UFUNCTION(BlueprintCallable, Category = "SplatCapture|PointCloud",
		meta = (DisplayName = "Generate Point Cloud",
				ToolTip = "Run surface sampling, camera tracing, or both. Resets cloud first. All tuning knobs live on the component."))
	void GeneratePointCloud(
		UBoxComponent* VolumeBox,
		const TArray<FTransform>& CameraTransforms,
		AActor* CameraActor,
		bool bUseSurfaceSampling = true,
		bool bUseCameraTracing = true);

	// Save cloud as binary PLY.
	UFUNCTION(BlueprintCallable, Category = "SplatCapture|PointCloud")
	void ExportToPLY(const FString& SavePath);

	//Save cloud as COLMAP points3D.txt.
	UFUNCTION(BlueprintCallable, Category = "SplatCapture|PointCloud")
	void ExportToPoints3D(const FString& SavePath);

	// Push to Niagara for Previs

	UFUNCTION(BlueprintCallable, Category = "SplatCapture|PointCloud")
	void PushToNiagara(UNiagaraComponent* NiagaraComp);

	// Stop Niagara and clear its arrays.
	UFUNCTION(BlueprintCallable, Category = "SplatCapture|PointCloud")
	void DeactivateNiagara(UNiagaraComponent* NiagaraComp);

	UFUNCTION(BlueprintPure, Category = "SplatCapture|PointCloud")
	int32 GetCapturedPointCount() const { return CapturedPositions.Num(); }

private:
	TArray<FVector> CapturedPositions;

	// Both passes append into OutPositions. Camera pass reads UPROPERTY knobs directly.
	void RunSurfaceSampling(UBoxComponent* VolumeBox, TArray<FVector>& OutPositions);
	void RunCameraTracing(const TArray<FTransform>& CameraTransforms,
		AActor* CameraActor,
		TArray<FVector>& OutPositions);

	void SampleMeshSurface(UStaticMeshComponent* MeshComp, int32 NumSamples,
		TArray<FVector>& OutPositions);
	float ComputeMeshSurfaceArea(UStaticMeshComponent* MeshComp);
};

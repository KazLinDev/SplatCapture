// Copyright KaepTech, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PointCloudCaptureComponent.generated.h"

class UBoxComponent;
class UNiagaraComponent;

UCLASS(BlueprintType, Blueprintable, ClassGroup = "SplatCapture",
	meta = (BlueprintSpawnableComponent, DisplayName = "Point Cloud Capture"))
class SPLATCAPTURE_API UPointCloudCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPointCloudCaptureComponent();

	// ── Configuration ──────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplatCapture|PointCloud",
		meta = (ClampMin = "1.0", UIMax = "10000.0"))
	float PointDensityPerSqMeter = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplatCapture|PointCloud",
		meta = (ClampMin = "100", UIMax = "10000000"))
	int32 MaxPoints = 1000000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplatCapture|PointCloud")
	bool bPointsFromSurface = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplatCapture|PointCloud")
	bool bPointsFromTraces = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplatCapture|PointCloud",
		meta = (ClampMin = "10", UIMax = "500", EditCondition = "bPointsFromTraces"))
	int32 TraceGridResolution = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SplatCapture|PointCloud",
		meta = (ClampMin = "1.0", UIMax = "20.0"))
	float MeshSizeFilterMultiplier = 5.0f;

	// ── API ────────────────────────────────────────────────────────────

	/** Samples all geometry inside the volume box. */
	UFUNCTION(BlueprintCallable, Category = "SplatCapture|PointCloud")
	void GeneratePointCloud(UBoxComponent* VolumeBox);

	/** Exports as binary PLY file. */
	UFUNCTION(BlueprintCallable, Category = "SplatCapture|PointCloud")
	void ExportToPLY(const FString& SavePath);

	/** Exports as COLMAP points3D.txt. */
	UFUNCTION(BlueprintCallable, Category = "SplatCapture|PointCloud")
	void ExportToPoints3D(const FString& SavePath);

	/** Pushes positions into a Niagara system for visualization. */
	UFUNCTION(BlueprintCallable, Category = "SplatCapture|PointCloud")
	void PushToNiagara(UNiagaraComponent* NiagaraComp);

	UFUNCTION(BlueprintPure, Category = "SplatCapture|PointCloud")
	int32 GetCapturedPointCount() const { return CapturedPositions.Num(); }

private:
	TArray<FVector> CapturedPositions;

	void SampleMeshSurface(UStaticMeshComponent* MeshComp, int32 NumSamples,
		TArray<FVector>& OutPositions);

	float ComputeMeshSurfaceArea(UStaticMeshComponent* MeshComp);

	void SampleByLineTraces(const FVector& BoxMin, const FVector& BoxMax,
		TArray<FVector>& OutPositions);
};

// Copyright (c) 2026, KaepTech. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SplatCaptureFunctionLibrary.generated.h"

class UCineCameraComponent;

USTRUCT(BlueprintType)
struct FSplatCameraFrame
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "SplatCapture")
	FTransform WorldTransform;

	UPROPERTY(BlueprintReadWrite, Category = "SplatCapture")
	FString ImageFilename;

	UPROPERTY(BlueprintReadWrite, Category = "SplatCapture")
	int32 FrameIndex = 0;
};

UCLASS()
class SPLATCAPTURE_API USplatCaptureFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "SplatCapture|Files")
	static bool OpenFolderPicker(FString DialogTitle, FString& OutSelectedDirectory);

	UFUNCTION(BlueprintCallable, Category = "SplatCapture|COLMAP")
	static bool ExportColmapCameras(UCineCameraComponent* CameraComponent, FIntPoint Resolution, FString SavePath);

	UFUNCTION(BlueprintCallable, Category = "SplatCapture|COLMAP")
	static bool ExportColmapImages(const TArray<FSplatCameraFrame>& CapturedFrames, FString SavePath);

	UFUNCTION(BlueprintCallable, Category = "SplatCapture|COLMAP")
	static bool ExportColmapPoints3D(const TArray<FVector>& Positions, FString SavePath);

	UFUNCTION(BlueprintCallable, Category = "SplatCapture|PLY")
	static bool WriteBinaryPLY(const TArray<FVector>& Positions, FString SavePath);

	UFUNCTION(BlueprintPure, Category = "SplatCapture|Utility")
	static FString MakeFrameFilename(FString Prefix, int32 Index, int32 PadWidth, FString Extension);

	static void ConvertUETransformToColmapExtrinsics(
		const FTransform& CameraWorldTransform,
		double& OutQW, double& OutQX, double& OutQY, double& OutQZ,
		double& OutTX, double& OutTY, double& OutTZ);
};
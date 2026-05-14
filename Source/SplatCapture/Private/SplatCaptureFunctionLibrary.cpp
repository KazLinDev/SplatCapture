// Copyright (c) 2026, KaepTech. All rights reserved.

#include "SplatCaptureFunctionLibrary.h"
#include "CineCameraComponent.h"
#include "Developer/DesktopPlatform/Public/IDesktopPlatform.h"
#include "Developer/DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"

// OpenFolderPicker
bool USplatCaptureFunctionLibrary::OpenFolderPicker(FString DialogTitle, FString& OutSelectedDirectory)
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return false;
	void* ParentWindowHandle = nullptr;
	if (FSlateApplication::Get().GetActiveTopLevelWindow().IsValid())
	{
		ParentWindowHandle = FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle();
	}
	return DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, DialogTitle, TEXT(""), OutSelectedDirectory);
}


// ExportColmapCameras
bool USplatCaptureFunctionLibrary::ExportColmapCameras(UCineCameraComponent* CameraComponent, FIntPoint Resolution, FString SavePath)
{
	if (!CameraComponent) return false;
	const double FocalMM = (double)CameraComponent->CurrentFocalLength;
	const double SensorW = (double)CameraComponent->Filmback.SensorWidth;
	const double fx = (FocalMM * (double)Resolution.X) / SensorW;
	const double fy = fx;
	const double cx = (double)Resolution.X / 2.0;
	const double cy = (double)Resolution.Y / 2.0;

	FString Content = TEXT("# CAMERA_ID, MODEL, WIDTH, HEIGHT, PARAMS\n");
	Content += FString::Printf(TEXT("1 PINHOLE %d %d %.8f %.8f %.8f %.8f\n"),
		Resolution.X, Resolution.Y, fx, fy, cx, cy);

	return FFileHelper::SaveStringToFile(Content, *SavePath);
}

// ExportColmapImages
bool USplatCaptureFunctionLibrary::ExportColmapImages(const TArray<FSplatCameraFrame>& CapturedFrames, FString SavePath)
{
	if (CapturedFrames.Num() == 0) return false;

	FString Content = TEXT("# IMAGE_ID, QW, QX, QY, QZ, TX, TY, TZ, CAMERA_ID, NAME\n");

	for (const FSplatCameraFrame& Frame : CapturedFrames)
	{
		double QW, QX, QY, QZ, TX, TY, TZ;
		ConvertUETransformToColmapExtrinsics(Frame.WorldTransform, QW, QX, QY, QZ, TX, TY, TZ);
		Content += FString::Printf(TEXT("%d %.12f %.12f %.12f %.12f %.12f %.12f %.12f %d %s\n\n"),
			Frame.FrameIndex + 1, QW, QX, QY, QZ, TX, TY, TZ, 1, *Frame.ImageFilename);
	}

	return FFileHelper::SaveStringToFile(Content, *SavePath);
}

// ConvertUETransformToColmapExtrinsics
void USplatCaptureFunctionLibrary::ConvertUETransformToColmapExtrinsics(const FTransform& CameraWorldTransform, double& OutQW, double& OutQX, double& OutQY, double& OutQZ, double& OutTX, double& OutTY, double& OutTZ)
{
	const FQuat CamRot = CameraWorldTransform.GetRotation();
	const FVector Fw_ue = CamRot.GetAxisX();
	const FVector Rt_ue = CamRot.GetAxisY();
	const FVector Up_ue = CamRot.GetAxisZ();

	const FVector Fw_col(Fw_ue.Y, -Fw_ue.Z, Fw_ue.X);
	const FVector Rt_col(Rt_ue.Y, -Rt_ue.Z, Rt_ue.X);
	const FVector Up_col(Up_ue.Y, -Up_ue.Z, Up_ue.X);

	FMatrix C2W_Matrix;
	FVector ColmapY = -Up_col;
	C2W_Matrix.SetAxes(&Rt_col, &ColmapY, &Fw_col, nullptr);

	FTransform C2W_Colmap(C2W_Matrix);
	const FVector Pos_ue = CameraWorldTransform.GetTranslation();
	C2W_Colmap.SetTranslation(FVector(Pos_ue.Y, -Pos_ue.Z, Pos_ue.X) * 0.01);

	const FTransform W2C_Colmap = C2W_Colmap.Inverse();
	const FQuat Q_col = W2C_Colmap.GetRotation();
	const FVector T_col = W2C_Colmap.GetTranslation();

	OutQW = Q_col.W; OutQX = Q_col.X; OutQY = Q_col.Y; OutQZ = Q_col.Z;
	OutTX = T_col.X; OutTY = T_col.Y; OutTZ = T_col.Z;
}


// ExportColmap3DPoints
bool USplatCaptureFunctionLibrary::ExportColmapPoints3D(const TArray<FVector>& Positions, FString SavePath)
{
	FString Content = TEXT("# POINT3D_ID, X, Y, Z, R, G, B, ERROR\n");
	for (int32 i = 0; i < Positions.Num(); ++i) {
		Content += FString::Printf(TEXT("%d %.8f %.8f %.8f 255 255 255 0.0\n"), i + 1, Positions[i].Y * 0.01, -Positions[i].Z * 0.01, Positions[i].X * 0.01);
	}
	return FFileHelper::SaveStringToFile(Content, *SavePath);
}


// WriteBinaryPLY
bool USplatCaptureFunctionLibrary::WriteBinaryPLY(const TArray<FVector>& Positions, FString SavePath)
{
	if (Positions.Num() == 0) return false;
	FString Header = FString::Printf(TEXT("ply\nformat binary_little_endian 1.0\nelement vertex %d\nproperty float x\nproperty float y\nproperty float z\nproperty uchar red\nproperty uchar green\nproperty uchar blue\nend_header\n"), Positions.Num());
	FTCHARToUTF8 HeaderUTF8(*Header);
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(HeaderUTF8.Length() + Positions.Num() * 15);
	FMemory::Memcpy(Buffer.GetData(), HeaderUTF8.Get(), HeaderUTF8.Length());
	uint8* Cursor = Buffer.GetData() + HeaderUTF8.Length();
	for (const FVector& P : Positions) {
		float X = (float)(P.Y * 0.01); float Y = (float)(-P.Z * 0.01); float Z = (float)(P.X * 0.01);
		FMemory::Memcpy(Cursor, &X, 4); Cursor += 4; FMemory::Memcpy(Cursor, &Y, 4); Cursor += 4; FMemory::Memcpy(Cursor, &Z, 4); Cursor += 4;
		*Cursor++ = 255; *Cursor++ = 255; *Cursor++ = 255;
	}
	return FFileHelper::SaveArrayToFile(Buffer, *SavePath);
}

// MakeFrameFilename
FString USplatCaptureFunctionLibrary::MakeFrameFilename(FString Prefix, int32 Index, int32 PadWidth, FString Extension)
{
	return FString::Printf(TEXT("%s%0*d.%s"), *Prefix, PadWidth, Index, *Extension);
}

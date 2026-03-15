// Copyright (c) 2025, KazTech. All rights reserverd.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FSplatCaptureStyle
{
public:

	static void Initialize();
	static void Shutdown();
	static void ReloadTextures();
	static const ISlateStyle& Get();
	static FName GetStyleSetName();

private:

	static TSharedRef< class FSlateStyleSet > Create();

private:

	static TSharedPtr< class FSlateStyleSet > StyleInstance;
};
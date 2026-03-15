// Copyright (c) 2025, KazTech. All rights reserverd.

#include "SplatCaptureStyle.h"
#include "SplatCapture.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateStyleRegistry.h"
#include "Slate/SlateGameResources.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleMacros.h"

#define RootToContentDir Style->RootToContentDir

TSharedPtr<FSlateStyleSet> FSplatCaptureStyle::StyleInstance = nullptr;

void FSplatCaptureStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FSplatCaptureStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

FName FSplatCaptureStyle::GetStyleSetName()
{
	static FName StyleSetName(TEXT("SplatCaptureStyle"));
	return StyleSetName;
}


const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);

TSharedRef< FSlateStyleSet > FSplatCaptureStyle::Create()
{
	TSharedRef< FSlateStyleSet > Style = MakeShareable(new FSlateStyleSet("SplatCaptureStyle"));
	Style->SetContentRoot(IPluginManager::Get().FindPlugin("SplatCapture")->GetBaseDir() / TEXT("Resources"));

	Style->Set("SplatCapture.PluginAction", new IMAGE_BRUSH_SVG(TEXT("PlaceholderButtonIcon"), Icon20x20));
	return Style;
}

void FSplatCaptureStyle::ReloadTextures()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}
}

const ISlateStyle& FSplatCaptureStyle::Get()
{
	return *StyleInstance;
}

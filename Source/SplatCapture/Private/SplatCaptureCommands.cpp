// Copyright (c) 2026, KaepTech. All rights reserved.

#include "SplatCaptureCommands.h"

#define LOCTEXT_NAMESPACE "FSplatCaptureModule"

void FSplatCaptureCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "SplatCapture", "Open the SplatCapture widget", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE

// Copyright (c) 2025, KazTech. All rights reserverd.

#include "SplatCaptureCommands.h"

#define LOCTEXT_NAMESPACE "FSplatCaptureModule"

void FSplatCaptureCommands::RegisterCommands()
{
	UI_COMMAND(PluginAction, "SplatCapture", "Execute SplatCapture action", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE







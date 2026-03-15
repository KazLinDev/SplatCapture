// Copyright (c) 2025, KazTech. All rights reserverd.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "SplatCaptureStyle.h"

class FSplatCaptureCommands : public TCommands<FSplatCaptureCommands>
{
public:

	FSplatCaptureCommands()
		: TCommands<FSplatCaptureCommands>(TEXT("SplatCapture"), NSLOCTEXT("Contexts", "SplatCapture", "SplatCapture Plugin"), NAME_None, FSplatCaptureStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > PluginAction;
};

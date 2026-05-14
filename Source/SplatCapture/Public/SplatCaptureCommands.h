// Copyright (c) 2026, KaepTech. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "SplatCaptureStyle.h"

class FSplatCaptureCommands : public TCommands<FSplatCaptureCommands>
{
public:
	FSplatCaptureCommands()
		: TCommands<FSplatCaptureCommands>(
			TEXT("SplatCapture"),
			NSLOCTEXT("Contexts", "SplatCapture", "SplatCapture Plugin"),
			NAME_None,
			FSplatCaptureStyle::GetStyleSetName())
	{
	}

	// TCommands<>

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> PluginAction;
};

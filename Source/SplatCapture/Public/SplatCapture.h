// Copyright (c) 2026, KaepTech. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSplatCaptureModule : public IModuleInterface
{
public:
	// IModuleInterface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	// Bound to the toolbar / menu command.

	void PluginButtonClicked();
	void RegisterMenus();

	TSharedPtr<class FUICommandList> PluginCommands;
};

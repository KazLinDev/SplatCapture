// Copyright (c) 2025, KazTech. All rights reserverd.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FToolBarBuilder;
class FMenuBuilder;

class FSplatCaptureModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** This function will be bound to Command. */
	void PluginButtonClicked();

private:
	void RegisterMenus();
	FName SplatTabID;
	bool bIsWidgetOpen = false;

private:
	TSharedPtr<class FUICommandList> PluginCommands;
};




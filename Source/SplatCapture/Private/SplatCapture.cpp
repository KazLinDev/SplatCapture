// Copyright (c) 2026, KaepTech. All rights reserved.

#include "SplatCapture.h"
#include "SplatCaptureStyle.h"
#include "SplatCaptureCommands.h"
#include "Misc/MessageDialog.h"
#include "Editor/EditorEngine.h"
#include "ToolMenus.h"
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilitySubsystem.h"

#define LOCTEXT_NAMESPACE "FSplatCaptureModule"

void FSplatCaptureModule::StartupModule()
{
	FSplatCaptureStyle::Initialize();
	FSplatCaptureStyle::ReloadTextures();

	FSplatCaptureCommands::Register();

	PluginCommands = MakeShareable(new FUICommandList);
	PluginCommands->MapAction(
		FSplatCaptureCommands::Get().PluginAction,
		FExecuteAction::CreateRaw(this, &FSplatCaptureModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSplatCaptureModule::RegisterMenus));
}

void FSplatCaptureModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);

	FSplatCaptureStyle::Shutdown();
	FSplatCaptureCommands::Unregister();
}

void FSplatCaptureModule::PluginButtonClicked()
{
	UEditorUtilitySubsystem* EUS = GEditor ? GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>() : nullptr;
	if (!EUS) return;

	UEditorUtilityWidgetBlueprint* WidgetBP = LoadObject<UEditorUtilityWidgetBlueprint>(
		nullptr,
		TEXT("/SplatCapture/UI/SplatCapture.SplatCapture"));
	if (!WidgetBP)
	{
		UE_LOG(LogTemp, Warning, TEXT("SplatCapture: Failed to load widget asset."));
		return;
	}

	const FName TabID(*(WidgetBP->GetPathName() + TEXT("_ActiveTab")));

	if (EUS->DoesTabExist(TabID))
	{
		EUS->CloseTabByID(TabID);
		EUS->UnregisterTabByID(TabID);
	}
	else
	{
		EUS->SpawnAndRegisterTab(WidgetBP);
	}
}

void FSplatCaptureModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
		Section.AddMenuEntryWithCommandList(FSplatCaptureCommands::Get().PluginAction, PluginCommands);
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
		FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FSplatCaptureCommands::Get().PluginAction));
		Entry.SetCommandList(PluginCommands);
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSplatCaptureModule, SplatCapture)
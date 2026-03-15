// Copyright (c) 2025, KazTech. All rights reserverd.

#include "SplatCapture.h"
#include "SplatCaptureStyle.h"
#include "SplatCaptureCommands.h"
#include "Misc/MessageDialog.h"
#include "Editor/EditorEngine.h"
#include "ToolMenus.h"

// Newly added
#include "EditorUtilityWidgetBlueprint.h"
#include "EditorUtilitySubsystem.h"

static const FName SplatCaptureTabName("SplatCapture");

#define LOCTEXT_NAMESPACE "FSplatCaptureModule"

void FSplatCaptureModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
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
	// Get the EditorUtilitySubsystem
	UEditorUtilitySubsystem* EditorUtilitySubsystem = GEditor->GetEditorSubsystem<UEditorUtilitySubsystem>();

	// Check if the widget is already open
	if (!EditorUtilitySubsystem->DoesTabExist(SplatTabID))
	{
		// Logic to open the widget

		// Load the EditorUtilityWidgetBlueprint object
		UObject* WidgetObj = LoadObject<UObject>(nullptr, TEXT("/Script/Blutility.EditorUtilityWidgetBlueprint'/SplatCapture/UI/SplatCapture.SplatCapture'"));
		if (!WidgetObj)
		{
			// Handle the case where the object could not be loaded
			UE_LOG(LogTemp, Warning, TEXT("Failed to load the EditorUtilityWidgetBlueprint."));
			return;
		}

		// Cast the loaded object to UEditorUtilityWidgetBlueprint
		UEditorUtilityWidgetBlueprint* SplatCapture = static_cast<UEditorUtilityWidgetBlueprint*>(WidgetObj);

		// Spawn and register the tab
		EditorUtilitySubsystem->SpawnAndRegisterTabAndGetID(SplatCapture, SplatTabID);
		return;
	}

	// Logic to close the widget
	EditorUtilitySubsystem->CloseTabByID(SplatTabID);
	EditorUtilitySubsystem->UnregisterTabByID(SplatTabID);
}


void FSplatCaptureModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FSplatCaptureCommands::Get().PluginAction, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FSplatCaptureCommands::Get().PluginAction));
				Entry.SetCommandList(PluginCommands);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FSplatCaptureModule, SplatCapture)
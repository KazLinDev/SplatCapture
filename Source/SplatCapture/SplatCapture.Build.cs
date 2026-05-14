// Copyright (c) 2026, KaepTech. All rights reserved.

using UnrealBuildTool;
using System.IO;

public class SplatCapture : ModuleRules
{
    public SplatCapture(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "LevelEditor",
                "MovieScene",
                "MovieSceneTools",
                "Sequencer",
                "InputCore",
                "CinematicCamera",
                "Niagara",
                "NiagaraCore",
                "MeshDescription",
                "StaticMeshDescription"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Projects",
                "InputCore",
                "EditorFramework",
                "UnrealEd",
                "ToolMenus",
                "Blutility",
                "UMGEditor",
                "EditorScriptingUtilities"
            }
        );
    }
}
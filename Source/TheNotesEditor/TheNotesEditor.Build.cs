// (c) 2026 Kentron Cowboys. All rights reserved.

using UnrealBuildTool;

public class TheNotesEditor : ModuleRules
{
    public TheNotesEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bWarningsAsErrors = true;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "TheNotes"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "EditorSubsystem",
            "InputCore",
            "LevelEditor",
            "Projects",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "UnrealEd",
            "WorkspaceMenuStructure"
        });
    }
}

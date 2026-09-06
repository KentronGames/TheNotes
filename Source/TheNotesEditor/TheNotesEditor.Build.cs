// (c) 2026 Kentron Cowboys. All rights reserved.

using UnrealBuildTool;

public class TheNotesEditor : ModuleRules
{
    public TheNotesEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Strict only in its home project: a consumer's toolchain or a newer engine must not turn a
        // warning into a hard failure of THEIR build over a plugin they cannot edit.
        bWarningsAsErrors = Target.ProjectFile != null && Target.ProjectFile.GetFileNameWithoutExtension() == "TheGame";

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "TheNotes"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "ContentBrowser",
            "DeveloperSettings",
            "DirectoryWatcher",
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

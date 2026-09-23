// (c) 2026 Kentron Cowboys. All rights reserved.

using EpicGames.Core;
using UnrealBuildTool;

public class TheNotesEditor : ModuleRules
{
    public TheNotesEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Strict only in a project of ours — one that mounts the platform, whatever it is called: a consumer's
        // toolchain or a newer engine must not turn a warning into a hard failure of THEIR build over a plugin
        // they cannot edit.
        bWarningsAsErrors = Target.ProjectFile != null
            && FileReference.Exists(FileReference.Combine(Target.ProjectFile.Directory, ".claude", "scripts", "roots.env"));

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

// (c) 2026 Kentron Cowboys. All rights reserved.

using UnrealBuildTool;

public class TheNotes : ModuleRules
{
    public TheNotes(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bWarningsAsErrors = true;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings"
        });

        // Runtime, not Editor: the same actor class is spawned in a packaged build, and a project
        // that wants notes to take part in its own interaction system subclasses it. Json is the
        // store format and Projects resolves the project directory the store is anchored to.
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Json",
            "Projects"
        });
    }
}

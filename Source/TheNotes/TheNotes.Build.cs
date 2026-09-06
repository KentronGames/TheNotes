// (c) 2026 Kentron Cowboys. All rights reserved.

using UnrealBuildTool;

public class TheNotes : ModuleRules
{
    public TheNotes(ReadOnlyTargetRules Target) : base(Target)
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
            "DeveloperSettings"
        });

        // Runtime, not Editor: the same actor class is spawned in a packaged build, and a project
        // that wants notes to take part in its own interaction system subclasses it. Json is the
        // store format and Projects resolves the project directory the store is anchored to.
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // ImageCore, not Engine: FImageUtils is Engine's, but the FImage it hands back — and the
            // format conversion the icon tint needs before it can touch bytes — live here.
            "ImageCore",
            "Json",
            "Projects"
        });
    }
}

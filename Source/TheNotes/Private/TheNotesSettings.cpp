// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheNotesSettings.h"

#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

#include "TheNote.h"
#include "TheNotesModule.h"

UTheNotesSettings::UTheNotesSettings()
{
    CategoryName = TEXT("Plugins");
    NotesDirectory = DefaultNotesDirectory();
    NoteIconTint = DefaultIconTint();

    // Left empty: unset means the plugin's own mark, read from its Resources folder as a PNG.
    // A project that wants a different one names a texture here and nothing else changes.
}

FString UTheNotesSettings::ResolvedNotesDirectory()
{
    const UTheNotesSettings* Settings = GetDefault<UTheNotesSettings>();
    FString Directory = Settings ? Settings->NotesDirectory : FString();
    Directory.TrimStartAndEndInline();
    if(Directory.IsEmpty())
    {
        Directory = DefaultNotesDirectory();
    }

    // Relative means relative to the project, not to whatever directory the process was started in:
    // the editor, a commandlet and a test runner all have different working directories.
    if(FPaths::IsRelative(Directory))
    {
        Directory = FPaths::ProjectDir() / Directory;
    }
    return FPaths::ConvertRelativePathToFull(Directory);
}

TSubclassOf<ATheNote> UTheNotesSettings::ResolvedNoteActorClass()
{
    const UTheNotesSettings* Settings = GetDefault<UTheNotesSettings>();
    if(Settings && Settings->NoteActorClass.IsValid())
    {
        UClass* Configured = Settings->NoteActorClass.TryLoadClass<ATheNote>();
        if(Configured)
        {
            return Configured;
        }

        // A misconfigured class must not cost the developer his notes: the notes still exist and
        // still need an actor, so fall back rather than spawn nothing and look like data loss.
        UE_LOG(LogTheNotes, Warning, TEXT("NoteActorClass '%s' does not resolve to a TheNote subclass; using ATheNote"), *Settings->NoteActorClass.ToString());
    }
    return ATheNote::StaticClass();
}

UTheNotesUserSettings::UTheNotesUserSettings()
{
    CategoryName = TEXT("Plugins");
}

FString UTheNotesUserSettings::ResolvedAuthorName()
{
    const UTheNotesUserSettings* Settings = GetDefault<UTheNotesUserSettings>();
    FString Author = Settings ? Settings->AuthorName : FString();
    Author.TrimStartAndEndInline();
    if(Author.IsEmpty())
    {
        Author = FPlatformProcess::UserName();
    }

    // The author is also a directory name. A machine account can carry a domain separator, and a
    // configured name can carry anything at all, so it is sanitised here rather than at every use.
    return FPaths::MakeValidFileName(Author, TEXT('_'));
}

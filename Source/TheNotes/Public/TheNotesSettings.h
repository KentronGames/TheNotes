// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TheNotesSettings.generated.h"

/**
 * What the whole team shares about notes: where they are kept, what a note looks like in the world,
 * and which actor class carries one. Lives in DefaultGame.ini because a packaged build reads it too.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "The Notes"))
class THENOTES_API UTheNotesSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UTheNotesSettings();

    static const TCHAR* DefaultNotesDirectory() { return TEXT("DevNotes"); }

    /**
     * The amber the plugin's own mark ships as. It is also the tint's default, which is what makes the
     * default the IDENTITY: the shift below is by the difference from this colour, so leaving the setting
     * alone changes no pixel.
     */
    static FColor DefaultIconTint() { return FColor(253, 151, 31); }

    /**
     * Where note files are kept, relative to the project directory. Deliberately outside Content:
     * a note is text a human reads in a diff, not an asset. Note that files outside Content are not
     * staged into a packaged build by any packaging setting, so a shipped build reads a baked copy.
     */
    UPROPERTY(EditAnywhere, config, Category = "Notes", meta = (RelativeToGameDir))
    FString NotesDirectory;

    /**
     * The sprite a note shows in the world. Faces the camera by virtue of being a billboard.
     * Empty means the plugin's own mark, read from its Resources folder rather than shipped as an
     * asset, so a project gets a note that looks like a note without importing anything.
     */
    UPROPERTY(EditAnywhere, config, Category = "Notes")
    TSoftObjectPtr<UTexture2D> NoteSprite;

    /**
     * What colour the plugin's own mark is drawn in. Shared by the team rather than set per developer:
     * the icon is how everyone recognises a note across a scene, so it belongs with the sprite itself.
     *
     * Applies to the built-in mark only. A project that names its own Note Sprite colours that texture
     * when it makes it, and this setting leaves it alone.
     */
    UPROPERTY(EditAnywhere, config, Category = "Notes")
    FColor NoteIconTint;

    /**
     * The actor class spawned for a note. A project with its own interaction system points this at
     * a subclass carrying its components; the plugin itself never names another module's types.
     */
    UPROPERTY(EditAnywhere, config, Category = "Notes", meta = (MetaClass = "/Script/TheNotes.TheNote"))
    FSoftClassPath NoteActorClass;

    /** The notes directory as a full path. */
    static FString ResolvedNotesDirectory();

    /** The configured note class, or ATheNote when the project has named none. */
    static TSubclassOf<class ATheNote> ResolvedNoteActorClass();
};

/**
 * Who this machine's notes are signed by. Per developer and per project, so it belongs in the user
 * settings ini and not in anything committed — a shared author name would sign everyone's notes
 * with whoever configured the project first.
 */
UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "The Notes (this developer)"))
class THENOTES_API UTheNotesUserSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UTheNotesUserSettings();

    /** The name new notes are signed with. Empty means the account this editor runs under. */
    UPROPERTY(EditAnywhere, config, Category = "Notes")
    FString AuthorName;

    /** The configured author, or the operating system account when none is configured. */
    static FString ResolvedAuthorName();
};

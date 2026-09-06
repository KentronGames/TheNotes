// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Containers/Ticker.h"

#include "TheNoteRecord.h"

#include "TheNotesEditorSubsystem.generated.h"

/**
 * Keeps the notes on disk and the note actors in the open level saying the same thing.
 *
 * The direction is decided per event and never runs both ways at once. Opening a level reads the
 * files and spawns; from then on the actors are the truth and every change flushes back to the
 * files. Nothing here ever writes into the map: the actors are spawned transient, which is the one
 * guarantee that keeps a developer's notes out of the level designer's files.
 *
 * The set of notes is always re-derived from the world rather than remembered. Undo and redo move
 * actors in and out behind our back, and a remembered list would have to be repaired after each
 * one; a list read from the world is correct by construction.
 */
UCLASS()
class THENOTESEDITOR_API UTheNotesEditorSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Writes a new note at a world position, spawns its actor and selects it. */
    class ATheNote* CreateNoteAt(const FVector& Location);

    /** Selects the note with this identity and moves the viewport cameras to it. */
    bool FocusOnNote(const FGuid& Id);

    /**
     * Removes the note with this identity from the open level, and from the file at the next flush.
     *
     * Deleting IS how a note is closed: the store is committed text, so the thought stays in history
     * while the level stops carrying a marker nobody needs any more. Answers false for a note that
     * belongs to a level that is not open — there is no actor to remove.
     */
    bool DeleteNote(const FGuid& Id);

    /** Every note the browser lists: the open level from its actors, every other level from disk. */
    TArray<FTheNoteRecord> CollectAllNotes() const;

    /** The note actors standing in the open level. */
    TArray<class ATheNote*> GetLevelNotes() const;

    /** The long package name of the level whose notes are loaded, or empty when none is. */
    const FString& GetTrackedLevel() const { return TrackedLevel; }

    /** Writes every pending change to disk immediately. */
    void Flush();

    /**
     * Throws away the spawned notes and reads the open level's files again.
     *
     * The one thing that makes notes arriving from source control visible without reopening the
     * map. Pending local changes are written first, so this never costs unsaved work.
     */
    void Reload();

    /** Raised after the store or the set of note actors changes, so the browser can refresh. */
    FSimpleMulticastDelegate& OnNotesChanged() { return NotesChanged; }

private:
    void HandleMapOpened(const FString& Filename, bool bAsTemplate);
    void HandleActorMoved(AActor* Actor);
    void HandleActorDeleted(AActor* Actor);
    void HandleObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& Event);
    void HandleUndoRedo();
    void HandleStoreDirectoryChanged(const TArray<struct FFileChangeData>& Changes);
    bool HandleTick(float DeltaTime);

    /**
     * Registers the file watch, or re-registers it when the configured directory has changed.
     *
     * Silently does nothing while the directory does not exist: the platform watcher opens an
     * existing handle and cannot wait for one to appear, and the ticker calls this again — so a
     * project whose first note has not been written yet starts watching the moment it is.
     */
    void StartWatchingStore();
    void StopWatchingStore();

    class UWorld* EditorWorld() const;
    static FString LevelPackageNameOf(const class UWorld* World);

    void ReloadFromStore();
    void DespawnAll();
    class ATheNote* SpawnFor(const FTheNoteRecord& Record);
    void MarkDirty();

    /** Long package name of the level the spawned actors belong to; empty when there is none. */
    FString TrackedLevel;

    /**
     * Every author with notes in this level, from the moment it was opened. An author whose last
     * note has just been deleted is still in here, because his file has to be written to say so.
     */
    TSet<FString> KnownAuthors;

    /** True while this subsystem is the one changing actors, so its own work does not look dirty. */
    bool bApplyingStore = false;

    bool bDirty = false;

    /** The directory the watch is registered on. Empty means nothing is being watched. */
    FString WatchedDirectory;

    /**
     * Raised when the spawned actors have to be built again, acted on by the ticker so a burst costs one
     * reload. Two causes: the files changed under us, or a setting that decides what a note looks like
     * did — the mark's colour lives in its pixels, so the standing actors hold the old texture.
     */
    bool bReloadRequested = false;

    /**
     * When this subsystem last wrote the store, on the platform clock.
     *
     * Our own writes come back through the watcher like anyone else's, and a reload triggered by
     * them would destroy and respawn the very actors the developer is dragging. Anything arriving
     * within the grace window after a write of ours is therefore taken to be that write.
     */
    double LastSelfWriteSeconds = 0.0;

    FSimpleMulticastDelegate NotesChanged;

    FDelegateHandle MapOpenedHandle;
    FDelegateHandle ActorMovedHandle;
    FDelegateHandle ActorDeletedHandle;
    FDelegateHandle PropertyChangedHandle;
    FDelegateHandle UndoRedoHandle;
    FDelegateHandle StoreWatcherHandle;
    FTSTicker::FDelegateHandle TickerHandle;
};

// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheNotesEditorSubsystem.h"

#include "ContentBrowserModule.h"
#include "DirectoryWatcherModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "IContentBrowserSingleton.h"
#include "IDirectoryWatcher.h"
#include "ScopedTransaction.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Engine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include "TheNote.h"
#include "TheNoteStore.h"
#include "TheNotesModule.h"
#include "TheNotesSettings.h"

namespace TheNotesEditorSubsystemLocal
{
// Long enough that dragging a note across the viewport is one write rather than one per frame,
// short enough that a developer who alt-tabs to look at the file finds it already correct.
static constexpr float FlushDelaySeconds = 0.75f;

// How long after a write of our own a file event is still attributed to that write. Wider than one
// flush interval on purpose: the watcher reports a batch some time after the bytes land, and
// mistaking our own write for someone else's costs a despawn-respawn under the developer's cursor.
static constexpr double SelfWriteGraceSeconds = 2.0;
}

void UTheNotesEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    MapOpenedHandle = FEditorDelegates::OnMapOpened.AddUObject(this, &UTheNotesEditorSubsystem::HandleMapOpened);
    UndoRedoHandle = FEditorDelegates::PostUndoRedo.AddUObject(this, &UTheNotesEditorSubsystem::HandleUndoRedo);
    PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UTheNotesEditorSubsystem::HandleObjectPropertyChanged);

    if(GEngine)
    {
        ActorMovedHandle = GEngine->OnActorMoved().AddUObject(this, &UTheNotesEditorSubsystem::HandleActorMoved);
        ActorDeletedHandle = GEngine->OnLevelActorDeleted().AddUObject(this, &UTheNotesEditorSubsystem::HandleActorDeleted);
    }

    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UTheNotesEditorSubsystem::HandleTick), TheNotesEditorSubsystemLocal::FlushDelaySeconds);

    StartWatchingStore();
}

void UTheNotesEditorSubsystem::Deinitialize()
{
    // The last write happens here rather than being left to the ticker: an editor closing does not
    // tick again, and the alternative is losing whatever was changed in the last second of a session.
    Flush();

    StopWatchingStore();
    FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
    FEditorDelegates::OnMapOpened.Remove(MapOpenedHandle);
    FEditorDelegates::PostUndoRedo.Remove(UndoRedoHandle);
    FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
    if(GEngine)
    {
        GEngine->OnActorMoved().Remove(ActorMovedHandle);
        GEngine->OnLevelActorDeleted().Remove(ActorDeletedHandle);
    }

    Super::Deinitialize();
}

UWorld* UTheNotesEditorSubsystem::EditorWorld() const
{
    return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

FString UTheNotesEditorSubsystem::LevelPackageNameOf(const UWorld* World)
{
    const UPackage* Package = World ? World->GetPackage() : nullptr;
    return Package ? Package->GetName() : FString();
}

TArray<ATheNote*> UTheNotesEditorSubsystem::GetLevelNotes() const
{
    TArray<ATheNote*> Notes;
    UWorld* World = EditorWorld();
    if(!World)
    {
        return Notes;
    }

    for(TActorIterator<ATheNote> It(World); It; ++It)
    {
        if(IsValid(*It))
        {
            Notes.Add(*It);
        }
    }
    return Notes;
}

void UTheNotesEditorSubsystem::HandleMapOpened(const FString& Filename, bool bAsTemplate)
{
    ReloadFromStore();
}

void UTheNotesEditorSubsystem::HandleActorMoved(AActor* Actor)
{
    if(!bApplyingStore && Cast<ATheNote>(Actor))
    {
        MarkDirty();
    }
}

void UTheNotesEditorSubsystem::HandleActorDeleted(AActor* Actor)
{
    if(!bApplyingStore && Cast<ATheNote>(Actor))
    {
        MarkDirty();
    }
}

void UTheNotesEditorSubsystem::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
    if(!bApplyingStore && Cast<ATheNote>(Object))
    {
        MarkDirty();
        return;
    }

    // A note is built from the settings at spawn — its sprite most of all, whose colour is baked into the
    // pixels. Standing actors would otherwise keep the old look until the map was opened again, which
    // reads as a setting that does nothing.
    if(Cast<UTheNotesSettings>(Object))
    {
        bReloadRequested = true;
    }
}

void UTheNotesEditorSubsystem::HandleUndoRedo()
{
    // Undo can have restored a deleted note or removed a created one, and neither raises the actor
    // delegates. The set is read from the world at flush time, so saying "dirty" is the whole fix.
    MarkDirty();
}

bool UTheNotesEditorSubsystem::HandleTick(float DeltaTime)
{
    // Also the retry: the directory may not have existed at startup, and the setting naming it can
    // change while the editor runs.
    StartWatchingStore();

    const FString CurrentLevel = LevelPackageNameOf(EditorWorld());

    // The editor already has a map open when this subsystem is created, and that map arrives
    // through no delegate at all. The first tick that sees a world is the load for it.
    if(!CurrentLevel.IsEmpty() && CurrentLevel != TrackedLevel)
    {
        ReloadFromStore();
        return true;
    }

    if(bDirty)
    {
        // Local changes go out first and the reload waits for the next tick: reloading over unsaved
        // work would spawn the files' version of notes the developer has just moved.
        Flush();
        return true;
    }

    if(bReloadRequested)
    {
        bReloadRequested = false;
        ReloadFromStore();
    }
    return true;
}

void UTheNotesEditorSubsystem::HandleStoreDirectoryChanged(const TArray<FFileChangeData>& Changes)
{
    if(FPlatformTime::Seconds() - LastSelfWriteSeconds < TheNotesEditorSubsystemLocal::SelfWriteGraceSeconds)
    {
        return;
    }

    bReloadRequested = true;
}

void UTheNotesEditorSubsystem::StartWatchingStore()
{
    const FString Directory = UTheNotesSettings::ResolvedNotesDirectory();
    if(Directory == WatchedDirectory)
    {
        return;
    }

    StopWatchingStore();

    if(!IFileManager::Get().DirectoryExists(*Directory))
    {
        return;
    }

    FDirectoryWatcherModule& Module = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
    IDirectoryWatcher* Watcher = Module.Get();
    if(!Watcher)
    {
        return;
    }

    // Default flags: files only, subdirectories included — which is the store's own shape, one
    // directory per author under the root.
    const bool bRegistered = Watcher->RegisterDirectoryChangedCallback_Handle(Directory, IDirectoryWatcher::FDirectoryChanged::CreateUObject(this, &UTheNotesEditorSubsystem::HandleStoreDirectoryChanged), StoreWatcherHandle);

    if(!bRegistered)
    {
        UE_LOG(LogTheNotes, Warning, TEXT("Cannot watch '%s' for changes; notes arriving from source control will need TheNotes.Reload"), *Directory);
        return;
    }

    WatchedDirectory = Directory;
}

void UTheNotesEditorSubsystem::StopWatchingStore()
{
    if(WatchedDirectory.IsEmpty())
    {
        return;
    }

    FDirectoryWatcherModule* Module = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
    IDirectoryWatcher* Watcher = Module ? Module->Get() : nullptr;
    if(Watcher)
    {
        Watcher->UnregisterDirectoryChangedCallback_Handle(WatchedDirectory, StoreWatcherHandle);
    }

    WatchedDirectory.Reset();
    StoreWatcherHandle.Reset();
}

void UTheNotesEditorSubsystem::Reload()
{
    ReloadFromStore();
}

void UTheNotesEditorSubsystem::MarkDirty()
{
    bDirty = true;
}

void UTheNotesEditorSubsystem::DespawnAll()
{
    UWorld* World = EditorWorld();
    if(!World)
    {
        return;
    }

    TGuardValue<bool> Applying(bApplyingStore, true);
    for(ATheNote* Note : GetLevelNotes())
    {
        World->DestroyActor(Note);
    }
}

ATheNote* UTheNotesEditorSubsystem::SpawnFor(const FTheNoteRecord& Record)
{
    UWorld* World = EditorWorld();
    if(!World)
    {
        return nullptr;
    }

    FActorSpawnParameters Parameters;

    // RF_Transient is what keeps the note out of the .umap and out of World Partition's external
    // actors; RF_Transactional is what keeps Ctrl+Z working on it. The engine's default is the
    // second alone, so writing the first without the second silently costs undo.
    Parameters.ObjectFlags = RF_Transient | RF_Transactional;
    Parameters.bCreateActorPackage = false;
    Parameters.bNoFail = true;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    const TSubclassOf<ATheNote> NoteClass = UTheNotesSettings::ResolvedNoteActorClass();
    ATheNote* Note = World->SpawnActor<ATheNote>(NoteClass, FTransform(Record.Location), Parameters);
    if(!Note)
    {
        return nullptr;
    }

#if WITH_EDITOR
    // A note is a landmark: it has to be visible from wherever the developer is looking, not only
    // from inside the World Partition cell it happens to stand in.
    Note->SetIsSpatiallyLoaded(false);
#endif

    Note->ApplyRecord(Record);
    return Note;
}

void UTheNotesEditorSubsystem::ReloadFromStore()
{
    UWorld* World = EditorWorld();
    const FString LevelName = LevelPackageNameOf(World);

    // Anything pending belongs to the level being left, and the tracked level is what says where to
    // write it. Flushing after the switch would file the old level's notes under the new one.
    if(bDirty && !TrackedLevel.IsEmpty())
    {
        Flush();
    }

    DespawnAll();
    TrackedLevel = LevelName;
    KnownAuthors.Reset();
    bDirty = false;

    if(TrackedLevel.IsEmpty())
    {
        NotesChanged.Broadcast();
        return;
    }

    TGuardValue<bool> Applying(bApplyingStore, true);
    const TArray<FTheNoteRecord> Records = FTheNoteStore::LoadLevel(TrackedLevel);
    for(const FTheNoteRecord& Record : Records)
    {
        KnownAuthors.Add(Record.Author);
        SpawnFor(Record);
    }

    UE_LOG(LogTheNotes, Log, TEXT("%s: %d note(s) from %d author(s)"), *TrackedLevel, Records.Num(), KnownAuthors.Num());
    NotesChanged.Broadcast();
}

void UTheNotesEditorSubsystem::Flush()
{
    // Unconditional on purpose: a method called Flush that quietly does nothing is how a change
    // reaches disk only sometimes. The dirty flag is the ticker's business — it decides when to
    // call this — and nothing that happens below writes a byte that was not going to change anyway.
    bDirty = false;
    if(TrackedLevel.IsEmpty())
    {
        return;
    }

    TMap<FString, TArray<FTheNoteRecord>> ByAuthor;
    TSet<FGuid> Seen;

    for(ATheNote* Note : GetLevelNotes())
    {
        // Ctrl+W copies a note's identity along with everything else, and two notes sharing one id
        // are one note as far as the store is concerned: the second silently replaces the first on
        // the next read. Re-signing here is what turns a copy into a note of its own. The new
        // identity is written back onto the actor, because the actor is what the next flush reads.
        if(!Note->Record.Id.IsValid() || Seen.Contains(Note->Record.Id))
        {
            Note->Record.Id = FGuid::NewGuid();
            Note->Record.CreatedAt = FDateTime::UtcNow();
            Note->Record.UpdatedAt = Note->Record.CreatedAt;
        }
        if(Note->Record.Author.IsEmpty())
        {
            Note->Record.Author = UTheNotesUserSettings::ResolvedAuthorName();
        }
        Note->Record.Level = TrackedLevel;
        Seen.Add(Note->Record.Id);

        FTheNoteRecord Record = Note->ToRecord();
        KnownAuthors.Add(Record.Author);
        ByAuthor.FindOrAdd(Record.Author).Add(MoveTemp(Record));
    }

    for(const FString& Author : KnownAuthors)
    {
        const FString Path = FTheNoteStore::FilePath(Author, TrackedLevel);
        TArray<FTheNoteRecord>* Records = ByAuthor.Find(Author);
        FString Error;

        if(Records)
        {
            // What the file already says decides which notes are actually new or changed. Reading
            // it back is what keeps UpdatedAt still on the notes nobody touched, and a save that
            // changes nothing therefore writes the same bytes and shows up in no diff.
            TArray<FTheNoteRecord> Previous;
            FString ReadError;
            FTheNoteStore::LoadFile(Path, Previous, ReadError);

            for(FTheNoteRecord& Candidate : *Records)
            {
                const FTheNoteRecord* Before = Previous.FindByPredicate([&Candidate](const FTheNoteRecord& Existing) { return Existing.Id == Candidate.Id; });

                // A note the file has never seen keeps the stamps it was created with, so a fresh
                // note reads "written and last changed at the same moment" rather than showing an
                // edit it never had.
                if(!Before)
                {
                    continue;
                }

                if(!Before->EqualsIgnoringStamps(Candidate))
                {
                    Candidate.CreatedAt = Before->CreatedAt;
                    Candidate.UpdatedAt = FDateTime::UtcNow();
                }
                else
                {
                    Candidate.CreatedAt = Before->CreatedAt;
                    Candidate.UpdatedAt = Before->UpdatedAt;
                }
            }
        }

        // An author with nothing left in this level loses his file rather than keeping an empty
        // one: an empty file is a diff nobody asked for and a directory that never gets tidied.
        const bool bWritten = (Records && Records->Num() > 0) ? FTheNoteStore::SaveFile(Path, Author, TrackedLevel, *Records, Error) : FTheNoteStore::DeleteFile(Path, Error);

        if(!bWritten)
        {
            UE_LOG(LogTheNotes, Error, TEXT("%s"), *Error);
        }
    }

    LastSelfWriteSeconds = FPlatformTime::Seconds();
    NotesChanged.Broadcast();
}

ATheNote* UTheNotesEditorSubsystem::CreateNoteAt(const FVector& Location)
{
    UWorld* World = EditorWorld();
    if(!World)
    {
        return nullptr;
    }

    if(TrackedLevel.IsEmpty())
    {
        TrackedLevel = LevelPackageNameOf(World);
    }
    if(TrackedLevel.IsEmpty())
    {
        UE_LOG(LogTheNotes, Warning, TEXT("Cannot create a note: no level is open"));
        return nullptr;
    }

    const FScopedTransaction Transaction(NSLOCTEXT("TheNotes", "CreateNote", "Create DEV Note"));

    // What the note is ABOUT, when the answer is on screen and selected. A coordinate says where a
    // problem is and not what it is attached to, and the actor's own label is the cheapest way to carry
    // that: it is a title the author can then rewrite, not a reference that can dangle.
    FString Subject;
    if(GEditor)
    {
        TArray<AActor*> Selected;
        GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(Selected);
        if(Selected.Num() == 1 && !Selected[0]->IsA<ATheNote>())
        {
            Subject = Selected[0]->GetActorLabel();
        }
    }

    ATheNote* Note = nullptr;
    {
        TGuardValue<bool> Applying(bApplyingStore, true);
        FTheNoteRecord Record = FTheNoteStore::MakeRecord(TrackedLevel, Location);
        Record.Title = Subject;
        Note = SpawnFor(Record);
    }
    if(!Note)
    {
        return nullptr;
    }

    KnownAuthors.Add(Note->Record.Author);

    if(GEditor)
    {
        GEditor->SelectNone(false, true, false);
        GEditor->SelectActor(Note, true, true);
    }

    MarkDirty();
    return Note;
}

void UTheNotesEditorSubsystem::CommentOnAssets(const TArray<FString>& AssetPackageNames, const FString& Text)
{
    if(AssetPackageNames.Num() == 0 || Text.IsEmpty())
    {
        return;
    }

    const FString Author = UTheNotesUserSettings::ResolvedAuthorName();
    TArray<FTheNoteRecord> Records = FTheNoteStore::LoadAssetComments(Author);

    for(const FString& AssetPackageName : AssetPackageNames)
    {
        FTheNoteRecord Record = FTheNoteStore::MakeAssetRecord(AssetPackageName);

        // The asset's own name is the title, so the list reads as a list of assets rather than of
        // first lines. The comment is one field to type, and asking for a title as well is asking for
        // the asset's name a second time.
        Record.Title = FPackageName::GetShortName(AssetPackageName);
        Record.Body = Text;
        Records.Add(MoveTemp(Record));
    }

    FString Error;
    if(!FTheNoteStore::SaveAssetComments(Author, Records, Error))
    {
        UE_LOG(LogTheNotes, Error, TEXT("%s"), *Error);
        return;
    }

    // Our own write comes back through the directory watcher like anyone else's, and a reload it
    // triggers would despawn and respawn every note standing in the open level for nothing.
    LastSelfWriteSeconds = FPlatformTime::Seconds();
    UE_LOG(LogTheNotes, Log, TEXT("%d asset comment(s) written to %s"), AssetPackageNames.Num(), *FTheNoteStore::AssetsFilePath(Author));
    NotesChanged.Broadcast();
}

bool UTheNotesEditorSubsystem::FocusOnNote(const FGuid& Id)
{
    for(ATheNote* Note : GetLevelNotes())
    {
        if(Note->Record.Id == Id)
        {
            if(GEditor)
            {
                GEditor->SelectNone(false, true, false);
                GEditor->SelectActor(Note, true, true);
                GEditor->MoveViewportCamerasToActor(*Note, false);
            }
            return true;
        }
    }
    return FocusOnAsset(Id);
}

bool UTheNotesEditorSubsystem::FocusOnAsset(const FGuid& Id)
{
    for(const FString& File : FTheNoteStore::AssetFiles())
    {
        TArray<FTheNoteRecord> Records;
        FString Error;
        if(!FTheNoteStore::LoadFile(File, Records, Error))
        {
            UE_LOG(LogTheNotes, Warning, TEXT("%s"), *Error);
            continue;
        }

        const FTheNoteRecord* Found = Records.FindByPredicate([&Id](const FTheNoteRecord& Record) { return Record.Id == Id; });
        if(!Found || Found->Asset.IsEmpty())
        {
            continue;
        }

        // By package name rather than by object path: the record stores the package, and building an
        // object path out of it would guess that the asset inside is named after its package — true
        // for everything the content browser makes and not a rule the engine keeps.
        TArray<FAssetData> Assets;
        IAssetRegistry::GetChecked().GetAssetsByPackageName(FName(*Found->Asset), Assets);
        if(Assets.Num() == 0)
        {
            UE_LOG(LogTheNotes, Warning, TEXT("The asset '%s' this comment is about no longer exists"), *Found->Asset);
            return false;
        }

        FContentBrowserModule& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
        ContentBrowser.Get().SyncBrowserToAssets(Assets);
        return true;
    }
    return false;
}

bool UTheNotesEditorSubsystem::DeleteNote(const FGuid& Id)
{
    // No world is not a refusal any more: an asset comment lives in a file and is deleted whether or
    // not a level happens to be open.
    UWorld* World = EditorWorld();
    const TArray<ATheNote*> Notes = World ? GetLevelNotes() : TArray<ATheNote*>();

    for(ATheNote* Note : Notes)
    {
        if(Note->Record.Id != Id)
        {
            continue;
        }

        const FScopedTransaction Transaction(NSLOCTEXT("TheNotes", "DeleteNote", "Delete DEV Note"));

        // Not guarded by bApplyingStore: this IS a change the store has to hear about, and the author
        // whose last note this was stays in KnownAuthors so the flush writes his file empty and removes it.
        World->DestroyActor(Note);
        MarkDirty();
        return true;
    }
    return DeleteAssetComment(Id);
}

bool UTheNotesEditorSubsystem::DeleteAssetComment(const FGuid& Id)
{
    for(const FString& File : FTheNoteStore::AssetFiles())
    {
        TArray<FTheNoteRecord> Records;
        FString Error;
        if(!FTheNoteStore::LoadFile(File, Records, Error))
        {
            UE_LOG(LogTheNotes, Warning, TEXT("%s"), *Error);
            continue;
        }

        const int32 Index = Records.IndexOfByPredicate([&Id](const FTheNoteRecord& Record) { return Record.Id == Id; });
        if(Index == INDEX_NONE)
        {
            continue;
        }

        // Taken before the removal: the author is what says which file to write back, and the last
        // comment in a file takes the only copy of it with him.
        const FString Author = Records[Index].Author;
        Records.RemoveAt(Index);

        if(!FTheNoteStore::SaveAssetComments(Author, Records, Error))
        {
            UE_LOG(LogTheNotes, Error, TEXT("%s"), *Error);
            return false;
        }

        LastSelfWriteSeconds = FPlatformTime::Seconds();
        NotesChanged.Broadcast();
        return true;
    }
    return false;
}

TArray<FTheNoteRecord> UTheNotesEditorSubsystem::CollectAllNotes() const
{
    TArray<FTheNoteRecord> All;

    // The open level is read from its actors, not from its files: a note moved a second ago is on
    // screen and has to be in the list, and its file is up to three quarters of a second behind.
    for(const ATheNote* Note : GetLevelNotes())
    {
        FTheNoteRecord Record = Note->ToRecord();
        Record.Level = TrackedLevel;
        All.Add(MoveTemp(Record));
    }

    // The open level's own notes are the ones already taken from actors above. An asset comment is
    // never one of them — it has no level and no actor — so the test asks about the level note, not
    // about a level being empty: with no map open, an empty tracked level would otherwise swallow
    // every asset comment there is.
    for(const FTheNoteRecord& Record : FTheNoteStore::LoadAll())
    {
        if(!Record.Asset.IsEmpty() || Record.Level != TrackedLevel)
        {
            All.Add(Record);
        }
    }

    All.Sort(
        [](const FTheNoteRecord& A, const FTheNoteRecord& B)
        {
            if(A.Author != B.Author)
            {
                return A.Author < B.Author;
            }
            if(A.Collection != B.Collection)
            {
                return A.Collection < B.Collection;
            }
            return A.Title < B.Title;
        });
    return All;
}

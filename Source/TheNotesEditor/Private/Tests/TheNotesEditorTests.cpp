// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"

#include "TheNote.h"
#include "TheNoteStore.h"
#include "TheNotesEditorSubsystem.h"
#include "TheNotesSettings.h"
#include "TheNotesViewportLabels.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheNotesLeaveTheMapAloneTest, "TheNotes.Editor.NotesNeverDirtyTheMap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheNotesLeaveTheMapAloneTest::RunTest(const FString& Parameters)
{
    UTheNotesEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UTheNotesEditorSubsystem>() : nullptr;
    if(!TestNotNull(TEXT("the notes subsystem exists"), Subsystem))
    {
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* Package = World ? World->GetPackage() : nullptr;
    if(!TestNotNull(TEXT("a level is open"), Package))
    {
        return false;
    }

    const FString Level = Subsystem->GetTrackedLevel();
    if(!TestFalse(TEXT("the open level is tracked"), Level.IsEmpty()))
    {
        return false;
    }

    // The whole point of this test is the delta, so the starting state has to be clean. Something
    // else having dirtied the map before us would otherwise read as our doing.
    Package->SetDirtyFlag(false);

    ATheNote* Note = Subsystem->CreateNoteAt(FVector(1234.0, 5678.0, 90.0));
    ON_SCOPE_EXIT
    {
        if(IsValid(Note) && World)
        {
            World->DestroyActor(Note);
            Subsystem->Flush();
        }
    };

    if(!TestNotNull(TEXT("a note is created"), Note))
    {
        return false;
    }

    // Three independent ways for a note to end up in somebody's commit, and all three have to be
    // shut: living in the map package, living in its own external actor package, or simply being
    // saveable at all.
    TestTrue(TEXT("the note actor is transient"), Note->HasAnyFlags(RF_Transient));
    TestNull(TEXT("the note has no external package"), Note->GetExternalPackage());
    TestFalse(TEXT("creating a note does not dirty the map"), Package->IsDirty());

    Subsystem->Flush();
    const FString File = FTheNoteStore::FilePath(Note->Record.Author, Level);
    TestTrue(TEXT("the note reached its file"), IFileManager::Get().FileExists(*File));

    FString Untouched;
    FFileHelper::LoadFileToString(Untouched, *File);
    Subsystem->Flush();
    FString AfterIdleFlush;
    FFileHelper::LoadFileToString(AfterIdleFlush, *File);

    // A save that changes nothing must write the same bytes. Stamping every note on every flush
    // turns one dragged marker into a diff across the whole file, and the store stops being
    // reviewable — which is the entire reason the notes are text and not an asset.
    TestEqual(TEXT("a flush with nothing to say changes no bytes"), AfterIdleFlush, Untouched);

    // PostEditMove is what the editor itself calls after a gizmo drag or a transform typed into
    // the details panel, and it is what raises the actor-moved delegate. Moving the actor without
    // it would test a path no developer can take.
    Note->SetActorLocation(FVector(4321.0, 8765.0, 9.0));
    Note->PostEditMove(true);
    Subsystem->Flush();

    TArray<FTheNoteRecord> Written;
    FString Error;
    if(TestTrue(FString::Printf(TEXT("the file reads back: %s"), *Error), FTheNoteStore::LoadFile(File, Written, Error)))
    {
        const FTheNoteRecord* Moved = Written.FindByPredicate([Note](const FTheNoteRecord& Candidate)
        {
            return Candidate.Id == Note->Record.Id;
        });

        if(TestNotNull(TEXT("the moved note is in the file"), Moved))
        {
            TestTrue(TEXT("the move was written"), Moved->Location.Equals(FVector(4321.0, 8765.0, 9.0), 0.01));
        }
    }

    TestFalse(TEXT("moving a note does not dirty the map"), Package->IsDirty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheNotesDuplicateIdentityTest, "TheNotes.Editor.ACopiedNoteBecomesItsOwn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheNotesDuplicateIdentityTest::RunTest(const FString& Parameters)
{
    UTheNotesEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UTheNotesEditorSubsystem>() : nullptr;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if(!TestNotNull(TEXT("the notes subsystem exists"), Subsystem) || !TestNotNull(TEXT("a level is open"), World))
    {
        return false;
    }

    ATheNote* First = Subsystem->CreateNoteAt(FVector(100.0, 0.0, 0.0));
    ATheNote* Second = Subsystem->CreateNoteAt(FVector(200.0, 0.0, 0.0));
    ON_SCOPE_EXIT
    {
        if(IsValid(First))
        {
            World->DestroyActor(First);
        }
        if(IsValid(Second))
        {
            World->DestroyActor(Second);
        }
        Subsystem->Flush();
    };

    if(!TestNotNull(TEXT("the first note exists"), First) || !TestNotNull(TEXT("the second note exists"), Second))
    {
        return false;
    }

    First->Record.Title = TEXT("Original");
    Second->Record.Title = TEXT("Copy");

    // What Ctrl+W leaves behind: a second actor carrying the first one's identity. Nothing in the
    // editor prevents it, and without a fix one of the two notes never reaches the file.
    Second->Record.Id = First->Record.Id;
    Subsystem->Flush();

    TestTrue(TEXT("the copy was given an identity of its own"), Second->Record.Id != First->Record.Id);

    const FString File = FTheNoteStore::FilePath(First->Record.Author, Subsystem->GetTrackedLevel());
    TArray<FTheNoteRecord> Written;
    FString Error;
    if(!TestTrue(FString::Printf(TEXT("the file reads back: %s"), *Error), FTheNoteStore::LoadFile(File, Written, Error)))
    {
        return false;
    }

    const bool bHasOriginal = Written.ContainsByPredicate([](const FTheNoteRecord& R) { return R.Title == TEXT("Original"); });
    const bool bHasCopy = Written.ContainsByPredicate([](const FTheNoteRecord& R) { return R.Title == TEXT("Copy"); });
    TestTrue(TEXT("both notes survived the save"), bHasOriginal && bHasCopy);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheNotesPanelMeasureTest, "TheNotes.Editor.PanelFitsItsText", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheNotesPanelMeasureTest::RunTest(const FString& Parameters)
{
    // -nullrhi cannot draw, but it can shape text, and the panel size is laid out by the same code
    // that draws it. A screen-filling black rectangle shipped because the two disagreed, and this
    // is the half of that failure a headless run can still catch.
    const TArray<FString> Body = { TEXT("Bla-bla-bla-bla"), TEXT("Bla-bla-bla-bla") };
    const FVector2D Size = FTheNotesViewportLabels::MeasurePanel(TEXT("Note title"), Body, TEXT("JefeKabanizzer"));

    if(!TestTrue(TEXT("the panel has a size at all"), Size.X > 0.0 && Size.Y > 0.0))
    {
        return false;
    }

    // Generous bounds on purpose: this is not pinning a layout, it is catching a panel measured in
    // the wrong units. The one that shipped was over five hundred by five hundred for this content.
    TestTrue(FString::Printf(TEXT("width is sane, got %.0f"), Size.X), Size.X > 60.0 && Size.X < 400.0);
    TestTrue(FString::Printf(TEXT("height is sane, got %.0f"), Size.Y), Size.Y > 60.0 && Size.Y < 260.0);

    const FVector2D Empty = FTheNotesViewportLabels::MeasurePanel(FString(), TArray<FString>(), FString());
    TestTrue(TEXT("nothing to say draws no box"), Empty.IsZero());

    const FVector2D Longer = FTheNotesViewportLabels::MeasurePanel(TEXT("Note title"), { TEXT("Bla"), TEXT("Bla"), TEXT("Bla"), TEXT("Bla") }, TEXT("JefeKabanizzer"));
    TestTrue(TEXT("more lines make a taller panel"), Longer.Y > Size.Y);
    return true;
}

#endif

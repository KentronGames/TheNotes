// (c) 2026 Kentron Cowboys. All rights reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TheNoteStore.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

namespace TheNoteStoreTests
{
FString ScratchDirectory()
{
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("TheNotesTests") / FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

FTheNoteRecord MakeOne(const FString& Author, const FString& Title, const FVector& Location)
{
    FTheNoteRecord Record;
    Record.Id = FGuid::NewGuid();
    Record.Author = Author;
    Record.Level = TEXT("/Game/Maps/L_Main");
    Record.Title = Title;
    Record.Body = TEXT("Line one\nLine two");
    Record.Location = Location;
    Record.CreatedAt = FDateTime(2026, 9, 4, 12, 0, 0);
    Record.UpdatedAt = Record.CreatedAt;
    return Record;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheNoteStoreRoundTripTest, "TheNotes.Store.RoundTripKeepsEveryField", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheNoteStoreRoundTripTest::RunTest(const FString& Parameters)
{
    using namespace TheNoteStoreTests;

    const FString Directory = ScratchDirectory();
    const FString File = Directory / TEXT("Game.Maps.L_Main.json");
    ON_SCOPE_EXIT
    {
        IFileManager::Get().DeleteDirectory(*Directory, false, true);
    };

    FTheNoteRecord Written = MakeOne(TEXT("Author"), TEXT("A title"), FVector(10.0, -20.5, 30.25));
    Written.Collection = TEXT("Blockers");
    Written.bShowInGame = true;

    FString Error;
    if(!TestTrue(FString::Printf(TEXT("save succeeds: %s"), *Error),
        FTheNoteStore::SaveFile(File, Written.Author, Written.Level, { Written }, Error)))
    {
        return false;
    }

    TArray<FTheNoteRecord> Read;
    if(!TestTrue(FString::Printf(TEXT("load succeeds: %s"), *Error), FTheNoteStore::LoadFile(File, Read, Error)))
    {
        return false;
    }
    if(!TestEqual(TEXT("one note back"), Read.Num(), 1))
    {
        return false;
    }

    TestTrue(TEXT("id"), Read[0].Id == Written.Id);
    TestEqual(TEXT("title"), Read[0].Title, Written.Title);

    // The body is the field a naive writer breaks: it is the only one that legitimately holds a
    // newline, and a format that escapes it wrongly loses the second half of every note.
    TestEqual(TEXT("body survives its newline"), Read[0].Body, Written.Body);
    TestEqual(TEXT("collection"), Read[0].Collection, Written.Collection);
    TestEqual(TEXT("author from the file header"), Read[0].Author, Written.Author);
    TestEqual(TEXT("level from the file header"), Read[0].Level, Written.Level);
    TestTrue(TEXT("show in game"), Read[0].bShowInGame);
    TestTrue(TEXT("location"), Read[0].Location.Equals(Written.Location, 0.0001));
    TestTrue(TEXT("created at"), Read[0].CreatedAt == Written.CreatedAt);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheNoteStoreEmptyCollectionTest, "TheNotes.Store.EmptyCollectionIsAValue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheNoteStoreEmptyCollectionTest::RunTest(const FString& Parameters)
{
    using namespace TheNoteStoreTests;

    const FString Directory = ScratchDirectory();
    const FString File = Directory / TEXT("Game.Maps.L_Main.json");
    ON_SCOPE_EXIT
    {
        IFileManager::Get().DeleteDirectory(*Directory, false, true);
    };

    const FTheNoteRecord Written = MakeOne(TEXT("Author"), TEXT("Ungrouped"), FVector::ZeroVector);

    FString Error;
    FTheNoteStore::SaveFile(File, Written.Author, Written.Level, { Written }, Error);

    TArray<FTheNoteRecord> Read;
    if(!TestTrue(FString::Printf(TEXT("load succeeds: %s"), *Error), FTheNoteStore::LoadFile(File, Read, Error)))
    {
        return false;
    }
    if(!TestEqual(TEXT("one note back"), Read.Num(), 1))
    {
        return false;
    }

    // No group is the normal state of a note, not a missing value: a reader that treats it as an
    // error, or a writer that omits the key, turns most of the file into a special case.
    TestTrue(TEXT("collection round-trips empty"), Read[0].Collection.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheNoteStoreMalformedTest, "TheNotes.Store.MalformedFileIsReportedNotEmptied", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheNoteStoreMalformedTest::RunTest(const FString& Parameters)
{
    using namespace TheNoteStoreTests;

    const FString Directory = ScratchDirectory();
    const FString File = Directory / TEXT("Game.Maps.L_Main.json");
    ON_SCOPE_EXIT
    {
        IFileManager::Get().DeleteDirectory(*Directory, false, true);
    };

    FFileHelper::SaveStringToFile(FString(TEXT("{ \"author\": \"Author\", \"notes\": [")), *File);

    TArray<FTheNoteRecord> Read;
    FString Error;

    // A truncated file must fail loudly. Reading it as zero notes is the dangerous answer: the very
    // next save would write that emptiness back and the notes would be gone for real.
    TestFalse(TEXT("truncated JSON fails"), FTheNoteStore::LoadFile(File, Read, Error));
    TestFalse(TEXT("the failure says which file"), Error.IsEmpty());
    TestEqual(TEXT("nothing is returned"), Read.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTheNoteStoreFileNameTest, "TheNotes.Store.LevelsWithTheSameShortNameGetSeparateFiles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTheNoteStoreFileNameTest::RunTest(const FString& Parameters)
{
    // Two maps called L_Test in different folders is an ordinary thing to have, and a store keyed
    // by the short name would merge one map's notes into the other's without ever saying so.
    const FString First = FTheNoteStore::LevelFileName(TEXT("/Game/Maps/L_Test"));
    const FString Second = FTheNoteStore::LevelFileName(TEXT("/Game/Maps/Tests/L_Test"));

    TestEqual(TEXT("path is flattened, not shortened"), First, FString(TEXT("Game.Maps.L_Test.json")));
    TestNotEqual(TEXT("two L_Test maps do not share a file"), First, Second);
    return true;
}

#endif

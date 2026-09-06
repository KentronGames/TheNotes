// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheNoteStore.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "TheNotesModule.h"
#include "TheNotesSettings.h"

namespace TheNoteStoreKeys
{
static const TCHAR* Author = TEXT("author");
static const TCHAR* Level = TEXT("level");
static const TCHAR* Notes = TEXT("notes");
static const TCHAR* Id = TEXT("id");
static const TCHAR* Title = TEXT("title");
static const TCHAR* Body = TEXT("body");
static const TCHAR* Collection = TEXT("collection");
static const TCHAR* Location = TEXT("location");
static const TCHAR* X = TEXT("x");
static const TCHAR* Y = TEXT("y");
static const TCHAR* Z = TEXT("z");
static const TCHAR* ShowInGame = TEXT("showInGame");
// Written only by a note that overrides its colour, so its PRESENCE is the override flag and a file
// of ordinary notes gains no line. Hex, because that is the form a person reading a diff recognises.
static const TCHAR* IconTint = TEXT("iconTint");
static const TCHAR* CreatedAt = TEXT("createdAt");
static const TCHAR* UpdatedAt = TEXT("updatedAt");
}

FString FTheNoteStore::LevelFileName(const FString& LevelPackageName)
{
    // The full package path, not the short name: two levels called L_Test in different folders are
    // a normal thing to have, and a short name would quietly merge one's notes into the other's.
    FString Flattened = LevelPackageName;
    Flattened.RemoveFromStart(TEXT("/"));
    Flattened.ReplaceInline(TEXT("/"), TEXT("."));
    return FPaths::MakeValidFileName(Flattened, TEXT('_')) + TEXT(".json");
}

FString FTheNoteStore::FilePath(const FString& Author, const FString& LevelPackageName)
{
    return UTheNotesSettings::ResolvedNotesDirectory() / FPaths::MakeValidFileName(Author, TEXT('_')) / LevelFileName(LevelPackageName);
}

TArray<FString> FTheNoteStore::FilesForLevel(const FString& LevelPackageName)
{
    TArray<FString> Found;
    const FString Root = UTheNotesSettings::ResolvedNotesDirectory();
    const FString Wanted = LevelFileName(LevelPackageName);

    TArray<FString> AuthorDirectories;
    IFileManager::Get().FindFiles(AuthorDirectories, *(Root / TEXT("*")), false, true);
    for(const FString& AuthorDirectory : AuthorDirectories)
    {
        const FString Candidate = Root / AuthorDirectory / Wanted;
        if(IFileManager::Get().FileExists(*Candidate))
        {
            Found.Add(Candidate);
        }
    }
    Found.Sort();
    return Found;
}

TArray<FString> FTheNoteStore::AllFiles()
{
    TArray<FString> Found;
    IFileManager::Get().FindFilesRecursive(Found, *UTheNotesSettings::ResolvedNotesDirectory(), TEXT("*.json"), true, false);
    Found.Sort();
    return Found;
}

bool FTheNoteStore::LoadFile(const FString& FilePath, TArray<FTheNoteRecord>& OutRecords, FString& OutError)
{
    OutRecords.Reset();
    OutError.Reset();

    FString Text;
    if(!FFileHelper::LoadFileToString(Text, *FilePath))
    {
        OutError = FString::Printf(TEXT("cannot read %s"), *FilePath);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Text);
    if(!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = FString::Printf(TEXT("%s is not valid JSON"), *FilePath);
        return false;
    }

    const FString FileAuthor = Root->GetStringField(TheNoteStoreKeys::Author);
    const FString FileLevel = Root->GetStringField(TheNoteStoreKeys::Level);

    const TArray<TSharedPtr<FJsonValue>>* Notes = nullptr;
    if(!Root->TryGetArrayField(TheNoteStoreKeys::Notes, Notes) || !Notes)
    {
        OutError = FString::Printf(TEXT("%s has no notes array"), *FilePath);
        return false;
    }

    for(const TSharedPtr<FJsonValue>& Value : *Notes)
    {
        const TSharedPtr<FJsonObject>* Entry = nullptr;
        if(!Value.IsValid() || !Value->TryGetObject(Entry) || !Entry)
        {
            continue;
        }

        FTheNoteRecord Record;
        FString IdText;
        (*Entry)->TryGetStringField(TheNoteStoreKeys::Id, IdText);

        // A note without a readable identity cannot be matched to an actor, written back, or told
        // apart from the next one. Dropping it here is better than carrying a nameless duplicate.
        if(!FGuid::Parse(IdText, Record.Id))
        {
            UE_LOG(LogTheNotes, Warning, TEXT("%s holds a note with no usable id; skipped"), *FilePath);
            continue;
        }

        (*Entry)->TryGetStringField(TheNoteStoreKeys::Title, Record.Title);
        (*Entry)->TryGetStringField(TheNoteStoreKeys::Body, Record.Body);
        (*Entry)->TryGetStringField(TheNoteStoreKeys::Collection, Record.Collection);
        (*Entry)->TryGetBoolField(TheNoteStoreKeys::ShowInGame, Record.bShowInGame);

        FString TintText;
        if((*Entry)->TryGetStringField(TheNoteStoreKeys::IconTint, TintText))
        {
            Record.bOverrideIconTint = true;
            Record.IconTint = FColor::FromHex(TintText);
        }

        const TSharedPtr<FJsonObject>* LocationObject = nullptr;
        if((*Entry)->TryGetObjectField(TheNoteStoreKeys::Location, LocationObject) && LocationObject)
        {
            (*LocationObject)->TryGetNumberField(TheNoteStoreKeys::X, Record.Location.X);
            (*LocationObject)->TryGetNumberField(TheNoteStoreKeys::Y, Record.Location.Y);
            (*LocationObject)->TryGetNumberField(TheNoteStoreKeys::Z, Record.Location.Z);
        }

        FString Stamp;
        if((*Entry)->TryGetStringField(TheNoteStoreKeys::CreatedAt, Stamp))
        {
            FDateTime::ParseIso8601(*Stamp, Record.CreatedAt);
        }
        if((*Entry)->TryGetStringField(TheNoteStoreKeys::UpdatedAt, Stamp))
        {
            FDateTime::ParseIso8601(*Stamp, Record.UpdatedAt);
        }

        // Author and level are properties of the file, not of the entry: they are written once at
        // the top and every note in the file carries them, so a hand edit cannot desynchronise them.
        Record.Author = FileAuthor;
        Record.Level = FileLevel;
        OutRecords.Add(MoveTemp(Record));
    }

    return true;
}

bool FTheNoteStore::SaveFile(const FString& FilePath, const FString& Author, const FString& LevelPackageName, const TArray<FTheNoteRecord>& Records, FString& OutError)
{
    OutError.Reset();

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TheNoteStoreKeys::Author, Author);
    Root->SetStringField(TheNoteStoreKeys::Level, LevelPackageName);

    // Sorted by identity, so that the order of notes in the file does not depend on the order the
    // editor happened to spawn actors in. An unstable order turns every save into a diff.
    TArray<FTheNoteRecord> Sorted = Records;
    Sorted.Sort([](const FTheNoteRecord& A, const FTheNoteRecord& B) { return A.Id.ToString(EGuidFormats::DigitsWithHyphens) < B.Id.ToString(EGuidFormats::DigitsWithHyphens); });

    TArray<TSharedPtr<FJsonValue>> Notes;
    Notes.Reserve(Sorted.Num());
    for(const FTheNoteRecord& Record : Sorted)
    {
        TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TheNoteStoreKeys::Id, Record.Id.ToString(EGuidFormats::DigitsWithHyphens));
        Entry->SetStringField(TheNoteStoreKeys::Title, Record.Title);
        Entry->SetStringField(TheNoteStoreKeys::Body, Record.Body);
        Entry->SetStringField(TheNoteStoreKeys::Collection, Record.Collection);

        TSharedRef<FJsonObject> LocationObject = MakeShared<FJsonObject>();
        LocationObject->SetNumberField(TheNoteStoreKeys::X, Record.Location.X);
        LocationObject->SetNumberField(TheNoteStoreKeys::Y, Record.Location.Y);
        LocationObject->SetNumberField(TheNoteStoreKeys::Z, Record.Location.Z);
        Entry->SetObjectField(TheNoteStoreKeys::Location, LocationObject);

        Entry->SetBoolField(TheNoteStoreKeys::ShowInGame, Record.bShowInGame);
        if(Record.bOverrideIconTint)
        {
            Entry->SetStringField(TheNoteStoreKeys::IconTint, Record.IconTint.ToHex());
        }
        Entry->SetStringField(TheNoteStoreKeys::CreatedAt, Record.CreatedAt.ToIso8601());
        Entry->SetStringField(TheNoteStoreKeys::UpdatedAt, Record.UpdatedAt.ToIso8601());
        Notes.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Root->SetArrayField(TheNoteStoreKeys::Notes, Notes);

    FString Output;
    TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
    if(!FJsonSerializer::Serialize(Root, Writer))
    {
        OutError = FString::Printf(TEXT("cannot serialise notes for %s"), *FilePath);
        return false;
    }

    if(!FFileHelper::SaveStringToFile(Output, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FString::Printf(TEXT("cannot write %s"), *FilePath);
        return false;
    }
    return true;
}

bool FTheNoteStore::DeleteFile(const FString& FilePath, FString& OutError)
{
    OutError.Reset();
    if(!IFileManager::Get().FileExists(*FilePath))
    {
        return true;
    }
    if(!IFileManager::Get().Delete(*FilePath))
    {
        OutError = FString::Printf(TEXT("cannot delete %s"), *FilePath);
        return false;
    }
    return true;
}

TArray<FTheNoteRecord> FTheNoteStore::LoadLevel(const FString& LevelPackageName)
{
    TArray<FTheNoteRecord> All;
    for(const FString& File : FilesForLevel(LevelPackageName))
    {
        TArray<FTheNoteRecord> Records;
        FString Error;
        if(LoadFile(File, Records, Error))
        {
            All.Append(MoveTemp(Records));
        }
        else
        {
            UE_LOG(LogTheNotes, Warning, TEXT("%s"), *Error);
        }
    }
    return All;
}

TArray<FTheNoteRecord> FTheNoteStore::LoadAll()
{
    TArray<FTheNoteRecord> All;
    for(const FString& File : AllFiles())
    {
        TArray<FTheNoteRecord> Records;
        FString Error;
        if(LoadFile(File, Records, Error))
        {
            All.Append(MoveTemp(Records));
        }
        else
        {
            UE_LOG(LogTheNotes, Warning, TEXT("%s"), *Error);
        }
    }
    return All;
}

FTheNoteRecord FTheNoteStore::MakeRecord(const FString& LevelPackageName, const FVector& Location)
{
    FTheNoteRecord Record;
    Record.Id = FGuid::NewGuid();
    Record.Author = UTheNotesUserSettings::ResolvedAuthorName();
    Record.Level = LevelPackageName;
    Record.Location = Location;
    Record.CreatedAt = FDateTime::UtcNow();
    Record.UpdatedAt = Record.CreatedAt;
    return Record;
}

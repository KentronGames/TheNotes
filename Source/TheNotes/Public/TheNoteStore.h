// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "TheNoteRecord.h"

/**
 * The note files on disk: <NotesDirectory>/<Author>/<Level>.json.
 *
 * One file per author per level is the whole conflict story. Two developers writing notes in the
 * same level write different files, so their work merges without a conflict; a file's path answers
 * which author and which level without parsing it, so opening a level parses only what it shows.
 */
struct THENOTES_API FTheNoteStore
{
    /** File name for a level, without directory: /Game/Maps/L_Main becomes Game.Maps.L_Main.json. */
    static FString LevelFileName(const FString& LevelPackageName);

    /** Full path of one author's file for one level. */
    static FString FilePath(const FString& Author, const FString& LevelPackageName);

    /** Every note file that exists for a level, across all authors. */
    static TArray<FString> FilesForLevel(const FString& LevelPackageName);

    /** Every note file that exists, for every level and author. */
    static TArray<FString> AllFiles();

    /**
     * Reads one file. Returns false and fills OutError on malformed JSON, leaving OutRecords empty:
     * a file we cannot read is reported, never silently treated as a level with no notes.
     */
    static bool LoadFile(const FString& FilePath, TArray<FTheNoteRecord>& OutRecords, FString& OutError);

    /** Writes one author's notes for one level, replacing whatever the file held. */
    static bool SaveFile(const FString& FilePath, const FString& Author, const FString& LevelPackageName, const TArray<FTheNoteRecord>& Records, FString& OutError);

    /** Deletes an author's file for a level. Missing is success — the state asked for is reached. */
    static bool DeleteFile(const FString& FilePath, FString& OutError);

    /** Every note in a level, from every author, with unreadable files logged and skipped. */
    static TArray<FTheNoteRecord> LoadLevel(const FString& LevelPackageName);

    /** Every note anywhere, for the browser's list. */
    static TArray<FTheNoteRecord> LoadAll();

    /** A new record, signed by this developer and stamped with the current time. */
    static FTheNoteRecord MakeRecord(const FString& LevelPackageName, const FVector& Location);
};

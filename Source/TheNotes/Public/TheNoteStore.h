// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "TheNoteRecord.h"

/**
 * The note files on disk: <NotesDirectory>/<Author>/<Level>.json, plus <Author>/Assets.json.
 *
 * One file per author per level is the whole conflict story. Two developers writing notes in the
 * same level write different files, so their work merges without a conflict; a file's path answers
 * which author and which level without parsing it, so opening a level parses only what it shows.
 *
 * Comments on assets take one more file per author rather than one per asset: an asset is commented
 * once or twice in its life, so a file each would be a directory of one-line files, and the whole
 * point of the asset file is that a reader — a person or an agent — opens ONE path to see what this
 * author has said about the content. A level's flattened name always carries its mount point
 * (/Game/Maps/L_Main becomes Game.Maps.L_Main.json), so no level can claim the assets file's name.
 */
struct THENOTES_API FTheNoteStore
{
    /** File name for a level, without directory: /Game/Maps/L_Main becomes Game.Maps.L_Main.json. */
    static FString LevelFileName(const FString& LevelPackageName);

    /** File name every author's asset comments are kept in, without directory. */
    static FString AssetsFileName();

    /** Full path of one author's file for one level. */
    static FString FilePath(const FString& Author, const FString& LevelPackageName);

    /** Full path of one author's asset comments. */
    static FString AssetsFilePath(const FString& Author);

    /** Every note file that exists for a level, across all authors. */
    static TArray<FString> FilesForLevel(const FString& LevelPackageName);

    /** Every note file that exists, for every level and author. */
    static TArray<FString> AllFiles();

    /** Every author's assets file that exists. */
    static TArray<FString> AssetFiles();

    /**
     * Reads one file. Returns false and fills OutError on malformed JSON, leaving OutRecords empty:
     * a file we cannot read is reported, never silently treated as a level with no notes.
     */
    static bool LoadFile(const FString& FilePath, TArray<FTheNoteRecord>& OutRecords, FString& OutError);

    /**
     * Writes one author's notes for one level, replacing whatever the file held.
     *
     * An empty level writes the assets file instead: no level line at the top, and each entry carries
     * the asset it is about, because one asset file holds comments on many assets while one level file
     * holds notes standing in one level.
     */
    static bool SaveFile(const FString& FilePath, const FString& Author, const FString& LevelPackageName, const TArray<FTheNoteRecord>& Records, FString& OutError);

    /** Deletes an author's file for a level. Missing is success — the state asked for is reached. */
    static bool DeleteFile(const FString& FilePath, FString& OutError);

    /** Every note in a level, from every author, with unreadable files logged and skipped. */
    static TArray<FTheNoteRecord> LoadLevel(const FString& LevelPackageName);

    /** Every note anywhere, for the browser's list. */
    static TArray<FTheNoteRecord> LoadAll();

    /** One author's comments on assets, sorted by the asset they are about. */
    static TArray<FTheNoteRecord> LoadAssetComments(const FString& Author);

    /** Writes one author's asset comments, removing the file when he has none left. */
    static bool SaveAssetComments(const FString& Author, const TArray<FTheNoteRecord>& Records, FString& OutError);

    /** A new record, signed by this developer and stamped with the current time. */
    static FTheNoteRecord MakeRecord(const FString& LevelPackageName, const FVector& Location);

    /** A new comment on an asset, signed and stamped the same way. */
    static FTheNoteRecord MakeAssetRecord(const FString& AssetPackageName);
};

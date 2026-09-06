// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "TheNoteRecord.generated.h"

/**
 * One developer note, in the shape it is stored in.
 *
 * While a level is open the actor is the truth and this struct is what the actor is written to;
 * between sessions the file is the truth and this struct is what it is read into. Nothing else
 * holds note state, so there is never a third copy to reconcile.
 */
USTRUCT(BlueprintType)
struct THENOTES_API FTheNoteRecord
{
    GENERATED_BODY()

    /** Identity that survives moving, retitling and rewriting. Assigned once, never reused. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Note")
    FGuid Id;

    /** The one line shown under the icon in the scene and in the DEV Notes list. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
    FString Title;

    /** The note itself. Hidden until the cursor is over the note in the editor. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note", meta = (MultiLine = true))
    FString Body;

    /**
     * Who wrote it. Decides which file the note is stored in, and an edit never changes it: a note
     * corrected by somebody else is still the author's note, in the author's file.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Note")
    FString Author;

    /** Free-form grouping within one author's notes. Empty is normal and means no group. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note")
    FString Collection;

    /** Long package name of the level the note stands in, such as /Game/Maps/L_Main. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Note")
    FString Level;

    /** Where the note stands. Written from the actor's transform, never edited by hand. */
    UPROPERTY()
    FVector Location = FVector::ZeroVector;

    /**
     * Marks a note as intended for players rather than for the team.
     *
     * Reserved, and hidden from the details panel for exactly that reason: the store keeps the flag and
     * nothing reads it at runtime, so a tick box here would be a promise the plugin does not keep.
     */
    UPROPERTY()
    bool bShowInGame = false;

    /** Whether this one note is marked in a colour of its own, whatever its collection says. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note", meta = (InlineEditConditionToggle))
    bool bOverrideIconTint = false;

    /**
     * The colour this note's mark is drawn in.
     *
     * The narrowest of three answers: a note that overrides wins, otherwise the colour its collection is
     * given in the project settings, otherwise the project's general tint. Turning the toggle off is what
     * hands the note back to its collection — clearing the colour would only make it black.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note", meta = (EditCondition = "bOverrideIconTint"))
    FColor IconTint = FColor(253, 151, 31);

    /** When the note was first written. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Note")
    FDateTime CreatedAt = FDateTime(0);

    /** When it was last changed, in content or in position. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Note")
    FDateTime UpdatedAt = FDateTime(0);

    bool IsValidRecord() const { return Id.IsValid(); }

    /**
     * Equal in everything a human wrote, ignoring when it was written.
     *
     * This is what decides whether UpdatedAt moves. Stamping every note on every save would make
     * one dragged marker produce a diff on every note in the file, which is how a store stops
     * being reviewable in a week.
     *
     * The overridden colour counts only while the override is on. A note that does not use a colour of
     * its own must not restamp itself because the picker sitting behind its disabled toggle differs.
     */
    bool EqualsIgnoringStamps(const FTheNoteRecord& Other) const
    {
        return Id == Other.Id && Title == Other.Title && Body == Other.Body && Author == Other.Author && Collection == Other.Collection && Level == Other.Level && bShowInGame == Other.bShowInGame && bOverrideIconTint == Other.bOverrideIconTint &&
               (!bOverrideIconTint || IconTint == Other.IconTint) && Location.Equals(Other.Location, 0.01);
    }
};

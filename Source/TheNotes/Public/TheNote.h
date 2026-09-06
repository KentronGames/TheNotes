// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "TheNoteRecord.h"

#include "TheNote.generated.h"

/**
 * A note standing in the world.
 *
 * In the editor these are spawned transient by the plugin and are never written into the map: the
 * store on disk is where a note lives, and this actor is how a developer touches it. Not placeable
 * on purpose — an actor dragged in from the placement browser would belong to no file and would
 * disappear at the next map load, looking exactly like lost work.
 */
UCLASS(NotPlaceable, meta = (DisplayName = "DEV Note"))
class THENOTES_API ATheNote : public AActor
{
    GENERATED_BODY()

public:
    ATheNote();

    /** The note this actor stands for. Editing these fields is editing the note. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note", meta = (ShowOnlyInnerProperties))
    FTheNoteRecord Record;

    /** The record with this actor's current position in it, ready to be written to the store. */
    FTheNoteRecord ToRecord() const;

    /** Takes a record from the store: content, identity and position. */
    void ApplyRecord(const FTheNoteRecord& InRecord);

#if WITH_EDITORONLY_DATA
    class UBillboardComponent* GetSprite() const { return SpriteComponent; }
#endif

protected:
    virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
    /** Retyping the collection has to move the mark to that collection's colour, which is built at construction. */
    virtual void PostEditChangeProperty(struct FPropertyChangedEvent& Event) override;
#endif

private:
#if WITH_EDITORONLY_DATA
    UPROPERTY()
    TObjectPtr<class UBillboardComponent> SpriteComponent;
#endif
};

// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheNote.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Texture2D.h"

#include "TheNotesModule.h"
#include "TheNotesSettings.h"

#define LOCTEXT_NAMESPACE "TheNote"

ATheNote::ATheNote()
{
    PrimaryActorTick.bCanEverTick = false;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

#if WITH_EDITORONLY_DATA
    SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
    if(SpriteComponent)
    {
        // The category has to exist on the class default object: the editor builds its sprite
        // category list by walking CDOs at engine init, so a category assigned later is not there
        // when the viewport's show-flag menu is built and the notes cannot be hidden by category.
        SpriteComponent->SpriteInfo.Category = TEXT("DEVNotes");
        SpriteComponent->SpriteInfo.DisplayName = NSLOCTEXT("SpriteCategory", "DEVNotes", "DEV Notes");
        SpriteComponent->bIsScreenSizeScaled = true;
        SpriteComponent->SetUsingAbsoluteScale(true);
        SpriteComponent->SetupAttachment(RootComponent);
    }
#endif
}

void ATheNote::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

#if WITH_EDITORONLY_DATA
    if(SpriteComponent)
    {
        const UTheNotesSettings* Settings = GetDefault<UTheNotesSettings>();
        UTexture2D* Sprite = Settings ? Settings->NoteSprite.LoadSynchronous() : nullptr;
        if(!Sprite)
        {
            Sprite = FTheNotesModule::DefaultSprite();
        }
        if(Sprite)
        {
            SpriteComponent->SetSprite(Sprite);
        }
    }
#endif
}

FTheNoteRecord ATheNote::ToRecord() const
{
    FTheNoteRecord Out = Record;
    Out.Location = GetActorLocation();
    return Out;
}

void ATheNote::ApplyRecord(const FTheNoteRecord& InRecord)
{
    Record = InRecord;
    SetActorLocation(InRecord.Location);

#if WITH_EDITOR
    // The outliner is one of the two places the spec asks a note to be findable, and it shows
    // labels rather than titles. Keeping them equal is what makes searching the outliner work.
    const FString Label = InRecord.Title.IsEmpty() ? LOCTEXT("UntitledNote", "DEV Note").ToString() : InRecord.Title;
    if(GetActorLabel() != Label)
    {
        SetActorLabel(Label, false);
    }
#endif
}

#undef LOCTEXT_NAMESPACE

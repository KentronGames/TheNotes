// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheNotesModule.h"

#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Interfaces/IPluginManager.h"

DEFINE_LOG_CATEGORY(LogTheNotes);

void FTheNotesModule::StartupModule()
{
}

void FTheNotesModule::ShutdownModule()
{
    Sprite.Reset();
}

UTexture2D* FTheNotesModule::DefaultSprite()
{
    FTheNotesModule* Module = FModuleManager::GetModulePtr<FTheNotesModule>(TEXT("TheNotes"));
    if(!Module)
    {
        return nullptr;
    }

    if(!Module->Sprite.IsValid())
    {
        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("TheNotes"));
        if(!Plugin.IsValid())
        {
            return nullptr;
        }

        const FString File = Plugin->GetBaseDir() / TEXT("Resources/Icons/T_DevNote.png");
        Module->Sprite.Reset(FImageUtils::ImportFileAsTexture2D(File));
        if(!Module->Sprite.IsValid())
        {
            // Silence here would look exactly like a note that failed to spawn, so say which file
            // was missing rather than leaving an unmarked point in space.
            UE_LOG(LogTheNotes, Warning, TEXT("Cannot read the note sprite at %s; notes will have no icon"), *File);
        }
    }
    return Module->Sprite.Get();
}

IMPLEMENT_MODULE(FTheNotesModule, TheNotes)

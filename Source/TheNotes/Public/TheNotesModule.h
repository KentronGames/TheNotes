// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"

THENOTES_API DECLARE_LOG_CATEGORY_EXTERN(LogTheNotes, Log, All);

class FTheNotesModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /**
     * The mark a note shows when the project has named no sprite of its own: the plugin's own PNG,
     * read from Resources and turned into a transient texture.
     *
     * Deliberately not a .uasset. A plugin that ships its mark as a file needs no content directory,
     * nothing of it travels through LFS, and replacing the mark is replacing one PNG.
     */
    static class UTexture2D* DefaultSprite();

private:
    TStrongObjectPtr<class UTexture2D> Sprite;
};

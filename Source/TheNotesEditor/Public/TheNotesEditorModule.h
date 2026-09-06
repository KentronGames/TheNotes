// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FTheNotesEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    TSharedRef<class SDockTab> SpawnBrowserTab(const class FSpawnTabArgs& Args);

    TUniquePtr<class FTheNotesViewportLabels> ViewportLabels;
};

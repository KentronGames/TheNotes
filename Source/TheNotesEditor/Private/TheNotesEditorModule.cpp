// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheNotesEditorModule.h"

#include "Editor.h"
#include "ToolMenus.h"
#include "HAL/IConsoleManager.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#include "STheNotesBrowser.h"
#include "TheNotesEditorStyle.h"
#include "TheNotesEditorSubsystem.h"
#include "TheNotesViewportLabels.h"

#define LOCTEXT_NAMESPACE "TheNotesEditor"

namespace TheNotesEditorLocal
{
static const FName BrowserTabName("TheNotesBrowser");
static const FName SectionName("TheNotes");

/** The two viewport menus a note can be created from: with something selected, and with nothing. */
static const TCHAR* ContextMenus[] = {TEXT("LevelEditor.ActorContextMenu"), TEXT("LevelEditor.EmptySelectionContextMenu")};

UTheNotesEditorSubsystem* Subsystem()
{
    return GEditor ? GEditor->GetEditorSubsystem<UTheNotesEditorSubsystem>() : nullptr;
}

void CreateNoteAtClickLocation()
{
    if(UTheNotesEditorSubsystem* Notes = Subsystem())
    {
        // Where the right-click ray hit the world. The editor caches it on every click for exactly
        // this purpose — it is what the engine's own "place actor here" entries read.
        Notes->CreateNoteAt(GEditor->ClickLocation);
    }
}

void ReloadNotes()
{
    if(UTheNotesEditorSubsystem* Notes = Subsystem())
    {
        Notes->Reload();
    }
}

// The manual way out when the watcher cannot see the change — a network share, a checkout restored
// underneath the editor, a platform whose file notifications the engine does not implement.
static FAutoConsoleCommand ReloadCommand(TEXT("TheNotes.Reload"), TEXT("Read the open level's note files again, picking up notes that arrived from source control."), FConsoleCommandDelegate::CreateStatic(&ReloadNotes));
}

void FTheNotesEditorModule::StartupModule()
{
    FTheNotesEditorStyle::Register();

    ViewportLabels = MakeUnique<FTheNotesViewportLabels>();
    ViewportLabels->Register();

    FGlobalTabmanager::Get()
        ->RegisterNomadTabSpawner(TheNotesEditorLocal::BrowserTabName, FOnSpawnTab::CreateRaw(this, &FTheNotesEditorModule::SpawnBrowserTab))
        .SetDisplayName(LOCTEXT("BrowserTabTitle", "DEV Notes"))
        .SetTooltipText(LOCTEXT("BrowserTabTooltip", "Every developer note in the project, and where it stands"))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory())
        .SetIcon(FSlateIcon(FTheNotesEditorStyle::StyleName(), "TheNotes.TabIcon"));

    UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FTheNotesEditorModule::RegisterMenus));
}

void FTheNotesEditorModule::ShutdownModule()
{
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TheNotesEditorLocal::BrowserTabName);

    if(ViewportLabels.IsValid())
    {
        ViewportLabels->Unregister();
        ViewportLabels.Reset();
    }

    FTheNotesEditorStyle::Unregister();
}

void FTheNotesEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    for(const TCHAR* MenuName : TheNotesEditorLocal::ContextMenus)
    {
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(FName(MenuName));
        if(!Menu)
        {
            continue;
        }

        FToolMenuSection& Section = Menu->FindOrAddSection(TheNotesEditorLocal::SectionName, LOCTEXT("NotesSection", "DEV Notes"));
        Section.AddMenuEntry(TEXT("CreateDevNoteHere"),
            LOCTEXT("CreateDevNoteHere", "Create DEV Note Here"),
            LOCTEXT("CreateDevNoteHereTooltip", "Puts a developer note at this point in the scene"),
            FSlateIcon(FTheNotesEditorStyle::StyleName(), "TheNotes.TabIcon"),
            FUIAction(FExecuteAction::CreateStatic(&TheNotesEditorLocal::CreateNoteAtClickLocation)));
    }
}

TSharedRef<SDockTab> FTheNotesEditorModule::SpawnBrowserTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(STheNotesBrowser)];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTheNotesEditorModule, TheNotesEditor)

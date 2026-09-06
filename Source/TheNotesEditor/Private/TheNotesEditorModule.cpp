// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheNotesEditorModule.h"

#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditor.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Engine/World.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/Docking/SDockTab.h"

#include "STheNotesBrowser.h"
#include "TheNotesEditorStyle.h"
#include "TheNotesEditorSubsystem.h"
#include "TheNotesViewportLabels.h"

#define LOCTEXT_NAMESPACE "TheNotesEditor"

/**
 * The plugin's commands, so the editor owns their key bindings.
 *
 * Declared rather than hard-bound: a chord registered this way appears in Editor Preferences → Keyboard
 * Shortcuts under «The Notes», where a project whose Shift+N already means something else can move it.
 */
class FTheNotesCommands : public TCommands<FTheNotesCommands>
{
public:
    FTheNotesCommands() : TCommands<FTheNotesCommands>(TEXT("TheNotes"), LOCTEXT("CommandsContext", "The Notes"), NAME_None, FTheNotesEditorStyle::StyleName()) { }

    virtual void RegisterCommands() override
    {
        UI_COMMAND(CreateNoteHere, "Create DEV Note Here", "Puts a developer note where the cursor is pointing", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::N));
        UI_COMMAND(TogglePin, "Pin DEV Note", "Keeps the note under the cursor open while you work on what it is about", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Shift, EKeys::P));
        UI_COMMAND(ShowAllNotes, "Show All DEV Notes", "Shows every note's title at once instead of only the one under the cursor", EUserInterfaceActionType::ToggleButton, FInputChord());
    }

    TSharedPtr<FUICommandInfo> CreateNoteHere;
    TSharedPtr<FUICommandInfo> TogglePin;
    TSharedPtr<FUICommandInfo> ShowAllNotes;
};

namespace TheNotesEditorLocal
{
static const FName BrowserTabName("TheNotesBrowser");
static const FName SectionName("TheNotes");

/** The two viewport menus a note can be created from: with something selected, and with nothing. */
static const TCHAR* ContextMenus[] = {TEXT("LevelEditor.ActorContextMenu"), TEXT("LevelEditor.EmptySelectionContextMenu")};

/** How far down the cursor ray a note lands when the ray hits nothing at all. */
static constexpr double UnobstructedDistance = 1000.0;

UTheNotesEditorSubsystem* Subsystem()
{
    return GEditor ? GEditor->GetEditorSubsystem<UTheNotesEditorSubsystem>() : nullptr;
}

/**
 * Where a note goes when the command came from a key rather than from a right-click.
 *
 * `GEditor->ClickLocation` is what the context-menu entry reads, and it is the last place the mouse was
 * CLICKED — right for a menu that was opened by that click, wrong for a key pressed some time later
 * while the cursor has moved on. So the ray under the cursor is traced instead, and a ray that hits
 * nothing puts the note out in front of the camera rather than at the world origin.
 */
FVector CursorPlacement()
{
    FEditorViewportClient* Client = GEditor ? static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient()) : nullptr;
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if(!Client || !World)
    {
        return GEditor ? GEditor->ClickLocation : FVector::ZeroVector;
    }

    const FViewportCursorLocation Cursor = Client->GetCursorWorldLocationFromMousePos();
    const FVector Start = Cursor.GetOrigin();
    const FVector End = Start + Cursor.GetDirection() * HALF_WORLD_MAX;

    FHitResult Hit;
    if(World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic))
    {
        return Hit.Location;
    }
    return Start + Cursor.GetDirection() * UnobstructedDistance;
}

void CreateNoteAtClickLocation()
{
    if(UTheNotesEditorSubsystem* Notes = Subsystem())
    {
        // The editor caches the click point for exactly this purpose — it is what the engine's own
        // "place actor here" entries read.
        Notes->CreateNoteAt(GEditor->ClickLocation);
    }
}

void CreateNoteAtCursor()
{
    if(UTheNotesEditorSubsystem* Notes = Subsystem())
    {
        Notes->CreateNoteAt(CursorPlacement());
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
    FTheNotesCommands::Register();

    ViewportLabels = MakeUnique<FTheNotesViewportLabels>();
    ViewportLabels->Register();

    BindCommands();

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

    FTheNotesCommands::Unregister();
    FTheNotesEditorStyle::Unregister();
}

void FTheNotesEditorModule::BindCommands()
{
    // Bound into the level editor's own command list, which is the one that is listening while the
    // viewport has focus — a list of our own would need a widget to attach it to and would answer only
    // while that widget had focus.
    FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
    const TSharedRef<FUICommandList> Actions = LevelEditor.GetGlobalLevelEditorActions();
    const FTheNotesCommands& Commands = FTheNotesCommands::Get();

    Actions->MapAction(Commands.CreateNoteHere, FExecuteAction::CreateStatic(&TheNotesEditorLocal::CreateNoteAtCursor));

    Actions->MapAction(Commands.TogglePin, FExecuteAction::CreateRaw(this, &FTheNotesEditorModule::TogglePin));

    Actions->MapAction(Commands.ShowAllNotes, FExecuteAction::CreateRaw(this, &FTheNotesEditorModule::ToggleShowAll), FCanExecuteAction(), FIsActionChecked::CreateRaw(this, &FTheNotesEditorModule::IsShowingAll));
}

void FTheNotesEditorModule::TogglePin()
{
    if(ViewportLabels.IsValid())
    {
        ViewportLabels->TogglePinnedToHovered();
    }
}

void FTheNotesEditorModule::ToggleShowAll()
{
    if(ViewportLabels.IsValid())
    {
        ViewportLabels->SetShowAll(!ViewportLabels->IsShowingAll());
    }
}

bool FTheNotesEditorModule::IsShowingAll() const
{
    return ViewportLabels.IsValid() && ViewportLabels->IsShowingAll();
}

void FTheNotesEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    const FTheNotesCommands& Commands = FTheNotesCommands::Get();

    for(const TCHAR* MenuName : TheNotesEditorLocal::ContextMenus)
    {
        UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(FName(MenuName));
        if(!Menu)
        {
            continue;
        }

        FToolMenuSection& Section = Menu->FindOrAddSection(TheNotesEditorLocal::SectionName, LOCTEXT("NotesSection", "DEV Notes"));

        // Its own entry rather than the command's: this one places the note where the menu was opened,
        // and the command places it under the cursor. Same intent, two different right answers.
        Section.AddMenuEntry(TEXT("CreateDevNoteHere"),
            LOCTEXT("CreateDevNoteHere", "Create DEV Note Here"),
            LOCTEXT("CreateDevNoteHereTooltip", "Puts a developer note at this point in the scene"),
            FSlateIcon(FTheNotesEditorStyle::StyleName(), "TheNotes.TabIcon"),
            FUIAction(FExecuteAction::CreateStatic(&TheNotesEditorLocal::CreateNoteAtClickLocation)));

        Section.AddMenuEntryWithCommandList(Commands.ShowAllNotes, FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetGlobalLevelEditorActions());
        Section.AddMenuEntryWithCommandList(Commands.TogglePin, FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetGlobalLevelEditorActions());
    }
}

TSharedRef<SDockTab> FTheNotesEditorModule::SpawnBrowserTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(STheNotesBrowser)];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTheNotesEditorModule, TheNotesEditor)

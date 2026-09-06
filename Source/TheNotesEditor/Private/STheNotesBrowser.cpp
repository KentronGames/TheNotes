// (c) 2026 Kentron Cowboys. All rights reserved.

#include "STheNotesBrowser.h"

#include "Editor.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#include "TheNotesEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "TheNotesBrowser"

namespace TheNotesBrowserColumns
{
static const FName Author("Author");
static const FName Collection("Collection");
static const FName Title("Title");
// One column for both kinds of subject — the level a note stands in, or the asset a comment is about.
// A note has exactly one of the two, so a column each would be a column half empty in every row.
static const FName Level("Level");
static const FName Asset("Asset");
static const FName Created("Created");
static const FName Updated("Updated");
}

namespace TheNotesBrowserLocal
{
/** The level's short name, which is what a developer calls it — /Game/Maps/L_Main is L_Main. */
FString ShortLevelName(const FString& PackageName)
{
    FString Short = PackageName;
    int32 LastSlash = INDEX_NONE;
    if(Short.FindLastChar(TEXT('/'), LastSlash))
    {
        Short = Short.RightChop(LastSlash + 1);
    }
    return Short;
}

/**
 * Stamps are written in UTC, and a developer reads the clock on his wall.
 *
 * The offset is asked of the engine rather than stored: it is the difference between the two clocks
 * right now, which is also the only form that survives the machine crossing a daylight-saving boundary
 * mid-session. An empty stamp prints as nothing rather than as the first second of year one.
 */
FString LocalStamp(const FDateTime& Utc)
{
    if(Utc.GetTicks() == 0)
    {
        return FString();
    }
    return (Utc + (FDateTime::Now() - FDateTime::UtcNow())).ToString(TEXT("%Y-%m-%d %H:%M"));
}

/**
 * One term of the filter: either a field the user named, or a word to look for anywhere.
 *
 * `author:` and `level:` exist because those are the two questions a list of notes is actually asked,
 * and a bare substring answers both of them wrongly — a level called Desert matches a note whose text
 * merely says «desert».
 */
struct FTerm
{
    FName Field;
    FString Value;

    bool Matches(const FTheNoteRecord& Record) const
    {
        if(Field == TheNotesBrowserColumns::Author)
        {
            return Record.Author.Contains(Value);
        }
        if(Field == TheNotesBrowserColumns::Level)
        {
            return Record.Level.Contains(Value);
        }
        if(Field == TheNotesBrowserColumns::Collection)
        {
            return Record.Collection.Contains(Value);
        }
        if(Field == TheNotesBrowserColumns::Asset)
        {
            return Record.Asset.Contains(Value);
        }
        return Record.Title.Contains(Value) || Record.Body.Contains(Value) || Record.Author.Contains(Value) || Record.Collection.Contains(Value) || Record.Level.Contains(Value) || Record.Asset.Contains(Value);
    }
};

/** Splits what was typed into terms. Every term has to match — narrowing is what a filter box is for. */
TArray<FTerm> ParseFilter(const FString& Text)
{
    TArray<FTerm> Terms;
    TArray<FString> Words;
    Text.ParseIntoArrayWS(Words);

    for(const FString& Word : Words)
    {
        FString Field;
        FString Value;
        if(Word.Split(TEXT(":"), &Field, &Value) && !Value.IsEmpty())
        {
            const FName AsField(*Field);
            if(AsField == TheNotesBrowserColumns::Author || AsField == TheNotesBrowserColumns::Level || AsField == TheNotesBrowserColumns::Collection || AsField == TheNotesBrowserColumns::Asset)
            {
                Terms.Add({AsField, Value});
                continue;
            }
        }
        Terms.Add({NAME_None, Word});
    }
    return Terms;
}

/** The text a column shows, which is also what it sorts on — so the eye and the order cannot disagree. */
FString CellText(const FTheNoteRecord& Record, const FName& Column)
{
    if(Column == TheNotesBrowserColumns::Author)
    {
        return Record.Author;
    }
    if(Column == TheNotesBrowserColumns::Collection)
    {
        return Record.Collection;
    }
    if(Column == TheNotesBrowserColumns::Title)
    {
        return Record.Title;
    }
    if(Column == TheNotesBrowserColumns::Level)
    {
        // The asset in full and the level short. A level is a place the developer already knows by
        // its short name, and an asset is a path he is about to hand to somebody — an agent most of
        // all — so the one thing it must not be is shortened.
        return Record.Asset.IsEmpty() ? ShortLevelName(Record.Level) : Record.Asset;
    }
    if(Column == TheNotesBrowserColumns::Created)
    {
        return LocalStamp(Record.CreatedAt);
    }
    if(Column == TheNotesBrowserColumns::Updated)
    {
        return LocalStamp(Record.UpdatedAt);
    }
    return FString();
}
}

class STheNoteRow : public SMultiColumnTableRow<TSharedPtr<FTheNoteRecord>>
{
public:
    SLATE_BEGIN_ARGS(STheNoteRow) { }
    SLATE_ARGUMENT(TSharedPtr<FTheNoteRecord>, Record)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
    {
        Record = InArgs._Record;
        SMultiColumnTableRow<TSharedPtr<FTheNoteRecord>>::Construct(FSuperRowType::FArguments(), OwnerTable);
    }

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
    {
        const FString Text = Record.IsValid() ? TheNotesBrowserLocal::CellText(*Record, ColumnName) : FString();

        return SNew(SBox).Padding(FMargin(6.0f, 2.0f)).VAlign(VAlign_Center)[SNew(STextBlock).Text(FText::FromString(Text)).ToolTipText(Record.IsValid() ? FText::FromString(Record->Body) : FText::GetEmpty())];
    }

private:
    TSharedPtr<FTheNoteRecord> Record;
};

void STheNotesBrowser::Construct(const FArguments& InArgs)
{
    if(GEditor)
    {
        UTheNotesEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UTheNotesEditorSubsystem>();
        if(Subsystem)
        {
            NotesChangedHandle = Subsystem->OnNotesChanged().AddSP(this, &STheNotesBrowser::Refresh);
        }
    }

    ChildSlot[SNew(SVerticalBox) +
              SVerticalBox::Slot().AutoHeight().Padding(4.0f)
                  [SNew(SHorizontalBox) +
                      SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SSearchBox).HintText(LOCTEXT("FilterHint", "Words to find, or author: / level: / asset: / collection: to name a field")).OnTextChanged(this, &STheNotesBrowser::HandleFilterChanged)] +
                      SHorizontalBox::Slot()
                          .AutoWidth()
                          .Padding(6.0f, 0.0f, 0.0f, 0.0f)
                          .VAlign(VAlign_Center)[SNew(SCheckBox)
                                  .ToolTipText(LOCTEXT("ThisLevelTooltip", "Show only the notes standing in the level that is open"))
                                  .OnCheckStateChanged(this, &STheNotesBrowser::HandleThisLevelChanged)[SNew(STextBlock).Text(LOCTEXT("ThisLevel", "This level"))]] +
                      SHorizontalBox::Slot()
                          .AutoWidth()
                          .Padding(6.0f, 0.0f, 0.0f, 0.0f)
                          .VAlign(VAlign_Center)[SNew(SButton)
                                  .Text(LOCTEXT("Delete", "Delete"))
                                  .ToolTipText(LOCTEXT("DeleteTooltip", "Remove the selected note. Its text stays in the history of the note files."))
                                  .IsEnabled(this, &STheNotesBrowser::CanDelete)
                                  .OnClicked(this, &STheNotesBrowser::HandleDeleteClicked)] +
                      SHorizontalBox::Slot()
                          .AutoWidth()
                          .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                          .VAlign(VAlign_Center)
                              [SNew(SButton).Text(LOCTEXT("Reload", "Reload")).ToolTipText(LOCTEXT("ReloadTooltip", "Read the note files again, picking up notes that arrived from source control")).OnClicked(this, &STheNotesBrowser::HandleReloadClicked)]] +
              SVerticalBox::Slot().FillHeight(1.0f)[SAssignNew(ListView, SListView<TSharedPtr<FTheNoteRecord>>)
                      .ListItemsSource(&Rows)
                      .OnGenerateRow(this, &STheNotesBrowser::MakeRow)
                      .OnMouseButtonDoubleClick(this, &STheNotesBrowser::HandleRowActivated)
                      .SelectionMode(ESelectionMode::Single)
                      .HeaderRow(SNew(SHeaderRow) +
                                 SHeaderRow::Column(TheNotesBrowserColumns::Author)
                                     .DefaultLabel(LOCTEXT("AuthorColumn", "Author"))
                                     .FillWidth(0.14f)
                                     .SortMode(this, &STheNotesBrowser::SortModeFor, TheNotesBrowserColumns::Author)
                                     .OnSort(this, &STheNotesBrowser::HandleSort) +
                                 SHeaderRow::Column(TheNotesBrowserColumns::Collection)
                                     .DefaultLabel(LOCTEXT("CollectionColumn", "Collection"))
                                     .FillWidth(0.14f)
                                     .SortMode(this, &STheNotesBrowser::SortModeFor, TheNotesBrowserColumns::Collection)
                                     .OnSort(this, &STheNotesBrowser::HandleSort) +
                                 SHeaderRow::Column(TheNotesBrowserColumns::Title)
                                     .DefaultLabel(LOCTEXT("TitleColumn", "Title"))
                                     .FillWidth(0.34f)
                                     .SortMode(this, &STheNotesBrowser::SortModeFor, TheNotesBrowserColumns::Title)
                                     .OnSort(this, &STheNotesBrowser::HandleSort) +
                                 SHeaderRow::Column(TheNotesBrowserColumns::Level)
                                     .DefaultLabel(LOCTEXT("LevelColumn", "Where"))
                                     .FillWidth(0.14f)
                                     .SortMode(this, &STheNotesBrowser::SortModeFor, TheNotesBrowserColumns::Level)
                                     .OnSort(this, &STheNotesBrowser::HandleSort) +
                                 SHeaderRow::Column(TheNotesBrowserColumns::Created)
                                     .DefaultLabel(LOCTEXT("CreatedColumn", "Written"))
                                     .FillWidth(0.12f)
                                     .SortMode(this, &STheNotesBrowser::SortModeFor, TheNotesBrowserColumns::Created)
                                     .OnSort(this, &STheNotesBrowser::HandleSort) +
                                 SHeaderRow::Column(TheNotesBrowserColumns::Updated)
                                     .DefaultLabel(LOCTEXT("UpdatedColumn", "Changed"))
                                     .FillWidth(0.12f)
                                     .SortMode(this, &STheNotesBrowser::SortModeFor, TheNotesBrowserColumns::Updated)
                                     .OnSort(this, &STheNotesBrowser::HandleSort))]];

    Refresh();
}

STheNotesBrowser::~STheNotesBrowser()
{
    if(GEditor && NotesChangedHandle.IsValid())
    {
        UTheNotesEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UTheNotesEditorSubsystem>();
        if(Subsystem)
        {
            Subsystem->OnNotesChanged().Remove(NotesChangedHandle);
        }
    }
}

TSharedRef<ITableRow> STheNotesBrowser::MakeRow(TSharedPtr<FTheNoteRecord> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(STheNoteRow, OwnerTable).Record(Item);
}

void STheNotesBrowser::HandleRowActivated(TSharedPtr<FTheNoteRecord> Item)
{
    if(!Item.IsValid() || !GEditor)
    {
        return;
    }

    UTheNotesEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UTheNotesEditorSubsystem>();
    if(Subsystem)
    {
        Subsystem->FocusOnNote(Item->Id);
    }
}

void STheNotesBrowser::HandleFilterChanged(const FText& Text)
{
    Filter = Text.ToString();
    Refresh();
}

void STheNotesBrowser::HandleThisLevelChanged(ECheckBoxState State)
{
    bThisLevelOnly = State == ECheckBoxState::Checked;
    Refresh();
}

FReply STheNotesBrowser::HandleReloadClicked()
{
    if(GEditor)
    {
        UTheNotesEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UTheNotesEditorSubsystem>();
        if(Subsystem)
        {
            Subsystem->Reload();
        }
    }
    return FReply::Handled();
}

bool STheNotesBrowser::CanDelete() const
{
    if(!ListView.IsValid() || !GEditor)
    {
        return false;
    }

    const TArray<TSharedPtr<FTheNoteRecord>> Selected = ListView->GetSelectedItems();
    if(Selected.Num() != 1 || !Selected[0].IsValid())
    {
        return false;
    }

    // A comment on an asset is deleted from wherever the list is: its file is its whole existence, so
    // there is no open level for it to be waiting on. A note in a level that is not open is the other
    // case — it has no actor to destroy, and rewriting somebody's file from here would be a change
    // with nothing on screen to show for it.
    if(!Selected[0]->Asset.IsEmpty())
    {
        return true;
    }

    const UTheNotesEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UTheNotesEditorSubsystem>();
    return Subsystem && Selected[0]->Level == Subsystem->GetTrackedLevel();
}

FReply STheNotesBrowser::HandleDeleteClicked()
{
    UTheNotesEditorSubsystem* Subsystem = GEditor ? GEditor->GetEditorSubsystem<UTheNotesEditorSubsystem>() : nullptr;
    if(Subsystem && CanDelete())
    {
        Subsystem->DeleteNote(ListView->GetSelectedItems()[0]->Id);
    }
    return FReply::Handled();
}

void STheNotesBrowser::HandleSort(EColumnSortPriority::Type Priority, const FName& Column, EColumnSortMode::Type Mode)
{
    SortColumn = Column;
    SortMode = Mode;
    Refresh();
}

EColumnSortMode::Type STheNotesBrowser::SortModeFor(FName Column) const
{
    return SortColumn == Column ? SortMode : EColumnSortMode::None;
}

void STheNotesBrowser::Refresh()
{
    Rows.Reset();

    if(GEditor)
    {
        UTheNotesEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UTheNotesEditorSubsystem>();
        if(Subsystem)
        {
            const TArray<TheNotesBrowserLocal::FTerm> Terms = TheNotesBrowserLocal::ParseFilter(Filter);
            const FString& OpenLevel = Subsystem->GetTrackedLevel();

            for(const FTheNoteRecord& Record : Subsystem->CollectAllNotes())
            {
                if(bThisLevelOnly && Record.Level != OpenLevel)
                {
                    continue;
                }
                if(Terms.ContainsByPredicate([&Record](const TheNotesBrowserLocal::FTerm& Term) { return !Term.Matches(Record); }))
                {
                    continue;
                }
                Rows.Add(MakeShared<FTheNoteRecord>(Record));
            }
        }
    }

    // Unsorted means the order CollectAllNotes produced, which is author then collection then title —
    // a sensible list rather than the order the files happened to be read in.
    if(!SortColumn.IsNone())
    {
        const FName Column = SortColumn;
        const bool bAscending = SortMode != EColumnSortMode::Descending;
        Rows.Sort(
            [&Column, bAscending](const TSharedPtr<FTheNoteRecord>& A, const TSharedPtr<FTheNoteRecord>& B)
            {
                const FString Left = A.IsValid() ? TheNotesBrowserLocal::CellText(*A, Column) : FString();
                const FString Right = B.IsValid() ? TheNotesBrowserLocal::CellText(*B, Column) : FString();
                return bAscending ? Left < Right : Right < Left;
            });
    }

    if(ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

#undef LOCTEXT_NAMESPACE

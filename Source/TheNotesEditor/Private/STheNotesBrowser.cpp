// (c) 2026 Kentron Cowboys. All rights reserved.

#include "STheNotesBrowser.h"

#include "Editor.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#include "TheNotesEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "TheNotesBrowser"

namespace TheNotesBrowserColumns
{
static const FName Author("Author");
static const FName Collection("Collection");
static const FName Title("Title");
static const FName Level("Level");
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

bool Matches(const FTheNoteRecord& Record, const FString& Filter)
{
    if(Filter.IsEmpty())
    {
        return true;
    }
    return Record.Title.Contains(Filter) || Record.Body.Contains(Filter) || Record.Author.Contains(Filter) || Record.Collection.Contains(Filter) || Record.Level.Contains(Filter);
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
        FString Text;
        if(!Record.IsValid())
        {
            Text = FString();
        }
        else if(ColumnName == TheNotesBrowserColumns::Author)
        {
            Text = Record->Author;
        }
        else if(ColumnName == TheNotesBrowserColumns::Collection)
        {
            Text = Record->Collection;
        }
        else if(ColumnName == TheNotesBrowserColumns::Title)
        {
            Text = Record->Title;
        }
        else if(ColumnName == TheNotesBrowserColumns::Level)
        {
            Text = TheNotesBrowserLocal::ShortLevelName(Record->Level);
        }

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
                  [SNew(SHorizontalBox) + SHorizontalBox::Slot().FillWidth(1.0f)[SNew(SSearchBox).HintText(LOCTEXT("FilterHint", "Filter by title, text, author, collection or level")).OnTextChanged(this, &STheNotesBrowser::HandleFilterChanged)] +
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
                      .HeaderRow(SNew(SHeaderRow) + SHeaderRow::Column(TheNotesBrowserColumns::Author).DefaultLabel(LOCTEXT("AuthorColumn", "Author")).FillWidth(0.18f) +
                                 SHeaderRow::Column(TheNotesBrowserColumns::Collection).DefaultLabel(LOCTEXT("CollectionColumn", "Collection")).FillWidth(0.18f) +
                                 SHeaderRow::Column(TheNotesBrowserColumns::Title).DefaultLabel(LOCTEXT("TitleColumn", "Title")).FillWidth(0.46f) +
                                 SHeaderRow::Column(TheNotesBrowserColumns::Level).DefaultLabel(LOCTEXT("LevelColumn", "Level")).FillWidth(0.18f))]];

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

void STheNotesBrowser::Refresh()
{
    Rows.Reset();

    if(GEditor)
    {
        UTheNotesEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UTheNotesEditorSubsystem>();
        if(Subsystem)
        {
            for(const FTheNoteRecord& Record : Subsystem->CollectAllNotes())
            {
                if(TheNotesBrowserLocal::Matches(Record, Filter))
                {
                    Rows.Add(MakeShared<FTheNoteRecord>(Record));
                }
            }
        }
    }

    if(ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

#undef LOCTEXT_NAMESPACE

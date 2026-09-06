// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"

#include "TheNoteRecord.h"

/**
 * The DEV Notes list: every note in the project, whoever wrote it and whichever level it stands in.
 *
 * Notes in the open level are read from their actors and are therefore current to the frame; notes in
 * other levels come from their files, and can only be read, not focused or deleted — there is no world
 * to do either in.
 */
class STheNotesBrowser : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STheNotesBrowser) { }
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~STheNotesBrowser() override;

private:
    TSharedRef<class ITableRow> MakeRow(TSharedPtr<FTheNoteRecord> Item, const TSharedRef<class STableViewBase>& OwnerTable);
    void HandleRowActivated(TSharedPtr<FTheNoteRecord> Item);
    void HandleFilterChanged(const FText& Text);
    FReply HandleReloadClicked();
    FReply HandleDeleteClicked();
    bool CanDelete() const;
    void HandleThisLevelChanged(ECheckBoxState State);
    void HandleSort(EColumnSortPriority::Type Priority, const FName& Column, EColumnSortMode::Type Mode);
    EColumnSortMode::Type SortModeFor(FName Column) const;
    void Refresh();

    /** The rows as they are shown: filtered, then ordered by the column the header is sorted on. */
    TArray<TSharedPtr<FTheNoteRecord>> Rows;
    TSharedPtr<SListView<TSharedPtr<FTheNoteRecord>>> ListView;

    /** What was typed into the search box, before it is parsed into terms. */
    FString Filter;

    /** Narrow the list to the level the editor has open — the usual question while working in one. */
    bool bThisLevelOnly = false;

    FName SortColumn;
    EColumnSortMode::Type SortMode = EColumnSortMode::Ascending;

    FDelegateHandle NotesChangedHandle;
};

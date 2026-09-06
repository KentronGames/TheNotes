// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

#include "TheNoteRecord.h"

/**
 * The DEV Notes list: every note in the project, whoever wrote it and whichever level it stands in.
 *
 * Notes in the open level are read from their actors and are therefore current to the frame; notes
 * in other levels come from their files, and can only be read, not focused — there is no camera to
 * move to a level that is not open.
 */
class STheNotesBrowser : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STheNotesBrowser)
    {
    }
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~STheNotesBrowser() override;

private:
    TSharedRef<class ITableRow> MakeRow(TSharedPtr<FTheNoteRecord> Item, const TSharedRef<class STableViewBase>& OwnerTable);
    void HandleRowActivated(TSharedPtr<FTheNoteRecord> Item);
    void HandleFilterChanged(const FText& Text);
    void Refresh();

    TArray<TSharedPtr<FTheNoteRecord>> Rows;
    TSharedPtr<SListView<TSharedPtr<FTheNoteRecord>>> ListView;
    FString Filter;
    FDelegateHandle NotesChangedHandle;
};

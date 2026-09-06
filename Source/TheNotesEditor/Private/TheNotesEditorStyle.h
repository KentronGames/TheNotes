// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

/** The plugin's icons: the note's class icon and thumbnail, and the DEV Notes tab icon. */
class FTheNotesEditorStyle
{
public:
    static void Register();
    static void Unregister();

    static FName StyleName();

private:
    static TSharedPtr<FSlateStyleSet> Instance;
};

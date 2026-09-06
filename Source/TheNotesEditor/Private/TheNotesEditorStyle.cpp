// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheNotesEditorStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"

TSharedPtr<FSlateStyleSet> FTheNotesEditorStyle::Instance;

FName FTheNotesEditorStyle::StyleName()
{
    static const FName Name("TheNotesEditorStyle");
    return Name;
}

void FTheNotesEditorStyle::Register()
{
    if(Instance.IsValid())
    {
        return;
    }

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("TheNotes"));
    if(!Plugin.IsValid())
    {
        return;
    }

    Instance = MakeShared<FSlateStyleSet>(StyleName());
    Instance->SetContentRoot(Plugin->GetBaseDir() / TEXT("Resources"));

    const FVector2D Icon16(16.0f, 16.0f);
    const FVector2D Icon64(64.0f, 64.0f);

    // The class icon key is ClassIcon.<class name without its prefix>: the editor builds it that
    // way when it looks for an icon, so a key spelled any other way is simply never asked for.
    Instance->Set("ClassIcon.TheNote", new FSlateVectorImageBrush(Instance->RootToContentDir(TEXT("Icons/TheNote_16"), TEXT(".svg")), Icon16));
    Instance->Set("ClassThumbnail.TheNote", new FSlateVectorImageBrush(Instance->RootToContentDir(TEXT("Icons/TheNote_64"), TEXT(".svg")), Icon64));
    Instance->Set("TheNotes.TabIcon", new FSlateVectorImageBrush(Instance->RootToContentDir(TEXT("Icons/TheNote_16"), TEXT(".svg")), Icon16));

    FSlateStyleRegistry::RegisterSlateStyle(*Instance);
}

void FTheNotesEditorStyle::Unregister()
{
    if(Instance.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*Instance);
        Instance.Reset();
    }
}

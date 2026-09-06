// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheNotesViewportLabels.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Editor.h"
#include "EngineFontServices.h"
#include "EngineUtils.h"
#include "HitProxies.h"
#include "SceneView.h"
#include "UnrealClient.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Fonts/FontCache.h"
#include "HAL/IConsoleManager.h"
#include "Styling/AppStyle.h"

#include "TheNote.h"
#include "TheNotesEditorSubsystem.h"
#include "TheNotesModule.h"
#include "TheNotesViewSettings.h"

namespace TheNotesViewportLabelsLocal
{
static TAutoConsoleVariable<int32> CVarDebugHover(TEXT("TheNotes.DebugHover"), 0, TEXT("Log what the cursor is over in the level viewport, and the size of the panel it draws."), ECVF_Default);

/** The editor's own fonts, so a note reads like the rest of the editor rather than like a debug print. */
FSlateFontInfo TitleFont()
{
    FSlateFontInfo Font = FAppStyle::Get().GetFontStyle("NormalFontBold");
    Font.Size = UTheNotesViewSettings::Get().TitleFontSize;
    return Font;
}

FSlateFontInfo BodyFont()
{
    FSlateFontInfo Font = FAppStyle::Get().GetFontStyle("NormalFont");
    Font.Size = UTheNotesViewSettings::Get().BodyFontSize;
    return Font;
}

FSlateFontInfo AuthorFont()
{
    FSlateFontInfo Font = FAppStyle::Get().GetFontStyle("NormalFontBold");
    Font.Size = UTheNotesViewSettings::Get().AuthorFontSize;
    return Font;
}

/**
 * Text shaped into glyphs, which is what the canvas can actually draw in an editor font.
 *
 * FCanvasTextItem taking an FSlateFontInfo looks like the way to use the editor fonts and is not
 * one: it keeps Cast<UFont>(FontInfo.FontObject) and refuses to draw when that is null, which it is
 * for every Starship font, because those carry a composite font and no UFont at all. The failure is
 * silent — HasValidText simply returns false — and the panel comes out as black nothing.
 *
 * The same sequence is what the panel is measured from, so the box always fits what is drawn in it.
 * Measuring through one service and drawing through another is what produced that empty panel.
 */
FShapedGlyphSequencePtr Shape(const FString& Text, const FSlateFontInfo& Font, float DPIScale)
{
    if(Text.IsEmpty() || !FEngineFontServices::IsInitialized())
    {
        return nullptr;
    }

    TSharedPtr<FSlateFontCache> Cache = FEngineFontServices::Get().GetFontCache();
    if(!Cache.IsValid())
    {
        return nullptr;
    }

    // Shaped at the canvas scale, so every measurement below is in the same device pixels the
    // backdrop and the border are drawn in.
    return Cache->ShapeUnidirectionalText(Text, Font, DPIScale, TextBiDi::ETextDirection::LeftToRight, ETextShapingMethod::Auto);
}

float ShapedWidth(const FShapedGlyphSequencePtr& Sequence)
{
    return Sequence.IsValid() ? static_cast<float>(Sequence->GetMeasuredWidth()) : 0.0f;
}

float ShapedHeight(const FShapedGlyphSequencePtr& Sequence)
{
    return Sequence.IsValid() ? static_cast<float>(Sequence->GetMaxTextHeight()) : 0.0f;
}

/** Splits one authored line into lines that fit the column, breaking on spaces where it can. */
void SplitToWidth(const FString& Authored, const FSlateFontInfo& Font, float DPIScale, TArray<FString>& OutLines)
{
    const float Column = UTheNotesViewSettings::Get().MaxTextWidth * DPIScale;

    FString Line = Authored;
    Line.ReplaceInline(TEXT("\r"), TEXT(""));

    while(!Line.IsEmpty())
    {
        // Measured, not counted in characters: at this font a line of Cyrillic and a line of Latin
        // of the same length are nowhere near the same width on screen.
        if(ShapedWidth(Shape(Line, Font, DPIScale)) <= Column)
        {
            OutLines.Add(Line);
            return;
        }

        int32 Fits = 0;
        for(int32 Index = 1; Index <= Line.Len(); ++Index)
        {
            if(ShapedWidth(Shape(Line.Left(Index), Font, DPIScale)) > Column)
            {
                break;
            }
            Fits = Index;
        }
        if(Fits <= 0)
        {
            OutLines.Add(Line);
            return;
        }

        int32 Break = INDEX_NONE;
        for(int32 Index = Fits; Index > 0; --Index)
        {
            if(FChar::IsWhitespace(Line[Index - 1]))
            {
                Break = Index;
                break;
            }
        }

        // A run with no space in it that is still too wide has to be cut somewhere, so cut it at
        // the last character that fits rather than let the panel grow off the screen.
        if(Break == INDEX_NONE)
        {
            Break = Fits;
        }

        OutLines.Add(Line.Left(Break).TrimEnd());
        Line = Line.RightChop(Break).TrimStart();
    }
}

/** Splits a body into drawable lines: the newlines the author typed, wrapped to a readable column. */
TArray<FString> LayOutBody(const FString& Body, const FSlateFontInfo& Font, float DPIScale)
{
    TArray<FString> Lines;
    TArray<FString> Authored;
    Body.ParseIntoArray(Authored, TEXT("\n"), false);

    for(const FString& Line : Authored)
    {
        if(Line.IsEmpty())
        {
            Lines.Add(Line);
            continue;
        }
        SplitToWidth(Line, Font, DPIScale, Lines);
    }
    return Lines;
}

FVector2D DrawOrMeasurePanel(UCanvas* Canvas, const FString& Title, const TArray<FString>& Body, const FString& Author, const FVector2D& TopCentre);

/**
 * The panel: a ground inside a border, the title above the note text, and the author along the
 * bottom right. TopCentre is where the panel hangs from — its top edge, centred horizontally.
 * Spacing, colours and font sizes come from the view settings; nothing here is fixed.
 *
 * Sized to its contents rather than to a fixed rectangle: this is a hover panel in a working
 * viewport, and a panel of constant size would cover most of it whatever the note says.
 */
FVector2D DrawPanel(UCanvas* Canvas, const FString& Title, const TArray<FString>& Body, const FString& Author, const FVector2D& TopCentre)
{
    return DrawOrMeasurePanel(Canvas, Title, Body, Author, TopCentre);
}

/**
 * A null canvas measures without drawing, and that is the point: the size a test can check is
 * produced by the same code that lays the text out, so the two cannot drift apart. They did once —
 * measured through the Slate measure service, drawn through a canvas item that refused the font.
 */
FVector2D DrawOrMeasurePanel(UCanvas* Canvas, const FString& Title, const TArray<FString>& Body, const FString& Author, const FVector2D& TopCentre)
{
    // Text items multiply their position by the canvas DPI scale and tiles do not
    // (CanvasItem.cpp:881), so everything here is laid out in device pixels and only the text
    // positions are divided back down on the way in. Mixing the two is what put the panel and its
    // words in different places on screen.
    const UTheNotesViewSettings& View = UTheNotesViewSettings::Get();

    const float DPI = Canvas ? Canvas->GetDPIScale() : 1.0f;
    const float Pad = View.Padding * DPI;
    const float TitleSpacing = View.TitleGap * DPI;
    const float AuthorSpacing = View.AuthorGap * DPI;
    const float LineSpacing = View.LineGap * DPI;

    const FShapedGlyphSequencePtr TitleGlyphs = Shape(Title, TitleFont(), DPI);
    const FShapedGlyphSequencePtr AuthorGlyphs = Shape(Author, AuthorFont(), DPI);

    TArray<FShapedGlyphSequencePtr> BodyGlyphs;
    BodyGlyphs.Reserve(Body.Num());
    for(const FString& Line : Body)
    {
        BodyGlyphs.Add(Shape(Line, BodyFont(), DPI));
    }

    const float TitleHeight = ShapedHeight(TitleGlyphs);
    const float AuthorHeight = ShapedHeight(AuthorGlyphs);

    float BodyLineHeight = 0.0f;
    float Widest = FMath::Max(ShapedWidth(TitleGlyphs), ShapedWidth(AuthorGlyphs));
    for(const FShapedGlyphSequencePtr& Glyphs : BodyGlyphs)
    {
        Widest = FMath::Max(Widest, ShapedWidth(Glyphs));
        BodyLineHeight = FMath::Max(BodyLineHeight, ShapedHeight(Glyphs));
    }
    BodyLineHeight += LineSpacing;

    float Height = TitleHeight;
    if(BodyGlyphs.Num() > 0)
    {
        Height += TitleSpacing + BodyLineHeight * BodyGlyphs.Num();
    }
    if(AuthorHeight > 0.0f)
    {
        Height += AuthorSpacing + AuthorHeight;
    }

    // Nothing measured means nothing shaped, and a panel drawn around no text is the black
    // rectangle this whole path exists to avoid.
    if(Widest <= 0.0f || Height <= 0.0f)
    {
        return FVector2D::ZeroVector;
    }

    const FVector2D Size(Widest + Pad * 2.0f, Height + Pad * 2.0f);
    if(!Canvas)
    {
        return Size;
    }

    const FVector2D TopLeft(TopCentre.X - Size.X * 0.5f, TopCentre.Y);

    // Opaque unless the setting says otherwise: over a bright floor a translucent panel leaves the
    // text barely readable, so the alpha decides the blend rather than the blend being fixed.
    FCanvasTileItem Backdrop(TopLeft, Size, View.BackdropColour);
    Backdrop.BlendMode = View.BackdropColour.A >= 1.0f ? SE_BLEND_Opaque : SE_BLEND_Translucent;
    Canvas->DrawItem(Backdrop);

    if(View.BorderThickness > 0.0f)
    {
        FCanvasBoxItem Border(TopLeft, Size);
        Border.SetColor(View.BorderColour);
        Border.LineThickness = View.BorderThickness;
        Canvas->DrawItem(Border);
    }

    float Y = static_cast<float>(TopLeft.Y) + Pad;

    if(TitleGlyphs.IsValid())
    {
        FCanvasShapedTextItem Item(FVector2D(TopLeft.X + Pad, Y) / DPI, TitleGlyphs.ToSharedRef(), View.TitleColour);
        Canvas->DrawItem(Item);
    }
    Y += TitleHeight;

    if(BodyGlyphs.Num() > 0)
    {
        Y += TitleSpacing;
        for(const FShapedGlyphSequencePtr& Glyphs : BodyGlyphs)
        {
            if(Glyphs.IsValid())
            {
                FCanvasShapedTextItem Item(FVector2D(TopLeft.X + Pad, Y) / DPI, Glyphs.ToSharedRef(), View.BodyColour);
                Canvas->DrawItem(Item);
            }
            Y += BodyLineHeight;
        }
        Y -= LineSpacing;
    }

    if(AuthorGlyphs.IsValid())
    {
        // Bottom right: the signature belongs to the whole note, not to the line it happens to sit
        // next to.
        Y += AuthorSpacing;
        const float X = static_cast<float>(TopLeft.X) + Size.X - Pad - ShapedWidth(AuthorGlyphs);
        FCanvasShapedTextItem Item(FVector2D(X, Y) / DPI, AuthorGlyphs.ToSharedRef(), View.AuthorColour);
        Canvas->DrawItem(Item);
    }

    return Size;
}
}

void FTheNotesViewportLabels::Register()
{
    if(!DrawHandle.IsValid())
    {
        DrawHandle = UDebugDrawService::Register(TEXT("Editor"), FDebugDrawDelegate::CreateRaw(this, &FTheNotesViewportLabels::Draw));
    }
    if(!TickerHandle.IsValid())
    {
        TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FTheNotesViewportLabels::Tick));
    }
}

void FTheNotesViewportLabels::Unregister()
{
    if(DrawHandle.IsValid())
    {
        UDebugDrawService::Unregister(DrawHandle);
        DrawHandle.Reset();
    }
    if(TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }
}

bool FTheNotesViewportLabels::Tick(float DeltaTime)
{
    using namespace TheNotesViewportLabelsLocal;

    FViewport* Viewport = GEditor ? GEditor->GetActiveViewport() : nullptr;
    if(!Viewport)
    {
        HoveredNote.Invalidate();
        return true;
    }

    // Play-in-editor makes the GAME viewport the active one, and a game viewport never renders hit
    // proxies — its proxy render target is never allocated. Asking one for a hit proxy therefore
    // builds a render pass with a null colour target and trips check(ColorRT) on the render thread,
    // taking the whole editor down. Compare against the PIE viewport rather than testing PlayWorld:
    // Simulate also has a play world, but keeps the level viewport active, and labels work fine there.
    if(Viewport == GEditor->GetPIEViewport())
    {
        HoveredNote.Invalidate();
        return true;
    }

    const FIntPoint Cursor(Viewport->GetMouseX(), Viewport->GetMouseY());
    const bool bDebug = CVarDebugHover.GetValueOnGameThread() != 0;
    if(Cursor == LastCursor && !bDebug)
    {
        return true;
    }
    LastCursor = Cursor;
    LastViewportSize = Viewport->GetSizeXY();

    // Outside the viewport the engine parks the cursor at (-1, -1). Asking for a hit proxy there is
    // both meaningless and the one case that would keep a stale note lit while the mouse is
    // somewhere else entirely.
    if(Cursor.X < 0 || Cursor.Y < 0)
    {
        HoveredNote.Invalidate();
        return true;
    }

    // The same query the editor already makes on every mouse move to decide what a click would
    // select (EditorViewportClient.cpp:6232), so the proxy map is warm and this rides on it.
    HHitProxy* Proxy = Viewport->GetHitProxy(Cursor.X, Cursor.Y);
    const ATheNote* Note = nullptr;
    if(Proxy && Proxy->IsA(HActor::StaticGetType()))
    {
        Note = Cast<ATheNote>(static_cast<HActor*>(Proxy)->Actor);
    }

    HoveredNote = Note ? Note->Record.Id : FGuid();

    if(bDebug)
    {
        UE_LOG(LogTheNotes, Log, TEXT("hover: cursor=(%d,%d) viewport=%dx%d proxy=%s note=%s"), Cursor.X, Cursor.Y, LastViewportSize.X, LastViewportSize.Y, Proxy ? Proxy->GetType()->GetName() : TEXT("none"), Note ? *Note->Record.Title : TEXT("none"));
    }
    return true;
}

void FTheNotesViewportLabels::Draw(UCanvas* Canvas, APlayerController* Controller)
{
    using namespace TheNotesViewportLabelsLocal;

    if(!Canvas || !GEditor)
    {
        return;
    }

    // Three ways a panel comes to be drawn, and the cheap exit is that none of them holds.
    if(!HoveredNote.IsValid() && !PinnedNote.IsValid() && !bShowAll)
    {
        return;
    }

    // Which viewport this canvas is drawing into. FViewport is an FRenderTarget and the view family
    // carries the one it renders to, so this is an identity test rather than a guess from sizes —
    // two viewports in a split layout are the same size, and the cursor is only ever in one.
    FViewport* Hovered = GEditor->GetActiveViewport();
    const bool bCursorIsHere = Hovered && Canvas->SceneView && Canvas->SceneView->Family && Canvas->SceneView->Family->RenderTarget == static_cast<const FRenderTarget*>(Hovered);
    if(!bCursorIsHere)
    {
        return;
    }

    UTheNotesEditorSubsystem* Subsystem = GEditor->GetEditorSubsystem<UTheNotesEditorSubsystem>();
    if(!Subsystem)
    {
        return;
    }

    const UTheNotesViewSettings& View = UTheNotesViewSettings::Get();

    // Held in a local: GetLevelNotes returns by value, and a pointer into the temporary would
    // dangle the moment the full expression ended.
    const TArray<ATheNote*> Notes = Subsystem->GetLevelNotes();

    for(const ATheNote* Note : Notes)
    {
        const bool bOpened = Note->Record.Id == HoveredNote || Note->Record.Id == PinnedNote;
        if(!bOpened && !bShowAll)
        {
            continue;
        }

        // The panel hangs off the note, not off the cursor: it is the label of that note, and a panel
        // that follows the mouse reads as a tooltip for the viewport rather than for the thing in it.
        const FVector Anchor = Canvas->Project(Note->GetActorLocation(), false);
        if(Anchor.Z <= 0.0f)
        {
            continue;
        }

        // Caps are a setting, and either way the stored text is untouched: this is how a note is shown,
        // not what it says.
        const FString Authored = Note->Record.Title.IsEmpty() ? FString(TEXT("DEV Note")) : Note->Record.Title;
        const FString Title = View.bUppercaseTitle ? Authored.ToUpper() : Authored;
        const FString Author = View.bUppercaseAuthor ? Note->Record.Author.ToUpper() : Note->Record.Author;

        // A note shown only because everything is shown gets its title and nothing else. The body is
        // what makes one panel worth reading and a dozen of them a wall in front of the level.
        const TArray<FString> Body = bOpened ? LayOutBody(Note->Record.Body, BodyFont(), Canvas->GetDPIScale()) : TArray<FString>();

        const FVector2D Size = DrawPanel(Canvas, Title, Body, bOpened ? Author : FString(), FVector2D(Anchor.X, Anchor.Y + View.PanelDrop * Canvas->GetDPIScale()));

        if(CVarDebugHover.GetValueOnGameThread() != 0)
        {
            UE_LOG(LogTheNotes, Log, TEXT("panel: %.0fx%.0f at (%.0f,%.0f) canvas=%dx%d dpi=%.2f"), Size.X, Size.Y, Anchor.X, Anchor.Y, Canvas->SizeX, Canvas->SizeY, Canvas->GetDPIScale());
        }
    }
}

void FTheNotesViewportLabels::TogglePinnedToHovered()
{
    // Pinning what is already pinned unpins it, and so does pinning nothing: one command, and the way
    // out of it is the same gesture that got in.
    PinnedNote = (HoveredNote.IsValid() && HoveredNote != PinnedNote) ? HoveredNote : FGuid();
}

FVector2D FTheNotesViewportLabels::MeasurePanel(const FString& Title, const TArray<FString>& Body, const FString& Author)
{
    using namespace TheNotesViewportLabelsLocal;
    const UTheNotesViewSettings& View = UTheNotesViewSettings::Get();
    return DrawOrMeasurePanel(nullptr, View.bUppercaseTitle ? Title.ToUpper() : Title, Body, View.bUppercaseAuthor ? Author.ToUpper() : Author, FVector2D::ZeroVector);
}

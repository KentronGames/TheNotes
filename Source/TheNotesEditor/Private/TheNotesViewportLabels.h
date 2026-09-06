// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

/**
 * Shows a note's title and text over the viewport while the cursor is on it.
 *
 * Nothing is drawn otherwise (owner, 2026-09-05): the icon alone says a note is there, and a scene
 * with a dozen notes in it stays readable. The panel hangs off the note's own projected position —
 * it is that note's label, and one that follows the mouse reads as a tooltip for the viewport
 * rather than for the thing in it (owner, 2026-09-05, on seeing it at the cursor).
 *
 * The panel follows the owner's layout (2026-09-05): a black ground inside a white border, the
 * title in orange caps, the text in white, the author in green caps along the bottom right. Text is
 * drawn in the editor's own Slate fonts and measured through the Slate font measure service, so a
 * note reads like the rest of the editor and wrapping is by width, not by counting characters.
 *
 * The engine hands every editor viewport a canvas and its scene view once a frame through
 * UDebugDrawService, under the "Editor" show flag — set for ESFIM_Editor and nothing else.
 *
 * Hover is the hit proxy under the cursor, the same thing that decides what a click would select.
 * Two earlier answers were wrong: FLevelEditorViewportClient::HoveredObjects is empty unless
 * bEnableViewportHoverFeedback is on and it ships False, and a distance test against the note's
 * projected point makes a target of a few pixels on a sprite of a few hundred.
 */
class FTheNotesViewportLabels
{
public:
    void Register();
    void Unregister();

    /**
     * The size the panel would take for this content, laid out by the very code that draws it.
     *
     * Exposed because it is the half of this that a headless run can check: -nullrhi draws nothing,
     * but it can still shape text, and a panel measured in the wrong units is exactly the defect
     * that shipped a screen-filling black rectangle.
     */
    static FVector2D MeasurePanel(const FString& Title, const TArray<FString>& Body, const FString& Author);

private:
    void Draw(class UCanvas* Canvas, class APlayerController* Controller);
    bool Tick(float DeltaTime);

    /** Identity of the note under the cursor, invalid when there is none. */
    FGuid HoveredNote;

    /** Where the cursor was when that was last worked out, so a still mouse costs nothing. */
    FIntPoint LastCursor = FIntPoint(-1, -1);

    /** Size of the viewport those coordinates belong to, for the canvas conversion. */
    FIntPoint LastViewportSize = FIntPoint(0, 0);

    FDelegateHandle DrawHandle;
    FTSTicker::FDelegateHandle TickerHandle;
};

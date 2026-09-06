// (c) 2026 Kentron Cowboys. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "TheNotesViewSettings.generated.h"

/**
 * How a note's panel looks to one developer on one machine.
 *
 * Per user rather than per project, and deliberately: the panel is read over whatever the viewport
 * happens to be showing, so the contrast that works against a bright blockout is not the one that
 * works against a night scene, and a toolbar-sized preference must never dirty a file the team
 * shares. Where the note is KEPT is a project decision and lives in the other settings object.
 *
 * Every value here has a default that works before anyone opens this page.
 */
UCLASS(config = EditorPerProjectUserSettings, meta = (DisplayName = "The Notes (View)"))
class THENOTESEDITOR_API UTheNotesViewSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UTheNotesViewSettings();

    /** Room between the panel's border and its text, in points before the display scale is applied. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Spacing", meta = (ClampMin = "0.0", ClampMax = "80.0"))
    float Padding = 18.0f;

    /** Space between the title and the first line of the note. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Spacing", meta = (ClampMin = "0.0", ClampMax = "80.0"))
    float TitleGap = 16.0f;

    /** Space between the last line of the note and the author's signature. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Spacing", meta = (ClampMin = "0.0", ClampMax = "80.0"))
    float AuthorGap = 14.0f;

    /** Extra space between two lines of the note, on top of what the font already leaves. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Spacing", meta = (ClampMin = "0.0", ClampMax = "40.0"))
    float LineGap = 3.0f;

    /** How far below the note's own point the panel starts, so the icon is not covered by its label. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Spacing", meta = (ClampMin = "0.0", ClampMax = "200.0"))
    float PanelDrop = 22.0f;

    /**
     * How wide the text is allowed to run before it wraps. The panel is sized to its contents, so
     * this is the only thing standing between a long note and a panel that covers the viewport.
     * A high-density display usually wants this larger, which is why it is a setting and not a rule.
     */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Spacing", meta = (ClampMin = "80.0", ClampMax = "2000.0"))
    float MaxTextWidth = 420.0f;

    /** The note's title. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Colour")
    FLinearColor TitleColour = FLinearColor(0.99f, 0.59f, 0.12f, 1.0f);

    /** The note's text. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Colour")
    FLinearColor BodyColour = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

    /** The signature along the bottom right. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Colour")
    FLinearColor AuthorColour = FLinearColor(0.44f, 0.88f, 0.41f, 1.0f);

    /**
     * The ground the text is drawn on. Opaque by default and not by accident: over a bright floor a
     * translucent panel leaves the text barely readable. An alpha below one restores that.
     */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Colour")
    FLinearColor BackdropColour = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

    /** The line around the panel. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Colour")
    FLinearColor BorderColour = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

    /** Thickness of that line. Zero draws no border. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Colour", meta = (ClampMin = "0.0", ClampMax = "16.0"))
    float BorderThickness = 2.0f;

    /** Size of the title. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Font", meta = (ClampMin = "6", ClampMax = "72"))
    int32 TitleFontSize = 18;

    /** Size of the note's text. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Font", meta = (ClampMin = "6", ClampMax = "72"))
    int32 BodyFontSize = 11;

    /** Size of the signature. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Font", meta = (ClampMin = "6", ClampMax = "72"))
    int32 AuthorFontSize = 14;

    /** Draw the title in capitals. What the note SAYS is untouched either way. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Font")
    bool bUppercaseTitle = true;

    /** Draw the signature in capitals. */
    UPROPERTY(EditAnywhere, config, Category = "Panel|Font")
    bool bUppercaseAuthor = true;

    /** The settings this editor is running with. */
    static const UTheNotesViewSettings& Get();
};

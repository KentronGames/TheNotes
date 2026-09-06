// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheNotesModule.h"

#include "ImageCore.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Interfaces/IPluginManager.h"

#include "TheNotesSettings.h"

DEFINE_LOG_CATEGORY(LogTheNotes);

namespace TheNotesModuleLocal
{
/**
 * Shifts the mark's amber to the configured colour, leaving its dark ring and glyph dark.
 *
 * A billboard cannot be tinted at draw time — its scene proxy fixes the colour at white for anything that
 * is not a light — so the colour has to be in the pixels, and the pixels are rebuilt when the setting
 * changes rather than multiplied by a material.
 *
 * The shift is by the DIFFERENCE from the amber the mark ships as, weighted by how bright the pixel is
 * relative to that amber. Three properties fall out of that and all three are wanted: the default tint is
 * the shipped amber, so the difference is zero and no pixel moves; a fully lit pixel lands exactly on the
 * chosen colour; and the near-black ring, at a sixth of that brightness, moves a sixth as far and stays a
 * dark ring instead of turning into a bright one. The anti-aliased edge interpolates because the weight
 * does, so no band appears where the disc meets the ring.
 */
void ShiftAmberTo(const FImageView& Image, const FColor& Tint)
{
    const FColor Base = UTheNotesSettings::DefaultIconTint();
    const float BaseLuma = 0.299f * Base.R + 0.587f * Base.G + 0.114f * Base.B;
    if(BaseLuma <= 0.0f)
    {
        return;
    }

    const float DeltaR = static_cast<float>(Tint.R) - Base.R;
    const float DeltaG = static_cast<float>(Tint.G) - Base.G;
    const float DeltaB = static_cast<float>(Tint.B) - Base.B;

    for(FColor& Pixel : Image.AsBGRA8())
    {
        const float Luma = 0.299f * Pixel.R + 0.587f * Pixel.G + 0.114f * Pixel.B;
        const float Weight = FMath::Clamp(Luma / BaseLuma, 0.0f, 1.0f);
        Pixel.R = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Pixel.R + DeltaR * Weight), 0, 255));
        Pixel.G = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Pixel.G + DeltaG * Weight), 0, 255));
        Pixel.B = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Pixel.B + DeltaB * Weight), 0, 255));
    }
}
}

void FTheNotesModule::StartupModule()
{
}

void FTheNotesModule::ShutdownModule()
{
    Sprites.Reset();
}

UTexture2D* FTheNotesModule::SpriteTinted(const FColor& Tint)
{
    FTheNotesModule* Module = FModuleManager::GetModulePtr<FTheNotesModule>(TEXT("TheNotes"));
    if(!Module)
    {
        return nullptr;
    }

    if(const TStrongObjectPtr<UTexture2D>* Held = Module->Sprites.Find(Tint.ToPackedARGB()))
    {
        if(Held->IsValid())
        {
            return Held->Get();
        }
    }

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("TheNotes"));
    if(!Plugin.IsValid())
    {
        return nullptr;
    }

    const FString File = Plugin->GetBaseDir() / TEXT("Resources/Icons/T_DevNote.png");
    FImage Image;
    if(!FImageUtils::LoadImage(*File, Image))
    {
        // Silence here would look exactly like a note that failed to spawn, so say which file
        // was missing rather than leaving an unmarked point in space.
        UE_LOG(LogTheNotes, Warning, TEXT("Cannot read the note sprite at %s; notes will have no icon"), *File);
        return nullptr;
    }

    // The shift is arithmetic on the bytes a colour picker shows, so the image is put in that form
    // first — whatever the PNG happened to be decoded as.
    Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    TheNotesModuleLocal::ShiftAmberTo(Image, Tint);

    TStrongObjectPtr<UTexture2D> Built(FImageUtils::CreateTexture2DFromImage(Image));
    if(!Built.IsValid())
    {
        UE_LOG(LogTheNotes, Warning, TEXT("Cannot build the note sprite from %s; notes will have no icon"), *File);
        return nullptr;
    }

    return Module->Sprites.Add(Tint.ToPackedARGB(), MoveTemp(Built)).Get();
}

IMPLEMENT_MODULE(FTheNotesModule, TheNotes)

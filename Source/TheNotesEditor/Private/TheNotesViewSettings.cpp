// (c) 2026 Kentron Cowboys. All rights reserved.

#include "TheNotesViewSettings.h"

UTheNotesViewSettings::UTheNotesViewSettings()
{
    CategoryName = TEXT("Plugins");
}

const UTheNotesViewSettings& UTheNotesViewSettings::Get()
{
    const UTheNotesViewSettings* Settings = GetDefault<UTheNotesViewSettings>();
    check(Settings);
    return *Settings;
}

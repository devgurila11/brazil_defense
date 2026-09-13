// Brazil Defense. Configuration of the interface layer: maps, timings and the audio it drives.

#include "UI/BDUISettings.h"

UBDUISettings::UBDUISettings()
{
	CategoryName = TEXT("Game");
}

const UBDUISettings& UBDUISettings::Get()
{
	const UBDUISettings* Settings = GetDefault<UBDUISettings>();
	check(Settings);
	return *Settings;
}

// Brazil Defense. Configuration of the music, the ambience and the sounds of the buses.

#include "Audio/BDAudioSettings.h"

UBDAudioSettings::UBDAudioSettings()
{
	CategoryName = TEXT("Game");
}

const UBDAudioSettings& UBDAudioSettings::Get()
{
	const UBDAudioSettings* Settings = GetDefault<UBDAudioSettings>();
	check(Settings);
	return *Settings;
}

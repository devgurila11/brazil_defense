// Brazil Defense. How loud the horde is, and how much of it may be heard at once.

#include "Enemy/BDCreepSoundSettings.h"

UBDCreepSoundSettings::UBDCreepSoundSettings()
{
	// Shows up under Project Settings > Game, next to the balance settings.
	CategoryName = TEXT("Game");
}

const UBDCreepSoundSettings& UBDCreepSoundSettings::Get()
{
	const UBDCreepSoundSettings* Settings = GetDefault<UBDCreepSoundSettings>();
	check(Settings);
	return *Settings;
}

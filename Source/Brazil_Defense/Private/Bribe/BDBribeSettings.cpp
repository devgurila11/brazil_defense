// Brazil Defense. The show the bribe puts on: the bag, the coins and the till.

#include "Bribe/BDBribeSettings.h"

UBDBribeSettings::UBDBribeSettings()
{
	// Shows up under Project Settings > Game, next to the balance settings.
	CategoryName = TEXT("Game");
}

const UBDBribeSettings& UBDBribeSettings::Get()
{
	const UBDBribeSettings* Settings = GetDefault<UBDBribeSettings>();
	check(Settings);
	return *Settings;
}

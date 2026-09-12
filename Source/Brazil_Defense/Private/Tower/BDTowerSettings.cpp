// Brazil Defense. Configuration of the towers.

#include "Tower/BDTowerSettings.h"

UBDTowerSettings::UBDTowerSettings()
{
	CategoryName = TEXT("Game");
}

const UBDTowerSettings& UBDTowerSettings::Get()
{
	const UBDTowerSettings* Settings = GetDefault<UBDTowerSettings>();
	check(Settings);
	return *Settings;
}

// Brazil Defense. Configuration of the placement gesture and its preview.

#include "Placement/BDPlacementSettings.h"

UBDPlacementSettings::UBDPlacementSettings()
{
	CategoryName = TEXT("Game");
}

const UBDPlacementSettings& UBDPlacementSettings::Get()
{
	const UBDPlacementSettings* Settings = GetDefault<UBDPlacementSettings>();
	check(Settings);
	return *Settings;
}

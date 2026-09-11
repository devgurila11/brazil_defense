// Brazil Defense. Rules the generated obstacle layout has to satisfy.

#include "Obstacle/BDObstacleSettings.h"

UBDObstacleSettings::UBDObstacleSettings()
{
	// Shows up under Project Settings > Game, next to the grid settings.
	CategoryName = TEXT("Game");
}

const UBDObstacleSettings& UBDObstacleSettings::Get()
{
	const UBDObstacleSettings* Settings = GetDefault<UBDObstacleSettings>();
	check(Settings);
	return *Settings;
}

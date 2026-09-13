// Brazil Defense. Configuration of the creeps.

#include "Wave/BDWaveSettings.h"

#include "Enemy/BDEnemyData.h"

UBDWaveSettings::UBDWaveSettings()
{
	CategoryName = TEXT("Game");
}

const UBDWaveSettings& UBDWaveSettings::Get()
{
	const UBDWaveSettings* Settings = GetDefault<UBDWaveSettings>();
	check(Settings);
	return *Settings;
}

const UBDEnemyData* UBDWaveSettings::ResolveWaveEnemy() const
{
	const UBDEnemyData* Data = WaveEnemy.LoadSynchronous();
	return Data != nullptr ? Data : DebugEnemy.LoadSynchronous();
}

FColor UBDWaveSettings::GetRouteColor(const int32 SpawnPointIndex) const
{
	if (RouteColors.Num() == 0)
	{
		return FColor::White;
	}

	return RouteColors[FMath::Max(0, SpawnPointIndex) % RouteColors.Num()];
}

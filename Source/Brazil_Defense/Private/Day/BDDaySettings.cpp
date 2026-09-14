// Brazil Defense. Default curves the day cycle runs on.

#include "Day/BDDaySettings.h"

UBDDaySettings::UBDDaySettings()
{
	// Shows up under Project Settings > Game, next to the grid settings.
	CategoryName = TEXT("Game");
}

const UBDDaySettings& UBDDaySettings::Get()
{
	const UBDDaySettings* Settings = GetDefault<UBDDaySettings>();
	check(Settings);
	return *Settings;
}

float UBDDaySettings::HourForAlpha(const float Alpha) const
{
	return FMath::Fmod(DawnHour + FMath::Clamp(Alpha, 0.0f, 1.0f) * 24.0f, 24.0f);
}

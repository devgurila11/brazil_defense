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

float UBDDaySettings::SkyAlphaForProgress(const float Progress) const
{
	// Where the night lies in alpha: from NightHour round to SunriseHour, counted from dawn.
	const auto AlphaForHour = [this](const float Hour) { return FMath::Frac((Hour - DawnHour) / 24.0f + 1.0f); };
	const float NightStart = AlphaForHour(NightHour);
	const float NightEnd = AlphaForHour(SunriseHour);
	const float NightSpan = NightEnd - NightStart;
	const float DaySpan = 1.0f - NightSpan;
	const float Share = FMath::Clamp(NightShare, 0.0f, 1.0f);
	const float P = FMath::Clamp(Progress, 0.0f, 1.0f);

	// A night that wraps past alpha 0, or a sky with no day left, is taken as it comes.
	if (NightSpan <= 0.0f || DaySpan <= KINDA_SMALL_NUMBER || Share >= 1.0f)
	{
		return P;
	}

	// The day, before and after the night, shares what the night does not take.
	const float DayRate = (1.0f - Share) / DaySpan;
	const float NightFrom = NightStart * DayRate;
	const float NightTo = NightFrom + Share;
	if (P < NightFrom)
	{
		return P / DayRate;
	}
	if (P < NightTo)
	{
		return NightStart + (P - NightFrom) / FMath::Max(Share, KINDA_SMALL_NUMBER) * NightSpan;
	}
	return FMath::Min(1.0f, NightEnd + (P - NightTo) / DayRate);
}

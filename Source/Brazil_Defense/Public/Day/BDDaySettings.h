// Brazil Defense. Default curves the day cycle runs on.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BDDaySettings.generated.h"

class UCurveFloat;
class UCurveLinearColor;

/**
 * The curves every day cycle falls back to.
 *
 * ABDMatchManager is spawned from C++ rather than placed, so its day cycle component has
 * no Blueprint defaults panel to hang assets off. These are that panel: set once in
 * Project Settings, saved to DefaultGame.ini, picked up by any cycle that was not given
 * curves of its own. A level that wants a different sky still overrides them on its own
 * placed match manager.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Day Cycle"))
class BRAZIL_DEFENSE_API UBDDaySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDDaySettings();

	static const UBDDaySettings& Get();

	/** Sun pitch in degrees, by alpha. */
	UPROPERTY(config, EditAnywhere, Category = "Curves")
	TSoftObjectPtr<UCurveFloat> SunPitchByAlpha;

	/** Sun brightness, by alpha. */
	UPROPERTY(config, EditAnywhere, Category = "Curves")
	TSoftObjectPtr<UCurveFloat> SunIntensityByAlpha;

	/** Sun colour, by alpha. */
	UPROPERTY(config, EditAnywhere, Category = "Curves")
	TSoftObjectPtr<UCurveLinearColor> SunColorByAlpha;

	/** Street light multiplier, 0 off to 1 fully lit, by alpha. */
	UPROPERTY(config, EditAnywhere, Category = "Curves")
	TSoftObjectPtr<UCurveFloat> StreetLightIntensityByAlpha;

	//~ The clock ----------------------------------------------------------------
	// Alpha 0 is DawnHour on a 24 hour clock and a full cycle is one day, so the HUD can
	// say a time and a phase. The phases are hour ranges, in order: sunrise from
	// SunriseHour, day from DayHour, sunset from SunsetHour, dusk from DuskHour, night
	// from NightHour round to SunriseHour. Line them up with the curves above.

	UPROPERTY(config, EditAnywhere, Category = "Clock", meta = (ClampMin = "0.0", ClampMax = "24.0", UIMin = "0.0", UIMax = "24.0"))
	float DawnHour = 6.0f;

	UPROPERTY(config, EditAnywhere, Category = "Clock", meta = (ClampMin = "0.0", ClampMax = "24.0", UIMin = "0.0", UIMax = "24.0"))
	float SunriseHour = 5.0f;

	UPROPERTY(config, EditAnywhere, Category = "Clock", meta = (ClampMin = "0.0", ClampMax = "24.0", UIMin = "0.0", UIMax = "24.0"))
	float DayHour = 8.0f;

	UPROPERTY(config, EditAnywhere, Category = "Clock", meta = (ClampMin = "0.0", ClampMax = "24.0", UIMin = "0.0", UIMax = "24.0"))
	float SunsetHour = 17.0f;

	UPROPERTY(config, EditAnywhere, Category = "Clock", meta = (ClampMin = "0.0", ClampMax = "24.0", UIMin = "0.0", UIMax = "24.0"))
	float DuskHour = 19.0f;

	UPROPERTY(config, EditAnywhere, Category = "Clock", meta = (ClampMin = "0.0", ClampMax = "24.0", UIMin = "0.0", UIMax = "24.0"))
	float NightHour = 20.5f;

	/** Seconds the HUD keeps the time up after the sun stops moving. */
	UPROPERTY(config, EditAnywhere, Category = "Clock", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float ClockNoticeSeconds = 3.0f;

	/** Hour of the day, 0 to 24, for a cycle alpha. */
	float HourForAlpha(float Alpha) const;
};

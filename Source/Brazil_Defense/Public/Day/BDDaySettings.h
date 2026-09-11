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
};

// Brazil Defense. Tooling: lay out the four sound classes the options panel drives.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BDSetupSoundClassesCommandlet.generated.h"

/**
 * Puts the sound classes of the interface settings in the one shape the options expect:
 * voice, music, environment and effects, four siblings under the master class, none of
 * them under another, so each slider and each mute owns its class alone. The environment
 * class is created when missing. The speech duck mix gets one adjuster for the effects and
 * one for the environment, children left alone, so the words step both back and nothing
 * else. The ambience slots of the audio settings go on the environment class. Safe to run
 * again: whatever is already in place is left as it is.
 *
 *   UnrealEditor-Cmd <uproject> -run=BDSetupSoundClasses [-DryRun]
 *
 * Every sound under /Game is listed with its class at the end, so what is routed where
 * can be read off the log. -DryRun only lists, and saves nothing.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDSetupSoundClassesCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UBDSetupSoundClassesCommandlet();

	//~ Begin UCommandlet interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet interface
};

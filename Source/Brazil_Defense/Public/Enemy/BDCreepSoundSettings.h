// Brazil Defense. How loud the horde is, and how much of it may be heard at once.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BDCreepSoundSettings.generated.h"

/**
 * The shared half of the creeps' sound: what every vocal creep obeys, whoever it is. The
 * other half, what a given creep says and how often, lives on its UBDEnemyData, so a new
 * vocal NPC is a data asset and never a line of code here.
 *
 * Two budgets, kept apart on purpose. Voices (speech and animal calls alike) share one
 * small budget over the whole board: fifty creeps each wanting to speak every few seconds
 * are a wall of words, three or four at a time are a crowd murmuring. Hooves are steps,
 * not voices, and get a budget of their own so a dense wave running never silences the
 * one creep talking.
 *
 * Everything is 3D at the creep, routed through the effects class, and attenuated against
 * the match camera, which is the listener: zoomed out over the whole board the horde is
 * barely there, down at street level it is right in the ear. Edited in Project Settings >
 * Game > Brazil Defense - Creep Sound.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Creep Sound"))
class BRAZIL_DEFENSE_API UBDCreepSoundSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDCreepSoundSettings();

	static const UBDCreepSoundSettings& Get();

	//~ Voices -----------------------------------------------------------------

	/**
	 * Most voices sounding at once over the whole board, every creep together. Past it the
	 * one farthest from the camera is cut for the new one, the oldest among equals. This is
	 * what keeps a dense wave a murmur.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Voices", meta = (ClampMin = "1", ClampMax = "16", UIMin = "1", UIMax = "16"))
	int32 VoiceMaxConcurrent = 4;

	UPROPERTY(config, EditAnywhere, Category = "Voices", meta = (ClampMin = "0.0", ClampMax = "4.0", UIMin = "0.0", UIMax = "4.0"))
	float VoiceVolume = 1.0f;

	/** Inside this radius from the camera a voice plays at full volume. */
	UPROPERTY(config, EditAnywhere, Category = "Voices", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float VoiceInnerRadius = 5000.0f;

	/**
	 * Past the inner radius it fades over this distance, down to VoiceAttenuationAtMax, and
	 * beyond it is not played at all. Inner plus falloff is the reach: 32000 leaves the
	 * horde a faint murmur from the camera fully zoomed out, which flies 28000 up.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Voices", meta = (ClampMin = "100.0", UIMin = "100.0", ForceUnits = "cm"))
	float VoiceFalloffDistance = 27000.0f;

	UPROPERTY(config, EditAnywhere, Category = "Voices", meta = (ClampMin = "-90.0", ClampMax = "0.0", UIMin = "-90.0", UIMax = "0.0"))
	float VoiceAttenuationAtMax = -48.0f;

	//~ Hooves -----------------------------------------------------------------

	/** Most steps sounding at once over the whole board. Past it the oldest is cut. */
	UPROPERTY(config, EditAnywhere, Category = "Hooves", meta = (ClampMin = "1", ClampMax = "32", UIMin = "1", UIMax = "32"))
	int32 FootstepMaxConcurrent = 10;

	UPROPERTY(config, EditAnywhere, Category = "Hooves", meta = (ClampMin = "0.0", ClampMax = "4.0", UIMin = "0.0", UIMax = "4.0"))
	float FootstepVolume = 0.6f;

	/** Inside this radius from the camera a step plays at full volume. Tighter than a voice: steps are for the close view. */
	UPROPERTY(config, EditAnywhere, Category = "Hooves", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float FootstepInnerRadius = 4000.0f;

	/** Past the inner radius a step fades over this distance, and beyond it is not played at all. */
	UPROPERTY(config, EditAnywhere, Category = "Hooves", meta = (ClampMin = "100.0", UIMin = "100.0", ForceUnits = "cm"))
	float FootstepFalloffDistance = 12000.0f;

	UPROPERTY(config, EditAnywhere, Category = "Hooves", meta = (ClampMin = "-90.0", ClampMax = "0.0", UIMin = "-90.0", UIMax = "0.0"))
	float FootstepAttenuationAtMax = -60.0f;
};

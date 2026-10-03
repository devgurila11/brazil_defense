// Brazil Defense. Configuration of the music, the ambience and the sounds of the buses.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BDAudioSettings.generated.h"

class USoundBase;

/**
 * Which sounds make the soundscape of a match, and how they are mixed. The slots are
 * plain lists: drop the waves in, the subsystems pick among them. Edited in Project
 * Settings > Game > Brazil Defense - Audio.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Audio"))
class BRAZIL_DEFENSE_API UBDAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDAudioSettings();

	static const UBDAudioSettings& Get();

	//~ Music ---------------------------------------------------------------------
	// One track at random while a wave is on, another a few seconds before it ends, never
	// the one just heard; between waves the music fades out and leaves the ambience.
	// Routed to the music class, so the options' music slider owns it.

	UPROPERTY(config, EditAnywhere, Category = "Music")
	TArray<TSoftObjectPtr<USoundBase>> BattleMusic;

	UPROPERTY(config, EditAnywhere, Category = "Music", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float MusicVolume = 0.8f;

	/** Seconds the music takes to come in when a wave starts. */
	UPROPERTY(config, EditAnywhere, Category = "Music", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MusicFadeIn = 2.5f;

	/** Seconds the music takes to go when the wave is over. */
	UPROPERTY(config, EditAnywhere, Category = "Music", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MusicFadeOut = 4.0f;

	/**
	 * Seconds before a track ends that the next one comes in over it. The tracks end on a
	 * fade of their own, so a plain loop would leave a hole: the next one covers it.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Music", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float MusicCrossfade = 5.0f;

	//~ Ambience ------------------------------------------------------------------
	// Always on during a match. Three layers, each chaining its short variations with a
	// crossfade between every two, so a clip of a second or two never repeats on a cut.
	// The day/night cycle sets their mix; under the music the whole bed goes down.

	UPROPERTY(config, EditAnywhere, Category = "Ambience")
	TArray<TSoftObjectPtr<USoundBase>> AmbienceCityDay;

	UPROPERTY(config, EditAnywhere, Category = "Ambience")
	TArray<TSoftObjectPtr<USoundBase>> AmbienceBirdsDay;

	UPROPERTY(config, EditAnywhere, Category = "Ambience")
	TArray<TSoftObjectPtr<USoundBase>> AmbienceNight;

	UPROPERTY(config, EditAnywhere, Category = "Ambience", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CityVolume = 0.5f;

	UPROPERTY(config, EditAnywhere, Category = "Ambience", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BirdsVolume = 0.45f;

	UPROPERTY(config, EditAnywhere, Category = "Ambience", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float NightVolume = 0.7f;

	/** Share of the city left at the dead of night: fewer cars, not none. */
	UPROPERTY(config, EditAnywhere, Category = "Ambience", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CityAtNight = 0.35f;

	/** The ambience's level while the music plays: present in the pause, under the music in the wave. */
	UPROPERTY(config, EditAnywhere, Category = "Ambience", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AmbienceUnderMusic = 0.5f;

	/** Seconds of overlap between two variations of a layer, cut down to a third of a short clip. */
	UPROPERTY(config, EditAnywhere, Category = "Ambience", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float AmbienceCrossfade = 0.4f;

	//~ Buses ---------------------------------------------------------------------
	// Heard from the bus itself, in 3D, only on the mouths the wave opened.

	/** The engine starting, on every open mouth's bus as the wave is dealt. A cue picks among the takes. */
	UPROPERTY(config, EditAnywhere, Category = "Buses")
	TSoftObjectPtr<USoundBase> BusEngineSound;

	/** The horn, on a bus the moment the first militant of the wave leaves it. */
	UPROPERTY(config, EditAnywhere, Category = "Buses")
	TSoftObjectPtr<USoundBase> BusHornSound;

	UPROPERTY(config, EditAnywhere, Category = "Buses", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BusEngineVolume = 0.9f;

	UPROPERTY(config, EditAnywhere, Category = "Buses", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float BusHornVolume = 0.8f;

	/**
	 * Seconds at least between the engine and the first militant out: the bus gets ready,
	 * then the horde leaves. Only asked when the wave is not already held that long (the
	 * buses pulling up, the candidate walking out).
	 */
	UPROPERTY(config, EditAnywhere, Category = "Buses", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float EngineLeadSeconds = 1.5f;

	/** Full volume up to this far from the camera. */
	UPROPERTY(config, EditAnywhere, Category = "Buses", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float BusInnerRadius = 6000.0f;

	/** Past the inner radius, it fades over this distance. The overview sits at some 33000 cm. */
	UPROPERTY(config, EditAnywhere, Category = "Buses", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float BusFalloffDistance = 40000.0f;

	/** How much of the bus sounds goes to the reverb: the open street, not a hall. */
	UPROPERTY(config, EditAnywhere, Category = "Buses", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BusReverbSend = 0.35f;
};

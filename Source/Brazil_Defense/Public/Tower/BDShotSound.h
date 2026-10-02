// Brazil Defense. The sound of a shot: one path for every defender that fires.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDShotSound.generated.h"

class USoundBase;
class USoundConcurrency;

/**
 * What every shot obeys, whoever fires it: how loud, how far it carries, and how many may
 * sound at once over the whole board. What a given weapon sounds like is not here: it is
 * the weapon's own FireSound (FBDAgentWeapon for the Agent; the towers and the platform
 * shooters will get the same field), a Sound Cue of two or three variations built with
 * the BDBuildSoundCue commandlet, which routes it through SC_Effects:
 *
 *   UnrealEditor-Cmd <uproject> -run=BDBuildSoundCue -Cue=/Game/BD/Audio/SCue_Pistola_Tiro
 *       -Folder=/Game/BD/Audio/Pistola -Pitch=0.05
 *
 * then point the weapon's FireSound at the cue. An empty FireSound fires in silence.
 * Edited in Project Settings > Game > Brazil Defense - Shot Sound.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Shot Sound"))
class BRAZIL_DEFENSE_API UBDShotSoundSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDShotSoundSettings();

	static const UBDShotSoundSettings& Get();

	/**
	 * Most shots sounding at once over the whole board, every defender together. Past it
	 * the one farthest from the camera is cut for the new one: a board full of defenders
	 * firing is a volley, not a wall.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Shots", meta = (ClampMin = "1", ClampMax = "32", UIMin = "1", UIMax = "32"))
	int32 ShotMaxConcurrent = 8;

	UPROPERTY(config, EditAnywhere, Category = "Shots", meta = (ClampMin = "0.0", ClampMax = "4.0", UIMin = "0.0", UIMax = "4.0"))
	float ShotVolume = 1.0f;

	/** Inside this radius from the camera a shot plays at full volume. */
	UPROPERTY(config, EditAnywhere, Category = "Shots", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float ShotInnerRadius = 8000.0f;

	/** Past the inner radius a shot fades over this distance, and beyond it is not played at all. */
	UPROPERTY(config, EditAnywhere, Category = "Shots", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float ShotFalloffDistance = 22000.0f;

	UPROPERTY(config, EditAnywhere, Category = "Shots", meta = (ClampMax = "0.0"))
	float ShotAttenuationAtMax = -40.0f;
};

/**
 * Plays the shots. A defender calls PlayShot at the instant it fires, with the sound of
 * its weapon; this keeps the budget, the attenuation and the effects class the same for
 * all of them. Counts what it was asked and what it played, for the regression.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDShotSoundSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * One shot at a point in the world.
	 * @param Sound the weapon's FireSound, resolved. Null is a silent shot: counted, nothing played.
	 * @return whether a sound was started.
	 */
	bool PlayShot(USoundBase* Sound, const FVector& Where);

	int32 GetShotsRequested() const { return ShotsRequested; }
	int32 GetShotsPlayed() const { return ShotsPlayed; }
	int32 GetShotsSilent() const { return ShotsSilent; }

private:
	UPROPERTY(Transient)
	TObjectPtr<USoundConcurrency> Concurrency;

	int32 ShotsRequested = 0;
	int32 ShotsPlayed = 0;
	int32 ShotsSilent = 0;
};

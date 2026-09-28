// Brazil Defense. Plays what the creeps say and the steps they take, under one budget.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDCreepSoundSubsystem.generated.h"

class ABDCreepCorpse;
class ABDEnemyBase;
class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/**
 * The one place a creep's sound goes through. A creep only decides when (its own timer
 * for a voice, the landing of a foot in its animation for a step); what is played, how
 * loud, how far it carries and whether there is room for it is decided here, with
 * UBDCreepSoundSettings.
 *
 * Nothing out of earshot of the camera is spawned at all: past the reach of its
 * attenuation a creep is silent, and a far creep never takes a place in the budget from
 * one the camera is looking at.
 *
 * Voices follow the creep, attached to it, since a sentence lasts long enough for a
 * running donkey to leave it behind. Steps are fire and forget at the spot: a hundred
 * creeps land a couple of feet a second each, and a component apiece would be waste.
 *
 * The voice budget is a sound concurrency shared by every creep, so it holds over the
 * whole board however many creeps are on it. The peak of voices sounding together is
 * kept for BD.Sound.Stats, which is how the "murmur, not a swarm" of a dense wave is
 * checked headless.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDCreepSoundSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBDCreepSoundSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem interface
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	//~ End USubsystem interface

	//~ Begin FTickableGameObject interface
	/** Samples the voices sounding, for the peak. */
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject interface

	/** The creep says something or makes its call, as its data shares them out. False when nothing was played. */
	bool PlayVocal(ABDEnemyBase& Creep);

	/** A foot of the creep landed. False when nothing was played. */
	bool PlayFootstep(const ABDEnemyBase& Creep);

	/** The creep was killed: its death cry, at the spot, under the death budget. False when nothing was played. */
	bool PlayDeath(const ABDEnemyBase& Creep);

	/** A body hit the ground: its thud, at the spot, under the fall budget. False when nothing was played. */
	bool PlayBodyFall(const ABDCreepCorpse& Corpse);

	/** Voices audible right now. Drops the finished ones as it counts. */
	int32 CountActiveVoices();

	/** Most voices audible together since the world started or the last ResetStats. */
	int32 GetPeakVoices() const { return PeakVoices; }

	/**
	 * Voices creeps came due for, those near enough the camera to be heard, and those the
	 * audio device started; then the same for steps. Out of range nothing is spawned.
	 */
	int32 GetVoicesRequested() const { return VoicesRequested; }
	int32 GetVoicesInRange() const { return VoicesInRange; }
	int32 GetVoicesStarted() const { return VoicesStarted; }
	int32 GetStepsRequested() const { return StepsRequested; }
	int32 GetStepsPlayed() const { return StepsPlayed; }
	int32 GetDeathsRequested() const { return DeathsRequested; }
	int32 GetDeathsPlayed() const { return DeathsPlayed; }
	int32 GetFallsRequested() const { return FallsRequested; }
	int32 GetFallsPlayed() const { return FallsPlayed; }

	/** Seconds spent with exactly N voices sounding, by N. Tells a real crowd from a frame of overlap. */
	const TArray<float>& GetSecondsAtCount() const { return SecondsAtCount; }

	void ResetStats();

private:
	/** Builds the concurrency and attenuation objects once, from the settings at that moment. */
	void EnsureObjects();

	UPROPERTY(Transient)
	TObjectPtr<USoundConcurrency> VoiceConcurrency;

	UPROPERTY(Transient)
	TObjectPtr<USoundConcurrency> FootstepConcurrency;

	UPROPERTY(Transient)
	TObjectPtr<USoundConcurrency> DeathConcurrency;

	UPROPERTY(Transient)
	TObjectPtr<USoundConcurrency> BodyFallConcurrency;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> VoiceAttenuation;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> FootstepAttenuation;

	/** Voices started and possibly still sounding, for the count. Weak: they destroy themselves when done. */
	TArray<TWeakObjectPtr<UAudioComponent>> Voices;

	TArray<float> SecondsAtCount;

	int32 PeakVoices = 0;
	int32 VoicesRequested = 0;
	int32 VoicesInRange = 0;
	int32 VoicesStarted = 0;
	int32 StepsRequested = 0;
	int32 StepsPlayed = 0;
	int32 DeathsRequested = 0;
	int32 DeathsPlayed = 0;
	int32 FallsRequested = 0;
	int32 FallsPlayed = 0;
};

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

	/** Why a kill did or did not become a line, for the log and the regression. */
	enum class ECelebration : uint8 { Said, NoBank, TooSoon, BoardBusy, Unlucky, HeldBack, Unheard };

	/**
	 * One of the player's people downed a creep: maybe he says a line from his own bank.
	 * Only by chance, after his own interval and the board's gap, and under the speech
	 * rule shared with the militants. Said, the blue exclamation goes up over him.
	 * @param Roll the chance's draw, 0..1; negative draws one here.
	 */
	ECelebration TryCelebrate(AActor& Speaker, const TSoftObjectPtr<USoundBase>& Bank, float Roll = -1.0f);

	/** The gates before a line is tried: his bank, his interval, the board's gap, the chance. Changes nothing. */
	ECelebration GateCelebration(const AActor& Speaker, const TSoftObjectPtr<USoundBase>& Bank, double Now, float Roll) const;

	/** Takes a line as said by Speaker at Now: his interval and the board's gap start from here. */
	void NoteCelebration(const AActor& Speaker, double Now);


	int32 GetCelebrationsAsked() const { return CelebrationsAsked; }
	int32 GetCelebrationsSaid() const { return CelebrationsSaid; }

	/** How the kills ended, by ECelebration: said, no bank, too soon, board busy, unlucky, held back, unheard. */
	int32 GetCelebrationOutcomes(ECelebration Outcome) const { return CelebrationOutcomes[static_cast<int32>(Outcome)]; }

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

	/** Sentences kept quiet by the speech rule: a full budget, or the side already talking. */
	int32 GetSpeechHeldBack() const { return SpeechHeldBack; }
	int32 GetPeakSentences() const { return PeakSentences; }
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
	/** TryCelebrate without the counting. */
	ECelebration TryCelebrateCounted(AActor& Speaker, const TSoftObjectPtr<USoundBase>& Bank, float Roll);

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

	/** The words: the voice falloff, panned hard by side. */
	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> SpeechAttenuation;

	/**
	 * Whether a sentence at Where may start now under the speech rule. When the budget is
	 * full and it is nearer the camera than the farthest sentence, that one is handed back
	 * in OutReplace to be stopped.
	 */
	bool CanSpeak(const FVector& Where, TWeakObjectPtr<UAudioComponent>& OutReplace);

	/** Sentences started and possibly still sounding. Also counted among Voices. */
	TArray<TWeakObjectPtr<UAudioComponent>> Sentences;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> FootstepAttenuation;

	/** Voices started and possibly still sounding, for the count. Weak: they destroy themselves when done. */
	TArray<TWeakObjectPtr<UAudioComponent>> Voices;

	TArray<float> SecondsAtCount;

	/** When each character last said a line, and when anyone did. */
	TMap<TWeakObjectPtr<const AActor>, double> LastCelebration;
	double LastCelebrationAny = -1.0e9;
	int32 CelebrationsAsked = 0;
	int32 CelebrationsSaid = 0;
	int32 CelebrationOutcomes[7] = { 0, 0, 0, 0, 0, 0, 0 };

	int32 PeakVoices = 0;
	int32 VoicesRequested = 0;
	int32 VoicesInRange = 0;
	int32 VoicesStarted = 0;
	int32 SpeechHeldBack = 0;
	int32 PeakSentences = 0;
	int32 StepsRequested = 0;
	int32 StepsPlayed = 0;
	int32 DeathsRequested = 0;
	int32 DeathsPlayed = 0;
	int32 FallsRequested = 0;
	int32 FallsPlayed = 0;
};

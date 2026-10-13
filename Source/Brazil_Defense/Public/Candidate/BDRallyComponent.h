// Brazil Defense. A candidate's rally: he stops, speaks untouchable, and a crowd rises around him.

#pragma once

#include "CoreMinimal.h"
#include "Candidate/BDCandidateData.h"
#include "Components/ActorComponent.h"
#include "BDRallyComponent.generated.h"

class ABDCandidate;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/** Where a candidate is in his rally. */
UENUM(BlueprintType)
enum class EBDRallyState : uint8
{
	/** On his way; the next rally counts down. */
	Walking,
	/** The warning: still walking, the red exclamation over him. */
	Tell,
	/** Stopped, speaking: nobody aims at him, nothing lands, the Mito does not see him, his are the only words. */
	Speaking,
	/** Stopped while the crowd rises out of the ground around him. He can be shot again. */
	Spawning,
};

/**
 * The rallies of one candidate (ABDCandidate), by the numbers on his record
 * (FBDRallyParams): RallyFirstDelay after he leaves the bus, then every RallyEvery
 * (drawn RallyEveryJitter either way) of walking,
 *
 *   Walking -> Tell (RallyTellSeconds, still walking) -> Speaking (stopped, untouchable,
 *   RallyDuration or the length of his cue) -> Spawning (RallyCount militants rise around
 *   him, RallyRiseSeconds) -> Walking.
 *
 * One rally on the board at a time: a second candidate due while another holds it waits for
 * him to finish before his own warning (UBDCandidateSubsystem::TryClaimRally). The clock is
 * the game's, so it runs faster at 2x and 4x and stops on the pause. Reaching the urn
 * during the warning is the defeat, as at any other time.
 *
 * The crowd: ordinary militants of RallyComposition (kinds 1..N for candidate N; a kind not
 * yet unlocked or without a body rises as a donkey, ResolveCrowdKinds), as tough as the creeps of the wave,
 * on the walkable cells nearest him - the free ones that lead to the urn, one each, ring by
 * ring outwards along the route both ways (UBDWaveSubsystem::SpawnEnemyAtCell). They join the
 * wave's creeps on the board and vote like them, but are not part of its count.
 */
UCLASS(ClassGroup = "Brazil Defense", meta = (BlueprintSpawnableComponent))
class BRAZIL_DEFENSE_API UBDRallyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBDRallyComponent();

	//~ Begin UActorComponent interface
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

	/**
	 * Arms the rallies with his numbers, as he walks out. Off when the master switch is off,
	 * when his numbers say so, or when he is one of the fallen come back and those do not rally.
	 */
	void Setup(const FBDRallyParams& InParams, bool bInEnabled, int32 Seed);

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Rally")
	EBDRallyState GetState() const { return State; }

	/** Untouchable: the speech. */
	bool IsImmune() const { return State == EBDRallyState::Speaking; }

	/** Stopped on the spot: the speech and the rise of the crowd. */
	bool IsStopped() const { return State == EBDRallyState::Speaking || State == EBDRallyState::Spawning; }

	bool IsEnabled() const { return bEnabled; }
	const FBDRallyParams& GetParams() const { return Params; }

	/** Seconds to the next warning while walking; seconds left of the current state otherwise. */
	float GetNextRallyIn() const { return NextRallyIn; }
	float GetStateRemaining() const { return StateRemaining; }

	/** How long the speech under way lasts (or the last one lasted). */
	float GetSpeechSeconds() const { return SpeechSeconds; }

	/** Rallies started (warnings), and of the last crowd: how many were asked for and how many rose. */
	int32 GetRalliesStarted() const { return RalliesStarted; }
	int32 GetLastAsked() const { return LastAsked; }
	int32 GetLastRaised() const { return LastRaised; }

	/** The militants of the last crowd, while they last. For the regression. */
	const TArray<TWeakObjectPtr<class ABDEnemyBase>>& GetLastCrowd() const { return LastCrowd; }

	/**
	 * Debug: the next rally is due now (BD.Candidate.Rally). It still waits its turn when
	 * another candidate holds the rally. @return false, with the reason, when he cannot rally.
	 */
	bool ForceRally(FString& OutWhy);

	/** "Walking", "Tell", "Speaking", "Spawning". */
	static const TCHAR* StateName(EBDRallyState InState);

	/**
	 * What a crowd is drawn among, by the same rule as the buses: each entry of the
	 * composition with its weight, as itself when its horde number is unlocked (<= Unlocked)
	 * and its DA has a body, as the Donkey otherwise. Empty only when nothing loads and there
	 * is no donkey either.
	 */
	static void ResolveCrowdKinds(const FBDRallyParams& InParams, int32 Unlocked, const UBDEnemyData* Donkey,
		TArray<const UBDEnemyData*>& OutKinds, TArray<float>& OutWeights);

	/** One index of Weights, drawn by weight off Stream. */
	static int32 PickByWeight(FRandomStream& InStream, const TArray<float>& Weights);

private:
	ABDCandidate* GetCandidate() const;

	void EnterTell();
	void EnterSpeaking();
	void EnterSpawning();
	void EnterWalking();

	/** The crowd: picked, placed and sent up. */
	void RaiseCrowd();

	/** Gives back whatever the rally holds: the turn, the floor, the disc. */
	void ReleaseAll();

	/** The red disc under him while he speaks. */
	void ShowAura(bool bShow);
	void UpdateAura(float DeltaTime);

	/** The seconds of walking to the next rally, drawn around RallyEvery. */
	float DrawInterval();

	FBDRallyParams Params;
	bool bEnabled = false;
	FRandomStream Stream;

	EBDRallyState State = EBDRallyState::Walking;
	float NextRallyIn = 0.0f;
	float StateRemaining = 0.0f;
	float SpeechSeconds = 0.0f;
	float AuraClock = 0.0f;

	int32 RalliesStarted = 0;
	int32 LastAsked = 0;
	int32 LastRaised = 0;
	TArray<TWeakObjectPtr<ABDEnemyBase>> LastCrowd;

	/** Whether this one holds the board's rally turn now, and the floor of the words. */
	bool bHoldsTurn = false;
	bool bHoldsFloor = false;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Aura;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> AuraMaterial;

	TWeakObjectPtr<UAudioComponent> SpeechAudio;
};

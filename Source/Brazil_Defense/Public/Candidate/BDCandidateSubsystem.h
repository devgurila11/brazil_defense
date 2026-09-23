// Brazil Defense. The candidates: the bosses of the match, and what the count does with them.

#pragma once

#include "CoreMinimal.h"
#include "Match/BDMatchTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDCandidateSubsystem.generated.h"

class ABDCandidate;
class ABDMatchManager;
class UBDWaveSubsystem;

/** A candidate the match has sent: who he was, so he can come back as he was. */
USTRUCT()
struct FBDCandidateRecord
{
	GENERATED_BODY()

	/** 1 for the first of the match, 2 for the second, and so on. */
	UPROPERTY()
	int32 Ordinal = 0;

	/** The wave he first came out on. */
	UPROPERTY()
	int32 Wave = 0;

	/** The health he came out with, and comes back with. */
	UPROPERTY()
	float MaxHealth = 0.0f;
};

/**
 * The candidates are the bosses: one walks out every CandidateInterval waves, as tough
 * as the creeps of that wave times CandidateHealthMultiplier, so the twentieth is far
 * beyond the first. Slow, out of a drawn mouth, the first thing every defender shoots.
 * Any candidate reaching the urn, on any wave, ends the match at once.
 *
 * The count decides the rest. When the red counter passes the blue, every candidate
 * killed so far comes back at the health he fell with, spread over the wave, and no
 * ordinary creep walks while they do: the wave is their parade. Kill them all and the
 * count is levelled downwards, blue brought to red - nothing won, only the bleeding
 * stopped - so throwing the count on purpose buys nothing. One of them at the urn is the
 * defeat like any other. Pass the count again later and they all come back again.
 *
 * ABDMatchManager asks HasCandidateOnBoard before it settles the match; the towers ask
 * GetCandidate to know whom to shoot first; UBDWaveSubsystem asks IsReturnActive to hold
 * its creeps.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDCandidateSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject interface

	/** Convenience accessor. Returns null when the world context has no world. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate", meta = (WorldContext = "WorldContextObject"))
	static UBDCandidateSubsystem* Get(const UObject* WorldContextObject);

	//~ State -----------------------------------------------------------------

	/** The candidate on the board that came out first, or null. There may be more during a return. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate")
	ABDCandidate* GetCandidate() const;

	/** Every candidate walking right now. */
	void GetLivingCandidates(TArray<ABDCandidate*>& OutCandidates) const;

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate")
	bool HasCandidateOnBoard() const { return GetCandidate() != nullptr; }

	/** Whether the fallen are coming back: no ordinary creep walks until they are all down. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate")
	bool IsReturnActive() const { return bReturnActive; }

	/** Of the returning candidates, how many are still to be beaten (walking or yet to come out). */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate")
	int32 GetReturnRemaining() const;

	/** How many candidates this match has sent out, scheduled ones only. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate")
	int32 GetCandidatesSent() const { return Sent.Num(); }

	/** How many of them have been killed at least once. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate")
	int32 GetFallenCount() const { return Fallen.Num(); }

	/** Whether red is ahead of blue on the count, with at least one red vote in. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate")
	bool IsScoreboardInverted() const;

	/** Kept for the callers of the old pause: nothing freezes any more. */
	bool IsCountFrozen() const { return false; }
	float GetPauseRemaining() const { return 0.0f; }

	//~ Sending them out -----------------------------------------------------------

	/** Sends the next scheduled candidate now, whatever the wave. Debug and the schedule both use it. */
	ABDCandidate* SpawnCandidate(const TCHAR* Why);

	/** Starts the return of the fallen now, whatever the count says. Nothing happens with none fallen. */
	void BeginReturn(const TCHAR* Why);

	//~ Reports from the candidates. Not meant to be called by anything else. ------
	void NotifyCandidateArrived(ABDCandidate* Arrived);
	void NotifyCandidateKilled(ABDCandidate* Killed);

private:
	ABDMatchManager* GetMatch() const;
	UBDWaveSubsystem* GetWaves() const;

	/** Listens to the match once there is one; it is spawned after this subsystem. */
	void EnsureMatchBinding();
	void HandleVotesChanged(int32 Blue, int32 Red);
	/** A wave has been dealt (UBDWaveSubsystem::OnWaveDealt): a scheduled candidate walks out ahead of its creeps. */
	void HandleWaveDealt(int32 Wave);
	void HandlePhaseChanged(EBDMatchPhase NewPhase);

	/** Spawns a candidate actor for a record, at that record's health. Null when it cannot. */
	ABDCandidate* SpawnFromRecord(const FBDCandidateRecord& Record, bool bReturning, const TCHAR* Why);

	/** Everything from the last match dropped: on a rewind to wave 0. */
	void ResetForNewMatch();

	/** The one who came out first among those still walking. */
	TArray<TWeakObjectPtr<ABDCandidate>> Living;

	/** Every scheduled candidate sent, in order. */
	UPROPERTY(Transient)
	TArray<FBDCandidateRecord> Sent;

	/** Those killed at least once, in order of their first death: the ones a return brings back. */
	UPROPERTY(Transient)
	TArray<FBDCandidateRecord> Fallen;

	/** A scheduled candidate whose wave came while another was still walking: sent with the next wave. */
	bool bSchedulePending = false;

	//~ The return
	bool bReturnActive = false;
	UPROPERTY(Transient)
	TArray<FBDCandidateRecord> ReturnQueue;
	float ReturnTimer = 0.0f;
	float ReturnInterval = 0.0f;
	int32 ReturnAlive = 0;
	int32 ReturnsStarted = 0;

	/** So a board with no route is reported once, not on every vote. */
	bool bWarnedNoMouth = false;

	TWeakObjectPtr<ABDMatchManager> BoundMatch;
	FDelegateHandle VotesChangedHandle;
	/** Bound to the wave subsystem rather than the match, so the candidate goes out after the mouths move and before the first creep. */
	TWeakObjectPtr<UBDWaveSubsystem> BoundWaves;
	FDelegateHandle WaveDealtHandle;
	FDelegateHandle PhaseChangedHandle;
};

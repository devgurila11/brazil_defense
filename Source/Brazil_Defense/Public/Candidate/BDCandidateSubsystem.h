// Brazil Defense. When the red candidate comes out, and what its end does to the match.

#pragma once

#include "CoreMinimal.h"
#include "Match/BDMatchTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDCandidateSubsystem.generated.h"

class ABDCandidate;
class ABDMatchManager;
class UBDWaveSubsystem;

/**
 * The climax of a match, as a rule: whenever the red counter passes the blue one, and
 * at least one red vote has been scored, a candidate walks out of a mouth drawn among
 * those that have a route. There is never more than one out.
 *
 * What it does is decided here, not on the actor. Reaching the urn is the defeat. Dying
 * scores nothing and buys a pause: for CandidateKillPauseSeconds no wave goes out, the
 * countdown holds, and the red counter is frozen - arrivals in that window count for
 * nothing. When the pause ends everything resumes, and if the scoreboard is still
 * inverted the candidate comes back with the next wave, not before.
 *
 * ABDMatchManager and UBDWaveSubsystem ask IsCountFrozen to hold their clocks; the
 * towers ask GetCandidate to know whom to shoot first.
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

	/** The candidate on the board, or null. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate")
	ABDCandidate* GetCandidate() const { return Candidate.Get(); }

	/** Whether the pause bought by a kill is running: no wave goes out and red votes are not counted. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate")
	bool IsCountFrozen() const { return PauseRemaining > 0.0f; }

	/** Seconds of pause left. 0 when none is running. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate")
	float GetPauseRemaining() const { return PauseRemaining; }

	/** How many candidates this match has sent out. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate")
	int32 GetCandidatesSent() const { return CandidatesSent; }

	/** Whether the scoreboard calls for a candidate right now, whatever else may be in the way. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate")
	bool IsScoreboardInverted() const;

	//~ Sending it out -------------------------------------------------------------

	/**
	 * Sends the candidate out, whatever the scoreboard says. Refused, and logged, when
	 * one is already out, the match is over, or no mouth has a route.
	 * @return the candidate, or null.
	 */
	ABDCandidate* SpawnCandidate(const TCHAR* Why);

	//~ Reports from the candidate. Not meant to be called by anything else. ------

	void NotifyCandidateArrived(ABDCandidate* Arrived);
	void NotifyCandidateKilled(ABDCandidate* Killed);

private:
	ABDMatchManager* GetMatch() const;
	UBDWaveSubsystem* GetWaves() const;

	/** Listens to the match once there is one; it is spawned after this subsystem. */
	void EnsureMatchBinding();
	void HandleVotesChanged(int32 Blue, int32 Red);
	void HandleWaveStarted(int32 Wave);
	void HandlePhaseChanged(EBDMatchPhase NewPhase);

	/** Sends the candidate out if the scoreboard asks for it and nothing stands in the way. */
	void EvaluateTrigger(const TCHAR* Why);

	/** The candidate on the board. Weak: it destroys itself when it ends. */
	TWeakObjectPtr<ABDCandidate> Candidate;

	/** Seconds left of the pause a kill bought. Counted down here, in dilated time. */
	float PauseRemaining = 0.0f;

	/**
	 * Whether a vote change may send the candidate out. Cleared by a kill and set again
	 * when the next wave goes out: after a kill the candidate returns with a wave, not
	 * with the next arrival.
	 */
	bool bArmed = true;

	int32 CandidatesSent = 0;

	/** So a board with no route is reported once, not on every vote. */
	bool bWarnedNoMouth = false;

	TWeakObjectPtr<ABDMatchManager> BoundMatch;
	FDelegateHandle VotesChangedHandle;
	FDelegateHandle WaveStartedHandle;
	FDelegateHandle PhaseChangedHandle;
};

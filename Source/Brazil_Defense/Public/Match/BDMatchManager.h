// Brazil Defense. The clock and the ledger of a match.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/BDGridTypes.h"
#include "Match/BDMatchTypes.h"
#include "BDMatchManager.generated.h"

class ABDTowerBase;
class UBDDayCycleComponent;
class UBDDifficultyData;
class UBDGridSubsystem;
class UBDMatchSave;
class UBDPlacementComponent;

/** Broadcast whenever the match moves to another phase. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBDOnMatchPhaseChanged, EBDMatchPhase /*NewPhase*/);

/** Broadcast when a wave goes out. Carries the number of the wave that just started. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBDOnWaveStarted, int32 /*Wave*/);

/** Broadcast whenever either vote counter moves. Carries the new totals, blue then red. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBDOnVotesChanged, int32 /*Blue*/, int32 /*Red*/);

/**
 * Owns where a match stands: the phase, the wave, the countdown and what the player has
 * left to build with. Nothing else is allowed to decide those.
 *
 * Spawned by ABDGameMode when the level does not already carry one, and never spatially
 * loaded when it does: a match manager that streams out is a match that forgets itself.
 *
 * Also keeps the score, which in this game is votes: every creep killed adds its votes to
 * the blue counter, every creep that reaches the urn adds its votes to the red one.
 * UBDWaveSubsystem reports both, and reports the board empty through OnWaveCleared.
 */
UCLASS(meta = (DisplayName = "BD Match Manager"))
class BRAZIL_DEFENSE_API ABDMatchManager : public AActor
{
	GENERATED_BODY()

public:
	ABDMatchManager();

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
#endif
	//~ End AActor interface

	/** The match manager of a world, or null when there is none. */
	static ABDMatchManager* Get(const UObject* WorldContextObject);

	//~ State -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	EBDMatchPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetCurrentWave() const { return CurrentWave; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	float GetTimeUntilNextWave() const { return TimeUntilNextWave; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetDividersRemaining() const { return DividersRemaining; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetPlatformsRemaining() const { return PlatformsRemaining; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetTowersRemaining() const { return TowersRemaining; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetCharactersRemaining() const { return CharactersRemaining; }

	/** Health multiplier the creeps of the current wave spawn with. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	float GetHealthScale() const;

	/** 1 until the urn is placed, then 0. Not a difficulty knob: there is one urn in the game. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetObjectivesRemaining() const { return ObjectivesRemaining; }

	/** Running total of the early call bonus, banked until there is an economy to spend it. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetEarlyCallBonus() const { return EarlyCallBonus; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	float GetGameSpeed() const { return GameSpeed; }

	/** Votes scored by the player: the sum of VotesOnDeath of every creep killed. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetVotesBlue() const { return VotesBlue; }

	/** Votes scored against the player: the sum of VotesOnArrival of every creep that reached the urn. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetVotesRed() const { return VotesRed; }

	/** Null votes: wasted damage at the vote rate. Shown on the count, never scored. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetVotesNull() const { return VotesNull; }

	/** Wasted damage, in votes. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void AddVotesNull(int32 Votes);

	/** True once the first wave has gone out and the maze is locked in. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool IsBuildLocked() const { return CurrentWave >= 1; }

	//~ Phase control ---------------------------------------------------------

	/** Opens the building phase and starts the countdown to the next wave. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void StartBuildingPhase();

	/** Debug: jumps the wave counter, so the scaling of any wave can be tested without playing up to it. */
	void DebugSetWave(int32 Wave);

	/** Debug: puts every budget back to what the difficulty hands out, as if nothing had been placed. */
	void DebugResetBudgets();

	/** Debug: throws the generated obstacles away and lays them again from a given seed. Placed pieces keep their cells. */
	void DebugRegenerateObstacles(int32 Seed);

	/**
	 * Debug: forces the match into a phase, skipping whatever would normally get it there.
	 * Building rewinds the wave count to zero as well, so placement opens up again rather
	 * than staying locked by a wave that never happened. Console tooling until the HUD and
	 * the spawner exist; not a game rule.
	 */
	void DebugForcePhase(EBDMatchPhase NewPhase);

	/** Skips the rest of the countdown, banking a bonus for the time given up. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void CallWaveEarly();

	/** Reported by UBDWaveSubsystem once the board is empty again. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void OnWaveCleared();

	/**
	 * Ends the match as a loss: the phase goes to Defeat and the board freezes where it
	 * stands. Nothing after it moves until the match is rewound from the console.
	 * @param Reason for the log, e.g. "the candidate reached the urn".
	 */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void DeclareDefeat(const FString& Reason);

	/** Whether the match is over, won or lost. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool IsMatchOver() const { return Phase == EBDMatchPhase::Defeat || Phase == EBDMatchPhase::Victory; }

	//~ Winning ------------------------------------------------------------------
	// Clearing the difficulty's last wave wins: the phase goes to Victory and the board
	// freezes like a defeat, so the player reads the result. Not while the candidate is
	// on the board, though: he is the one thing that can still lose the match, so the
	// waves keep coming until he dies (the win is taken then, at once between waves or
	// when the running wave clears) or reaches the urn. The win is kept once it is
	// taken; the player may then carry on into endless, where the waves keep scaling and
	// only a defeat ends things. A defeat after a win does not undo the win.

	/** Waves to clear for the win on this difficulty. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetWavesToWin() const;

	/** The difficulty asset this match runs on, after the fallback to the class defaults. */
	const UBDDifficultyData* GetDifficultyData() const { return DifficultyData; }

	/** Prisoners the win on this difficulty frees. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetPrisonersFreed() const;

	/** True once the winning wave has been cleared, whatever happened after. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool HasWon() const { return bWon; }

	/** True once the player chose to keep playing past the win. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool IsEndless() const { return bEndless; }

	/** The last wave cleared, nothing settled yet, and no candidate walking: the count decides now. */
	bool IsEndDue() const;

	/** Settles the match by the count: blue ahead or level wins, red ahead loses. */
	void ResolveEnd();

	/** Brings blue down to red (or red down to blue) so nobody is ahead: what killing the returning candidates buys. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void EqualizeVotesDown(const FString& Why);

	/** Ends the match as a win. Called when the win is due; public so the console can force it. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void DeclareVictory();

	/** From Victory, reopens the building phase for the next wave and keeps going until a defeat. @return false outside Victory. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	bool ContinueEndless();

	//~ Saving ------------------------------------------------------------------
	// The difficulty hands out a few saves. Each one writes the whole match to the one
	// slot, between waves only, and the count goes down: choosing when to spend them is
	// the game. Loading rebuilds the match in place, at the start of the wave after the
	// one saved, with the saves left as they were after that save.

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetSavesRemaining() const { return SavesRemaining; }

	/** Between waves, match still running, saves left. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool CanSaveMatch() const;

	/** Spends a save and writes the match. @return false when it cannot, with nothing spent. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	bool SaveMatch();

	/** Whether a saved match exists to load. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	static bool HasSavedMatch();

	/** Reads the slot and rebuilds this match from it. @return false when there is nothing to load. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	bool LoadMatch();

	/** Rebuilds this match from a snapshot, whatever it was doing. */
	void RestoreMatch(const UBDMatchSave& Save);

	//~ Chained difficulties -------------------------------------------------------
	// A win is recorded in UBDProgressSave. A match on a difficulty whose lower neighbour
	// has been won opens with that difficulty's ChainBonus on top of its budgets.

	/** The difficulty one step below this match's, or Count for the lowest. */
	static EBDDifficulty GetDifficultyBelow(EBDDifficulty Difficulty);

	/** Whether the chain bonus was handed out at the start of this match. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool WasChainBonusApplied() const { return bChainBonusApplied; }

	//~ Votes, reported by the wave subsystem -----------------------------------

	/** A creep was killed: its votes go to the player. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void AddVotesBlue(int32 Votes);

	/** A creep reached the urn: its votes go against the player. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void AddVotesRed(int32 Votes);

	/** Blue votes are the player's currency as well as their score: spending them lowers both. @return false, and nothing spent, when there are not enough. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	bool SpendVotesBlue(int32 Votes);

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool CanAffordVotesBlue(int32 Votes) const { return Votes <= VotesBlue; }

	//~ Upgrades, for the HUD and the log -----------------------------------------

	/** Blue votes the next level of a defender costs. 0 for null or a defender at max level. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetUpgradeCost(const ABDTowerBase* Tower) const;

	/**
	 * Whether spending this many blue votes would put red ahead of blue. The player may
	 * still do it, but has to be told: otherwise the candidate flips and it looks like a bug.
	 */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool WouldInvertScoreboard(int32 Cost) const;

	//~ Moving pieces between waves --------------------------------------------

	/** Whether a placed piece may be picked up and put elsewhere right now: only while building. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool CanMove() const { return Phase == EBDMatchPhase::Building; }

	/** Fraction of a piece's build cost a move costs on the current wave. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	float GetMoveTaxRate() const;

	/** Blue votes a move of a piece of this build cost charges on the current wave. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetMoveCost(int32 BuildCost) const;

	/**
	 * Sets how fast the match runs. Only the speeds listed in the balance settings are
	 * accepted, and the choice persists across waves: a player who asked for 4x meant it.
	 *
	 * Implemented with global time dilation, so every system follows without knowing this
	 * exists. Nothing may multiply its own deltas to match.
	 */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	bool SetGameSpeed(float Speed);

	//~ Budget, asked by the placement gesture ---------------------------------

	/** Whether the current phase and budget allow placing a piece of this kind. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool CanPlace(EBDPieceKind Kind) const;

	/** Pieces of this kind still in hand, whatever the phase. 0 for a kind the player never places. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetBudgetRemaining(EBDPieceKind Kind) const;

	/** Charges one piece of this kind to the budget. @return false when it was not allowed. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	bool ConsumeBudget(EBDPieceKind Kind);

	/** Whether a piece of this kind may be taken back right now: everything but the urn, at any point. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool CanRemove(EBDPieceKind Kind) const;

	/** Puts a piece of this kind back in the hand. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void RefundRemoval(EBDPieceKind Kind);

	//~ Selling ------------------------------------------------------------------
	// Nothing on the board is permanent: any piece can be sold, and the price of having
	// been wrong is the part of the build cost that does not come back. All of it comes
	// back before the first wave, half once the waves run, and a divider keeps the full
	// refund through the grace window: see UBDGameBalanceSettings::GetSellRefundRatio.
	// Building does not charge votes yet; selling already pays them, because the player
	// tries things and undoes them.

	/** Fraction of the build cost selling a piece of this kind pays back on the current wave. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	float GetSellRefundRatio(EBDPieceKind Kind) const;

	/** Blue votes selling a piece of this kind and build cost pays back right now. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetSellRefund(EBDPieceKind Kind, int32 BuildCost) const;

	/** Pays the sale of a piece into the blue counter. @return the votes paid. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	int32 RefundSale(EBDPieceKind Kind, int32 BuildCost);

	/** The day cycle driven by this match. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	UBDDayCycleComponent* GetDayCycle() const { return DayCycle; }

	//~ Notifications ---------------------------------------------------------

	FBDOnMatchPhaseChanged OnPhaseChanged;
	FBDOnWaveStarted OnWaveStarted;
	FBDOnVotesChanged OnVotesChanged;

	/** Seed the board was generated from, so a match can be handed over as a number. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brazil Defense|Match")
	int32 ObstacleSeed = 0;

	/** Draw a fresh seed at BeginPlay instead of using ObstacleSeed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brazil Defense|Match")
	bool bRandomizeSeed = true;

	/** Difficulty this match runs on. Falls back to the one in the balance settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brazil Defense|Match")
	EBDDifficulty Difficulty = EBDDifficulty::Normal;

private:
	/** Board setup: apply the difficulty, then generate the obstacles over the layout. */
	void SetupBoard();

	/** Moves to a phase and tells whoever is listening. */
	void SetPhase(EBDMatchPhase NewPhase);

	/** Sends the next wave out. */
	void StartWave();

	/** Counter behind a placeable state, or null when that state is not the player's to place. */
	int32* FindBudget(EBDPieceKind Kind);
	const int32* FindBudget(EBDPieceKind Kind) const;

	UBDGridSubsystem* GetGrid() const;

	/** The local player's placement component, which owns the pieces on the board. */
	UBDPlacementComponent* GetPlacement() const;

	/** Resolves DifficultyData from Difficulty, falling back to the class defaults. */
	void ResolveDifficulty();

	/** The budgets of the difficulty, plus the chain bonus when the one below has been won. Votes are not touched. */
	void ApplyStartingBudgets();

	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense|Match")
	TObjectPtr<UBDDayCycleComponent> DayCycle;

	/** Resolved from Difficulty at BeginPlay. */
	UPROPERTY(Transient)
	TObjectPtr<const UBDDifficultyData> DifficultyData;

	EBDMatchPhase Phase = EBDMatchPhase::Setup;
	int32 CurrentWave = 0;
	float TimeUntilNextWave = 0.0f;
	int32 DividersRemaining = 0;
	int32 PlatformsRemaining = 0;
	int32 TowersRemaining = 0;
	int32 CharactersRemaining = 0;
	int32 ObjectivesRemaining = 0;
	int32 EarlyCallBonus = 0;
	float GameSpeed = 1.0f;
	int32 VotesBlue = 0;
	int32 VotesRed = 0;
	int32 VotesNull = 0;
	bool bWon = false;
	bool bEndless = false;
	int32 SavesRemaining = 0;
	bool bChainBonusApplied = false;
};

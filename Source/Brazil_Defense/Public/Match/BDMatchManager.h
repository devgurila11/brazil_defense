// Brazil Defense. The clock and the ledger of a match.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/BDGridTypes.h"
#include "Match/BDMatchTypes.h"
#include "BDMatchManager.generated.h"

class UBDDayCycleComponent;
class UBDDifficultyData;
class UBDGridSubsystem;

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

	/** True once the first wave has gone out and the maze is locked in. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool IsBuildLocked() const { return CurrentWave >= 1; }

	//~ Phase control ---------------------------------------------------------

	/** Opens the building phase and starts the countdown to the next wave. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void StartBuildingPhase();

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

	//~ Votes, reported by the wave subsystem -----------------------------------

	/** A creep was killed: its votes go to the player. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void AddVotesBlue(int32 Votes);

	/** A creep reached the urn: its votes go against the player. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void AddVotesRed(int32 Votes);

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

	/** Charges one piece of this kind to the budget. @return false when it was not allowed. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	bool ConsumeBudget(EBDPieceKind Kind);

	/** Whether a piece of this kind may still be taken back right now. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool CanRemove(EBDPieceKind Kind) const;

	/** Gives back whatever the current rules refund for taking a piece of this kind back. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void RefundRemoval(EBDPieceKind Kind);

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
	int32 EarlyCallBonus = 0;
	float GameSpeed = 1.0f;
	int32 VotesBlue = 0;
	int32 VotesRed = 0;
};

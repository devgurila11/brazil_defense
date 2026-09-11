// Brazil Defense. Seeded, validated generation of the permanent obstacles of a board.

#pragma once

#include "CoreMinimal.h"
#include "Grid/BDGridTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDObstacleGenerator.generated.h"

class UBDGridSubsystem;
class UBDPathfinder;
struct FRandomStream;

/**
 * Scatters the permanent obstacles of a match over the grid.
 *
 * Two properties matter more than the scattering itself. It is seeded, so the same seed
 * always rebuilds the same board: a bug someone hit can be handed over as a number, and
 * a daily challenge is the same number for everyone. And it proposes rather than decides:
 * a candidate layout is measured against UBDObstacleSettings and thrown away if it fails,
 * because a board where a spawn cannot reach the goal is not a hard board, it is a broken
 * one. After MaxAttempts rejected proposals it falls back to an authored layout, which is
 * dull but known playable.
 *
 * Obstacles are written as Blocked: permanent scenery the player can never take back,
 * as opposed to the Platform cells and divider edges they place themselves.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDObstacleGenerator : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Convenience accessor. Returns null when the world context has no world. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Obstacles", meta = (WorldContext = "WorldContextObject"))
	static UBDObstacleGenerator* Get(const UObject* WorldContextObject);

	/**
	 * Clears whatever this generator placed before and lays down a fresh obstacle layout.
	 *
	 * @param Seed                 same seed on the same board gives the same layout.
	 * @param ObstacleCountOverride how many obstacles to place, or -1 to use the settings.
	 *                             A match passes the count from its UBDDifficultyData.
	 * @return true when a proposal passed validation, false when every attempt was rejected
	 *         and the authored fallback was used. Callers that care about board quality can
	 *         tell the two apart; callers that just want a playable board can ignore it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Obstacles")
	bool GenerateObstacles(UBDGridSubsystem* Grid, int32 Seed, int32 ObstacleCountOverride = -1);

	/** Puts every cell this generator blocked back to Free. @return cells cleared. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Obstacles")
	int32 ClearGeneratedObstacles(UBDGridSubsystem* Grid);

	//~ Last generation, for logging and for the console ----------------------

	const TArray<FBDCellCoord>& GetGeneratedCells() const { return GeneratedCells; }
	int32 GetLastSeed() const { return LastSeed; }
	int32 GetLastAttemptCount() const { return LastAttemptCount; }
	int32 GetLastPathLength() const { return LastPathLength; }
	bool WasLastGenerationValidated() const { return bLastGenerationValidated; }

private:
	/**
	 * Cells that may receive an obstacle: currently Free and far enough from every Spawn
	 * and Goal. Clearance is enforced here rather than in validation, because a rule that
	 * can be obeyed by construction should never cost a rejected attempt.
	 */
	static void BuildCandidates(const UBDGridSubsystem& Grid, const TArray<FBDCellCoord>& Spawns,
		const TArray<FBDCellCoord>& Goals, int32 Clearance, TArray<FBDCellCoord>& OutCandidates);

	/** Draws Count distinct cells out of Candidates, without disturbing the caller array. */
	static void PickCells(FRandomStream& Stream, const TArray<FBDCellCoord>& Candidates,
		int32 Count, TArray<FBDCellCoord>& OutPicked);

	static void WriteCells(UBDGridSubsystem& Grid, const TArray<FBDCellCoord>& Cells, EBDCellState State);

	/** Fraction of the whole grid still Free. */
	static float ComputeFreeRatio(const UBDGridSubsystem& Grid);

	/**
	 * Whether the board as it stands right now is worth playing.
	 * @param OutReason filled with why it was rejected, for the log.
	 */
	bool IsLayoutPlayable(const UBDGridSubsystem& Grid, const TArray<FBDCellCoord>& Spawns,
		const TArray<FBDCellCoord>& Goals, FString& OutReason);

	const UBDPathfinder* GetPathfinder() const;

	/** Cells currently held by this generator, so a regeneration can hand them back. */
	UPROPERTY(Transient)
	TArray<FBDCellCoord> GeneratedCells;

	int32 LastSeed = 0;
	int32 LastAttemptCount = 0;
	int32 LastPathLength = 0;
	bool bLastGenerationValidated = false;
};

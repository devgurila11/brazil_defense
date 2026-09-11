// Brazil Defense. Rules the generated obstacle layout has to satisfy.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Grid/BDGridTypes.h"
#include "BDObstacleSettings.generated.h"

/**
 * Shape and acceptance criteria of the generated obstacle layout.
 * Edited in Project Settings > Game > Brazil Defense - Obstacles, saved to DefaultGame.ini.
 *
 * These are the rules a candidate layout is measured against, not the layout itself:
 * generation proposes, this decides whether the proposal is playable.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Obstacles"))
class BRAZIL_DEFENSE_API UBDObstacleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDObstacleSettings();

	static const UBDObstacleSettings& Get();

	/**
	 * How many obstacle cells to place when no difficulty is driving the generation.
	 * A match takes this from UBDDifficultyData instead; this is what the console
	 * command and any test without a match use.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Shape", meta = (ClampMin = "0", UIMin = "0"))
	int32 ObstacleCount = 20;

	/** Cells this close to a Spawn or a Goal never receive an obstacle. Chebyshev radius. */
	UPROPERTY(config, EditAnywhere, Category = "Shape", meta = (ClampMin = "0", UIMin = "0"))
	int32 ClearanceFromSpawnGoal = 3;

	/** Shortest accepted route from a spawn to its goal, in cells. Below this the map is trivial. */
	UPROPERTY(config, EditAnywhere, Category = "Validation", meta = (ClampMin = "1", UIMin = "1"))
	int32 MinPathLength = 40;

	/** Longest accepted route, in cells. Above this the board is already close to sealed. */
	UPROPERTY(config, EditAnywhere, Category = "Validation", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxPathLength = 200;

	/** Fraction of the whole grid that must still be Free once the obstacles are down. */
	UPROPERTY(config, EditAnywhere, Category = "Validation", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MinFreeRatio = 0.6f;

	/** How many layouts to propose before giving up and using the authored fallback. */
	UPROPERTY(config, EditAnywhere, Category = "Validation", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxAttempts = 50;

	/**
	 * Layout used when no proposal passed validation. Authored by hand, so it is known
	 * good: a match that cannot generate a board still has to be playable.
	 * Left empty it means no obstacles at all, which is dull but never unplayable.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Validation")
	TArray<FBDCellCoord> FallbackObstacles;
};

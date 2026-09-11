// Brazil Defense. Core types of a match: how hard it is and where it stands.

#pragma once

#include "CoreMinimal.h"
#include "BDMatchTypes.generated.h"

/**
 * Difficulty of a match. Picks which UBDDifficultyData drives the budgets, the number
 * of obstacles and the towers the player starts with.
 */
UENUM(BlueprintType)
enum class EBDDifficulty : uint8
{
	Easy UMETA(DisplayName = "Easy"),
	Normal UMETA(DisplayName = "Normal"),
	Hard UMETA(DisplayName = "Hard"),

	/** Number of valid difficulties. Keep last. */
	Count UMETA(Hidden)
};

/**
 * Where a match stands right now.
 *
 * Building and WaveActive alternate for the whole match: the countdown runs during
 * Building, the wave runs during WaveActive. What changes across the match is not the
 * phase but what Building still allows, since dividers stop being placeable once the
 * first wave has gone out. See ABDMatchManager.
 */
UENUM(BlueprintType)
enum class EBDMatchPhase : uint8
{
	/** Board is being prepared: layout applied, obstacles generated. No input yet. */
	Setup UMETA(DisplayName = "Setup"),

	/** Player is placing, and the countdown to the next wave is running. */
	Building UMETA(DisplayName = "Building"),

	/** A wave is out on the board. */
	WaveActive UMETA(DisplayName = "Wave Active"),

	Defeat UMETA(DisplayName = "Defeat"),
	Victory UMETA(DisplayName = "Victory"),

	/** Number of valid phases. Keep last. */
	Count UMETA(Hidden)
};

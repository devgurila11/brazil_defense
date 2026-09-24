// Brazil Defense. What a difficulty hands the player at the start of a match.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Match/BDMatchTypes.h"
#include "BDDifficultyData.generated.h"

/**
 * What a win on the difficulty below adds to the starting hand of this one. Zero
 * everywhere means the difficulty stands alone. The defaults are placeholders, like
 * every number until there is real content to calibrate against.
 */
USTRUCT(BlueprintType)
struct FBDChainBonus
{
	GENERATED_BODY()

	/** Blue votes the match opens with: a head start on the count. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Bonus", meta = (ClampMin = "0", UIMin = "0"))
	int32 Votes = 100;

	/** Public money on top of the starting funds: a head start on the board. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Bonus", meta = (ClampMin = "0", UIMin = "0"))
	int32 Funds = 400;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Bonus", meta = (ClampMin = "0", UIMin = "0"))
	int32 Dividers = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Bonus", meta = (ClampMin = "0", UIMin = "0"))
	int32 Platforms = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Bonus", meta = (ClampMin = "0", UIMin = "0"))
	int32 Saves = 1;

	bool IsEmpty() const { return Votes == 0 && Funds == 0 && Dividers == 0 && Platforms == 0 && Saves == 0; }
};

/**
 * The starting hand of a match: how much the player gets to build with, how many pieces
 * of the maze they walk in holding, how cluttered the board is and how long the first
 * countdown runs.
 *
 * A data asset rather than settings because difficulty is content: designers add and tune
 * these without touching the project configuration, and a future daily challenge can ship
 * its own without inventing a new difficulty enum value.
 */
UCLASS(BlueprintType, meta = (DisplayName = "BD Difficulty"))
class BRAZIL_DEFENSE_API UBDDifficultyData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Which difficulty this asset describes. Used to look it up from the balance settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	EBDDifficulty Difficulty = EBDDifficulty::Normal;

	/**
	 * Dividers the player walks in with. One piece is one segment, whatever its length.
	 *
	 * The dividers are a budget of their own, like the walls of Clash of Clans: they cost
	 * no public money, so drawing the path never competes with defending it, and the
	 * hand grows as the match goes on (below) so the maze can get denser late.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0", UIMin = "0"))
	int32 DividerBudget = 28;

	/** Dividers added to the hand every time a wave is cleared. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0", UIMin = "0"))
	int32 DividersPerWave = 1;

	/** Dividers the first scheduled candidate killed adds to the hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0", UIMin = "0"))
	int32 DividersPerCandidate = 4;

	/** How many more each later candidate adds than the one before: the N-th pays PerCandidate + Step x (N - 1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0", UIMin = "0"))
	int32 DividersPerCandidateStep = 1;

	/** Dividers the N-th scheduled candidate killed adds to the hand. */
	int32 GetDividersForCandidate(const int32 Ordinal) const
	{
		return FMath::Max(0, DividersPerCandidate + DividersPerCandidateStep * FMath::Max(0, Ordinal - 1));
	}

	/** Platforms the player may place during the building phase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0", UIMin = "0"))
	int32 PlatformBudget = 3;

	// Defenders have no ceiling of their own. A ground tower is limited by the public money
	// it costs and by the cells left on the grid; a character by the money and by a free
	// platform slot to stand on. Two brakes the player can see, instead of a number that
	// silently greys a button out.

	/**
	 * Public money the player walks in with. Everything is bought with public money and
	 * the only other source of it is the scheduled candidates, the first of whom is waves
	 * away, so this is what the opening defense is paid for: a decent base, never the
	 * whole board. At the default prices a defender on wave 1 costs about 400.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0", UIMin = "0"))
	int32 StartingFunds = 2000;

	/**
	 * Blue votes on the count before the first kill: a head start in the election, and
	 * nothing more. Votes are never spent, so this is not something to build with.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0", UIMin = "0"))
	int32 StartingVotes = 3000;

	/** Permanent obstacles scattered on the board before the player sees it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board", meta = (ClampMin = "0", UIMin = "0"))
	int32 ObstacleCount = 20;

	/** Seconds of building time before the first wave goes out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float FirstWaveDelay = 120.0f;

	/** Waves the player has to clear to win. The match goes on past it as endless, if the player asks. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victory", meta = (ClampMin = "1", UIMin = "1"))
	int32 WavesToWin = 100;

	/** Prisoners a win on this difficulty sets free: the reward the ending is told with. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Victory", meta = (ClampMin = "0", UIMin = "0"))
	int32 PrisonersFreed = 2;

	/** Times the player may save the match. Each save overwrites the last: the choice of when is the resource. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Saves", meta = (ClampMin = "0", UIMin = "0"))
	int32 SaveBudget = 2;

	/**
	 * Added to the starting hand when the difficulty one step below this one has been
	 * won (UBDProgressSave). Easy has nothing below it, so its bonus never applies. This
	 * is what makes starting on Hard cold nearly impossible, on purpose.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Bonus")
	FBDChainBonus ChainBonus;
};

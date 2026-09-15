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

	/** Blue votes the match opens with: a head start on the count, not just on the board. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Bonus", meta = (ClampMin = "0", UIMin = "0"))
	int32 Votes = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Bonus", meta = (ClampMin = "0", UIMin = "0"))
	int32 Dividers = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Bonus", meta = (ClampMin = "0", UIMin = "0"))
	int32 Platforms = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Bonus", meta = (ClampMin = "0", UIMin = "0"))
	int32 Towers = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Bonus", meta = (ClampMin = "0", UIMin = "0"))
	int32 Characters = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chain Bonus", meta = (ClampMin = "0", UIMin = "0"))
	int32 Saves = 1;

	bool IsEmpty() const { return Votes == 0 && Dividers == 0 && Platforms == 0 && Towers == 0 && Characters == 0 && Saves == 0; }
};

/**
 * The starting hand of a match: how much the player gets to build with, how many towers
 * they walk in holding, how cluttered the board is and how long the first countdown runs.
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

	/** Divider pieces the player may place during the building phase. One piece is one segment, whatever its length. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0", UIMin = "0"))
	int32 DividerBudget = 28;

	/** Platforms the player may place during the building phase. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0", UIMin = "0"))
	int32 PlatformBudget = 3;

	/** Ground towers the player may build. Unlike dividers, defenders stay placeable through the waves. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0", UIMin = "0"))
	int32 TowerBudget = 8;

	/** Characters the player may mount on platform slots. Separate from towers: they are a different resource. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Budget", meta = (ClampMin = "0", UIMin = "0"))
	int32 CharacterBudget = 12;

	/**
	 * Blue votes the player walks in with. Building charges the build cost of every piece,
	 * so this is the capital the opening defense is paid for: enough for a base, never
	 * enough for the whole board. From wave 1 the kills pay it back, and every piece added
	 * after that comes off the scoreboard - which is the cruel model working.
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

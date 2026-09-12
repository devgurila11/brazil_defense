// Brazil Defense. What a difficulty hands the player at the start of a match.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Match/BDMatchTypes.h"
#include "BDDifficultyData.generated.h"

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

	/** Permanent obstacles scattered on the board before the player sees it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Board", meta = (ClampMin = "0", UIMin = "0"))
	int32 ObstacleCount = 20;

	/** Seconds of building time before the first wave goes out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float FirstWaveDelay = 60.0f;
};

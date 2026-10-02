// Brazil Defense. What the Palácio do Governo is: its look and how far it has evolved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BDPalaceData.generated.h"

class UStaticMesh;

/**
 * The palace: a 2x2 building the player buys with public money, where the Agent will
 * come out of. Evolving it is what gives the Agent better weapons - but the Agent is not
 * built yet, so for now the level is only shown: five stars over the roof, as many
 * filled as the level says.
 *
 * The footprint, the price and the cell it writes are on the placeable that builds it
 * (UBDPlaceableData::PalaceData points here); this asset is only the building itself.
 */
UCLASS(BlueprintType)
class BRAZIL_DEFENSE_API UBDPalaceData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Stars on the indicator, and the highest level the palace reaches. */
	static constexpr int32 MaxLevel = 5;

	/**
	 * The building's look. Empty falls back to the engine cube: a placeholder block until
	 * the final model arrives. Swap it here and nothing else changes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/**
	 * Scale the mesh uniformly so its widest side spans the footprint, and stand it on the
	 * floor whatever its pivot. Off, the mesh is taken as authored, pivot on the ground.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look")
	bool bFitToFootprint = true;

	/** Share of the footprint the fitted mesh covers, so neighbours do not touch. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0.1", ClampMax = "1.0", EditCondition = "bFitToFootprint"))
	float FootprintFill = 0.9f;

	/** Applied on top of the fit, per axis. The placeholder cube flattens itself into a block with Z below 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look")
	FVector MeshScale = FVector(1.0f, 1.0f, 0.6f);

	/**
	 * How far the palace has evolved: 0 is not at all, 5 is every star filled. Has no
	 * effect on play yet - the Agent who will profit from it does not exist - it only
	 * sets how many stars the indicator fills.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Evolution", meta = (ClampMin = "0", ClampMax = "5", UIMin = "0", UIMax = "5"))
	int32 Level = 0;

	/** How high over the roof the stars float. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Evolution", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float StarsLift = 80.0f;
};

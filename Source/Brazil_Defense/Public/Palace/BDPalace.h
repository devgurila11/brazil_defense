// Brazil Defense. The Palácio do Governo standing on the board.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BDPalace.generated.h"

class UBDPalaceData;
class UStaticMeshComponent;

/**
 * The palace as placed: a static building over 2x2 cells, the mesh of its data fitted to
 * the footprint, and a level that the HUD draws as five stars over the roof.
 *
 * It holds the cells like a platform does - the placement flow writes them - and nothing
 * else for now: the Agent it will send out, and what the level gives him, come later.
 * Units do not collide with it: only the cursor's visibility traces see it.
 */
UCLASS(Blueprintable, meta = (DisplayName = "BD Palace"))
class BRAZIL_DEFENSE_API ABDPalace : public AActor
{
	GENERATED_BODY()

public:
	ABDPalace();

	/**
	 * Called once by whoever spawned the palace, before its first frame: applies the data,
	 * fits the mesh to the footprint and takes the level the data starts it on.
	 * @param FootprintSize width and depth of the cells it covers, in cm, before its yaw.
	 */
	void InitializePalace(const UBDPalaceData* InData, const FVector2D& FootprintSize);

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Palace")
	const UBDPalaceData* GetData() const { return Data; }

	/** Stars filled: 0 to UBDPalaceData::MaxLevel. Not GetLevel: AActor owns that name for the ULevel. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Palace")
	int32 GetPalaceLevel() const { return PalaceLevel; }

	/** Debug for now: the evolution that will set it does not exist yet. Clamped to 0..MaxLevel. */
	void SetPalaceLevel(int32 NewLevel);

	/** Where the middle of the star row floats, in the world: over the roof by the data's lift. */
	FVector GetStarsAnchor() const;

	UStaticMeshComponent* GetMeshComponent() const { return Mesh; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Palace")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Palace")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<const UBDPalaceData> Data;

	int32 PalaceLevel = 0;
};

// Brazil Defense. The single defender class, wherever it stands.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/BDGridTypes.h"
#include "BDTowerBase.generated.h"

class UBDPlatformComponent;

/**
 * The one and only defender type. It behaves the same on a free ground cell and on a
 * platform slot: same class, same data asset, same upgrade tree. Standing on a platform
 * only multiplies its range, through the multiplier of that platform.
 */
UCLASS(Blueprintable, meta = (DisplayName = "BD Tower Base"))
class BRAZIL_DEFENSE_API ABDTowerBase : public AActor
{
	GENERATED_BODY()

public:
	ABDTowerBase();

	/**
	 * Attack range before any platform bonus.
	 * Placeholder home for the value until the tower data asset lands.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Brazil Defense|Tower", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float BaseRange = 600.0f;

	/** Range actually used in combat: base range times the multiplier of the platform, when on one. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	float GetEffectiveRange() const;

	/** Platform this tower stands on, or null when it sits on a ground cell. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	UBDPlatformComponent* GetPlatform() const;

	/** Index of the occupied slot, or INDEX_NONE when the tower is on the ground. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	int32 GetPlatformSlotIndex() const { return PlatformSlotIndex; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	bool IsOnPlatform() const;

	/** Ground cell the tower occupies. Only meaningful while it is not on a platform. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	FBDCellCoord GetGroundCoord() const { return GroundCoord; }

	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Tower")
	void SetGroundCoord(const FBDCellCoord& Coord) { GroundCoord = Coord; }

	/** Called by UBDPlatformComponent when this tower takes a slot. Not meant to be called directly. */
	void NotifyOccupiedSlot(UBDPlatformComponent* Platform, int32 SlotIndex);

	/** Called by UBDPlatformComponent when this tower leaves a slot. Not meant to be called directly. */
	void NotifyReleasedSlot();

private:
	/** Weak on purpose: the platform actor and the tower have independent lifetimes. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UBDPlatformComponent> Platform;

	UPROPERTY(Transient)
	int32 PlatformSlotIndex = INDEX_NONE;

	UPROPERTY(Transient)
	FBDCellCoord GroundCoord;
};

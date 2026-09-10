// Brazil Defense. Platforms that hold towers above the ground: trucks, bleachers, stages.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Grid/BDGridTypes.h"
#include "BDPlatformComponent.generated.h"

class ABDTowerBase;
class UBDGridSettings;
class UBDGridSubsystem;

/** One tower mounting point on a platform. */
USTRUCT(BlueprintType)
struct FBDPlatformSlot
{
	GENERATED_BODY()

	/** Position of the slot in the local space of the platform actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform Slot")
	FVector LocalOffset = FVector::ZeroVector;

	/** Facing of the tower mounted on this slot, in the local space of the platform actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Platform Slot")
	FRotator LocalRotation = FRotator::ZeroRotator;

	/** Tower standing here, if any. Weak: the tower owns its own lifetime. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Platform Slot")
	TWeakObjectPtr<ABDTowerBase> Occupant;
};

/** Broadcast when a slot is taken or released. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBDOnPlatformSlotChanged, int32 /*SlotIndex*/);

/**
 * Turns any actor into a tower platform. The platform occupies a rectangle of grid cells
 * and offers a number of elevated slots; a tower standing on one of them is the very same
 * ABDTowerBase that would sit on a ground cell, only with its range multiplied.
 *
 * The component stamps Platform over its footprint cells when it stands inside the
 * battle area, and
 * restores the previous cell states when it goes away or moves. Platform blocks movement
 * like Blocked does, but stays removable and refundable, which Blocked never is.
 */
UCLASS(ClassGroup = (BrazilDefense), meta = (BlueprintSpawnableComponent, DisplayName = "BD Platform"))
class BRAZIL_DEFENSE_API UBDPlatformComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBDPlatformComponent();

	//~ Begin UActorComponent interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent interface

	/** Size of the grid rectangle covered by the platform, in cells. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brazil Defense|Platform", meta = (ClampMin = "1"))
	FIntPoint GridFootprint = FIntPoint(1, 1);

	/** Mounting points offered by this platform. Authored per Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Platform")
	TArray<FBDPlatformSlot> Slots;

	/** Range bonus given by the height of the platform. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Platform", meta = (ClampMin = "0.01", UIMin = "1.0"))
	float RangeMultiplier = 1.25f;

	/**
	 * Where the platform stands. This is about location, not about movement rules:
	 * blocking is a consequence of standing on the grid, not a separate choice.
	 *
	 * True: inside the battle area. The footprint cells are stamped Platform and block
	 * movement. Everything placed on the playable area keeps this true.
	 *
	 * False: on the surrounding terrain, outside the grid, like an edge bleacher. There
	 * is no cell under it, so it takes no part in cell logic at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Platform")
	bool bInsideBattleArea = true;

	//~ Slots ----------------------------------------------------------------

	/** Places a tower on a slot and snaps it to the slot transform. @return false when the slot is taken or invalid. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Platform")
	bool TryOccupy(int32 SlotIndex, ABDTowerBase* Tower);

	/** Frees a slot. @return false when the index is invalid or the slot was already free. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Platform")
	bool Release(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Platform")
	int32 GetFreeSlotCount() const;

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Platform")
	bool IsSlotFree(int32 SlotIndex) const;

	/** @return the index of the first free slot, or INDEX_NONE when the platform is full. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Platform")
	int32 FindFirstFreeSlot() const;

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Platform")
	ABDTowerBase* GetSlotOccupant(int32 SlotIndex) const;

	/** World transform a tower mounted on this slot should take. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Platform")
	FTransform GetSlotWorldTransform(int32 SlotIndex) const;

	//~ Grid footprint -------------------------------------------------------

	/** Bottom-left cell of the footprint: the cell under the owner location. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Platform")
	bool GetFootprintOrigin(FBDCellCoord& OutCoord) const;

	/** Cells covered by the footprint, clipped to the grid. */
	void GetFootprintCells(TArray<FBDCellCoord>& OutCells) const;

	FBDOnPlatformSlotChanged OnSlotChanged;

private:
	UBDGridSubsystem* GetGrid() const;

	/** Writes Platform over the footprint cells, remembering what was there before. */
	void ApplyFootprintToGrid();

	/** Puts the remembered states back on the cells this platform had stamped. */
	void ClearFootprintFromGrid();

	/**
	 * Catches the authoring mistake of a platform marked as outside the battle area whose
	 * footprint still reaches into the grid.
	 */
	void ValidateOutsideBattleArea();

	/** Restamps the footprint after the platform moved. */
	void RefreshFootprint();

	/** The grid was resized or reset: the remembered states are stale, so only restamp. */
	void HandleGridRebuilt();

	void UpdateStampedTransform();

	void DrawDebugSlots(const UBDGridSettings& Settings) const;
	void DrawDebugFootprint(const UBDGridSettings& Settings) const;

	/** Cells currently stamped by this platform and the state each one had before. */
	TArray<TPair<FBDCellCoord, EBDCellState>> StampedCells;

	/** Owner transform the current stamp was computed from, used to detect movement. */
	FTransform StampedTransform;

	FDelegateHandle GridRebuiltHandle;

	/** Keeps the authoring warning to one line per mistake instead of one per move. */
	bool bWarnedOutsideBattleArea = false;
};

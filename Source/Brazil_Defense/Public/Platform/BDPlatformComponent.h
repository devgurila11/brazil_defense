// Brazil Defense. Platforms that hold towers above the ground: trucks, bleachers, stages.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Grid/BDGridTypes.h"
#include "BDPlatformComponent.generated.h"

class ABDTowerBase;
class UBDGridSettings;
class UBDGridSubsystem;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

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

	/**
	 * Tells the platform exactly which cells it was placed on. The placement gesture
	 * spawns the actor at the middle of its footprint, and may have turned it, so the
	 * "pivot is the bottom-left cell" rule of an authored platform does not hold for a
	 * placed one. Restamps the grid.
	 */
	void SetPlacedFootprint(const FBDCellCoord& Origin, const FIntPoint& Footprint);

	/** Takes the placed stamp off the grid and stops restamping until SetPlacedFootprint is called again. For a piece lifted off the board. */
	void ClearPlacedFootprint();

	/** Whether this platform is currently lifted off the board and stamps nothing. */
	bool IsLifted() const { return bLifted; }

	/** Bottom-left cell of the footprint: the placed origin, or the cell under the owner location. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Platform")
	bool GetFootprintOrigin(FBDCellCoord& OutCoord) const;

	/** Cells covered by the footprint, clipped to the grid. */
	void GetFootprintCells(TArray<FBDCellCoord>& OutCells) const;

	//~ Climbing as a block ---------------------------------------------------------
	// The shooters on a platform evolve in step, never one ahead of the rest: a slot may
	// buy its next level only when every slot is filled and only for the shooters at the
	// lowest level on the platform. Four characters on a palanque therefore go 1-1-1-1,
	// then 2-2-2-2, and the fourth one to pay is what finishes a floor. A platform with
	// an empty slot evolves nothing at all.
	//
	// The reward for finishing a level is height: the platform is built one storey
	// higher, like a scaffold going up, and the shooters ride up with it. The height
	// follows the LEVEL of the block, never the act of filling it - a platform that is
	// merely manned is at level 1 and stands exactly where it was authored, and every
	// level the block gains after that adds a storey. It never sticks: whenever the
	// block moves, the height moves with it.
	//
	// That is all it is. The platform gives no damage, no range and no parameter of any
	// kind - the parameters belong to the characters, which evolve one at a time and pay
	// for it one at a time. RangeMultiplier is the height bonus the platform always had,
	// on the same number it always was; what changes is that the platform now visibly
	// stands where that number always said it did.

	/** Whether every slot has a shooter on it. A platform with no slots at all is never manned. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Platform")
	bool IsFullyManned() const;

	/** The lowest level among the occupants, which is the level the block has reached. 0 when the platform is not fully manned. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Platform")
	int32 GetBlockLevel() const;

	/**
	 * Whether a shooter standing on this platform may buy its next level right now, and
	 * why not when it may not. Asked by ABDTowerBase::CanUpgrade; the platform is the
	 * only thing that knows about the others.
	 */
	bool CanOccupantEvolve(const ABDTowerBase& Occupant, FString& OutReason) const;

	//~ The floors ------------------------------------------------------------------

	/**
	 * How high one storey lifts the platform. Level 1 is the ground floor and lifts
	 * nothing; every level above it stacks another storey under the deck, so the deck
	 * ends up FloorHeight x (level - 1) above where it was authored, and the slots with
	 * it. Level 5 is five floors: the authored one and four built under it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Platform|Floors", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float FloorHeight = 120.0f;

	/**
	 * Mesh one storey of the scaffold is built from, stacked under the deck once per
	 * level above the first. Unset falls back to the platform's own first mesh, which
	 * makes the placeholder a stack of decks: enough to read the height until the art
	 * lands.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Platform|Floors")
	TSoftObjectPtr<UStaticMesh> FloorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Platform|Floors")
	FVector FloorMeshScale = FVector::OneVector;

	/**
	 * Colour the platform is tinted with at the top level, lerped from white at level 0
	 * and written to the "TintColor" vector parameter of its materials, with "TintGlow"
	 * as a scalar beside it. A second, quieter reading of the same rank for a board seen
	 * from above, where the height of one platform among many is hard to judge. A
	 * material without those parameters is left alone and nothing breaks.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Platform|Floors")
	FLinearColor TopLevelTint = FLinearColor(1.0f, 0.72f, 0.10f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Platform|Floors", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TopLevelGlow = 1.0f;

	/** The level the platform is standing at, which is the block level and which sets its height. Recomputed every tick, so it follows an evolution, a removal or a load with nothing to wire up. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Platform")
	int32 GetVisualLevel() const { return GetBlockLevel(); }

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

	/** Reads GetBlockLevel and, when it has moved, builds the platform to that height. Game worlds only. */
	void RefreshVisualLevel();

	/** Raises the deck, stacks the storeys under it and tints the lot for a level. */
	void ApplyVisualLevel(int32 Level);

	/** Remembers where the authored meshes sat before any floor lifted them, so the lift is always from the ground up. */
	void CacheAuthoredMeshTransforms();

	/** How high the deck stands above where it was authored at a level. 0 at level 1, the ground floor. */
	float GetLiftForLevel(int32 Level) const;

	/** How high the deck stands above where it was authored at the level it is built to. */
	float GetDeckLift() const;

	void DrawDebugSlots(const UBDGridSettings& Settings) const;
	void DrawDebugFootprint(const UBDGridSettings& Settings) const;

	/** Cells currently stamped by this platform and the state each one had before. */
	TArray<TPair<FBDCellCoord, EBDCellState>> StampedCells;

	/** Owner transform the current stamp was computed from, used to detect movement. */
	FTransform StampedTransform;

	/** See SetPlacedFootprint. */
	bool bHasPlacedFootprint = false;
	bool bLifted = false;
	FBDCellCoord PlacedOrigin;
	FIntPoint PlacedFootprint = FIntPoint(1, 1);

	FDelegateHandle GridRebuiltHandle;

	/** Keeps the authoring warning to one line per mistake instead of one per move. */
	bool bWarnedOutsideBattleArea = false;

	/** The level last built, so the meshes are touched only when it moves. -1 before the first pass. */
	int32 AppliedVisualLevel = -1;

	/** The authored local transform of every mesh of the owner, taken once before the first floor went up. */
	TArray<TPair<TWeakObjectPtr<UStaticMeshComponent>, FTransform>> AuthoredMeshTransforms;
	bool bAuthoredMeshTransformsCached = false;

	/** The storeys under the deck, one instance per floor. Made on demand, never authored. */
	UPROPERTY(Transient)
	TObjectPtr<class UInstancedStaticMeshComponent> Floors;
};

// Brazil Defense. Logical gameplay grid: the matrix of cell states and the
// conversions between grid coordinates and world space.

#pragma once

#include "CoreMinimal.h"
#include "Grid/BDGridTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDGridSubsystem.generated.h"

class UBDGridSettings;

/** Broadcast after the grid has been resized or fully reset. */
DECLARE_MULTICAST_DELEGATE(FBDOnGridRebuilt);

/** Broadcast whenever a single cell changes state. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBDOnCellStateChanged, const FBDCellCoord& /*Coord*/, EBDCellState /*NewState*/);

/**
 * Owns the logical grid for the current world.
 * Holds no geometry and does no drawing: it is the pure data layer every other
 * grid system reads from. Lives in editor worlds as well so the layout can be
 * validated against the level before playing.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDGridSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/** Convenience accessor. Returns null when the world context has no world. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid", meta = (WorldContext = "WorldContextObject"))
	static UBDGridSubsystem* Get(const UObject* WorldContextObject);

	//~ Layout ---------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	int32 GetSizeX() const { return SizeX; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	int32 GetSizeY() const { return SizeY; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	float GetCellSize() const { return CellSize; }

	/** World location of the bottom-left corner of cell (0,0). */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	FVector GetOrigin() const { return Origin; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	int32 GetCellCount() const { return Cells.Num(); }

	/** Rereads the settings and, when the layout changed, resets every cell to Free. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Grid")
	void RebuildFromSettings();

	/** Sets every cell back to Free without touching the layout. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Grid")
	void ResetAllCells();

	//~ Cell access ----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	bool IsValidCoord(const FBDCellCoord& Coord) const;

	/** Returns the state of a cell. Out of range coordinates read as Blocked. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	EBDCellState GetCellState(const FBDCellCoord& Coord) const;

	/** @return false when the coordinate is out of range or the state did not change. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Grid")
	bool SetCellState(const FBDCellCoord& Coord, EBDCellState NewState);

	/** A unit can move through this cell. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	bool IsWalkable(const FBDCellCoord& Coord) const;

	/** The player is allowed to place something on this cell. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	bool IsBuildable(const FBDCellCoord& Coord) const;

	/** This cell holds something the player put there, so it can be removed and refunded. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	bool IsPlayerPlaced(const FBDCellCoord& Coord) const;

	//~ State rules ----------------------------------------------------------
	// Single place where the meaning of each state lives. Everything else asks these.

	/** Towers do not block: they occupy a cell but creeps walk straight through them. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	static bool IsWalkableState(EBDCellState State);

	/** Only a Free cell accepts new content. Spawn and Goal are never occupiable. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	static bool IsBuildableState(EBDCellState State);

	/** Player placed content, as opposed to permanent level layout. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	static bool IsPlayerPlacedState(EBDCellState State);

	//~ Conversions ----------------------------------------------------------

	/** World location of the center of a cell, at the grid origin height. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	FVector CellToWorld(const FBDCellCoord& Coord) const;

	/** World location of the bottom-left corner of a cell. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	FVector CellCornerToWorld(const FBDCellCoord& Coord) const;

	/** Projects a world location onto the grid plane. @return false when it falls outside the grid. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Grid")
	bool WorldToCell(const FVector& WorldLocation, FBDCellCoord& OutCoord) const;

	/** Same as WorldToCell but keeps coordinates outside the grid, without validating them. */
	FBDCellCoord WorldToCellUnclamped(const FVector& WorldLocation) const;

	//~ Notifications --------------------------------------------------------

	FBDOnGridRebuilt OnGridRebuilt;
	FBDOnCellStateChanged OnCellStateChanged;

private:
	/** Flat index of a coordinate. Assumes the coordinate is valid. */
	int32 CoordToIndex(const FBDCellCoord& Coord) const { return Coord.Y * SizeX + Coord.X; }

	/** Applies a layout and sizes the cell array. Returns true when the layout actually changed. */
	bool ApplyLayout(const UBDGridSettings& Settings);

#if WITH_EDITOR
	/** Keeps the runtime grid in sync while the layout is being tuned in Project Settings. */
	void HandleSettingsChanged(UObject* ChangedObject, struct FPropertyChangedEvent& PropertyChangedEvent);

	FDelegateHandle SettingsChangedHandle;
#endif

	/** Row major matrix of cell states, indexed by Y * SizeX + X. */
	TArray<EBDCellState> Cells;

	int32 SizeX = 0;
	int32 SizeY = 0;
	float CellSize = 0.0f;
	FVector Origin = FVector::ZeroVector;
};

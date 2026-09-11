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

/** Broadcast whenever an edge is blocked or freed. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBDOnEdgeBlockedChanged, const FBDEdgeCoord& /*Edge*/, bool /*bBlocked*/);

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

	/**
	 * Bumped on every change to a cell state, to an edge or to the layout.
	 * Lets a caller tell whether an answer it cached about this grid went stale,
	 * without having to compare the cells themselves.
	 */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	int32 GetVersion() const { return Version; }

	/** Rereads the settings and, when the layout changed, resets every cell to Free. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Grid")
	void RebuildFromSettings();

	/** Sets every cell back to Free and frees every edge, without touching the layout. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Grid")
	void ResetAllCells();

	/**
	 * Writes an authored layout onto the cells. This is how the level reaches the grid:
	 * every world starts with all cells Free, so Spawn, Goal and permanent scenery only
	 * exist here because ABDGridLayoutActor put them here.
	 *
	 * Only the listed cells are touched. The layout is not treated as the whole truth of
	 * the board, so applying it does not erase what other systems own: platform
	 * footprints and generated obstacles keep their cells.
	 *
	 * @return number of cells that actually changed state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Grid")
	int32 ApplyAuthoredLayout(const TMap<FBDCellCoord, EBDCellState>& Layout);

	/** Blocks an authored set of edges. Same contract as ApplyAuthoredLayout. @return number of edges that changed. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Grid")
	int32 ApplyAuthoredEdges(const TArray<FBDEdgeCoord>& Edges);

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

	//~ Edge access ----------------------------------------------------------
	// Edges are a second layer over the cells. A cell state says whether a unit may
	// stand there; a blocked edge says it may not cross between two cells that are
	// both perfectly standable. Dividers live here and never write a cell.

	/** Whether a unit may not step from one cell straight into the other. False for cells that are not adjacent. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	bool IsEdgeBlocked(const FBDCellCoord& From, const FBDCellCoord& To) const;

	bool IsEdgeBlockedAt(const FBDEdgeCoord& Edge) const;

	/**
	 * An edge can only be blocked when it separates two cells of the grid. The outer
	 * border has nothing on the other side, so blocking it would be meaningless.
	 */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	bool CanBlockEdge(const FBDEdgeCoord& Edge) const;

	/** @return false when the edge cannot be blocked or its state did not change. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Grid")
	bool SetEdgeBlocked(const FBDEdgeCoord& Edge, bool bBlocked);

	/**
	 * The edges of a straight run of fence: Length edges of the same direction, one per
	 * cell along the line they form. +X edges line up along Y and +Y edges along X.
	 * Edges outside the grid are returned as well; CanBlockEdge is what refuses them.
	 */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	TArray<FBDEdgeCoord> GetEdgesForSegment(const FBDCellCoord& Start, int32 Length, uint8 Direction) const;

	/** Every edge currently blocked, for drawing and for capturing the layout. */
	void GetBlockedEdges(TArray<FBDEdgeCoord>& OutEdges) const;

	/** Flat index of an edge in the edge bit array, or INDEX_NONE when its cell is outside the grid. */
	int32 EdgeToIndex(const FBDEdgeCoord& Edge) const;

	int32 GetEdgeCount() const { return BlockedEdges.Num(); }

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

	/** World location of the middle of an edge: halfway between the centers of the two cells it separates. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Grid")
	FVector EdgeToWorld(const FBDEdgeCoord& Edge) const;

	/** The two ends of an edge on the grid plane, for drawing it as a line. */
	void EdgeEndpointsToWorld(const FBDEdgeCoord& Edge, FVector& OutStart, FVector& OutEnd) const;

	//~ Notifications --------------------------------------------------------

	FBDOnGridRebuilt OnGridRebuilt;
	FBDOnCellStateChanged OnCellStateChanged;
	FBDOnEdgeBlockedChanged OnEdgeBlockedChanged;

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

	/** Two bits per cell, its +X and +Y edges, indexed by CoordToIndex * 2 + Direction. */
	TBitArray<> BlockedEdges;

	/** See GetVersion. Starts at 1 so a cached version of 0 never looks current. */
	int32 Version = 1;

	int32 SizeX = 0;
	int32 SizeY = 0;
	float CellSize = 0.0f;
	FVector Origin = FVector::ZeroVector;
};

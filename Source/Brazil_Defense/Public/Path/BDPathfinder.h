// Brazil Defense. A* over the logical grid, plus the blocking validation used
// before the player is allowed to place something.

#pragma once

#include "CoreMinimal.h"
#include "Grid/BDGridTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDPathfinder.generated.h"

class UBDGridSubsystem;

/**
 * Pathfinding over the cell states of UBDGridSubsystem.
 *
 * Movement is four directional: a maze made of dividers has to actually close, and
 * diagonal steps would let creeps slip through the corner between two blocked cells.
 * Every step costs the same, so the Manhattan distance is an admissible heuristic.
 *
 * A step is allowed when the destination cell is walkable, per
 * UBDGridSubsystem::IsWalkableState, and the edge between the two cells is not
 * blocked. Cells and edges are separate graphs: a Tower does not obstruct a path, a
 * Platform does, and a divider obstructs the crossing without touching either cell.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDPathfinder : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return true; }
	//~ End FTickableGameObject interface

	/** Convenience accessor. Returns null when the world context has no world. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Path", meta = (WorldContext = "WorldContextObject"))
	static UBDPathfinder* Get(const UObject* WorldContextObject);

	/**
	 * Shortest path from Start to Goal over walkable cells.
	 * @param OutPath filled from Start to Goal inclusive, empty when there is no path.
	 * @return false when either end is out of the grid, is not walkable, or is unreachable.
	 */
	bool FindPath(const UBDGridSubsystem* Grid, FBDCellCoord Start, FBDCellCoord Goal, TArray<FBDCellCoord>& OutPath) const;

	/** Same search without rebuilding the path. Answers only whether one exists. */
	bool HasAnyPath(const UBDGridSubsystem* Grid, FBDCellCoord Start, FBDCellCoord Goal) const;

	/**
	 * Whether placing a footprint here would cut the creeps off.
	 *
	 * The footprint cells are treated as blocked for the duration of the check only:
	 * the grid is never written to, so a rejected placement leaves no trace. Every
	 * Spawn cell must keep a path to the Goal.
	 *
	 * Answers are cached, because this runs every frame while the player hovers a
	 * piece around and the board only changes when something is actually placed. The
	 * cache is dropped when the hovered cell, the footprint or the grid version moves.
	 *
	 * @param Origin bottom-left cell of the footprint, the same corner UBDPlatformComponent uses.
	 * @return true when the placement would leave some spawn with no way to the goal.
	 */
	bool WouldBlockPath(const UBDGridSubsystem* Grid, FBDCellCoord Origin, FIntPoint Footprint) const;

	/**
	 * Whether fencing off these edges would cut the creeps off. The edge counterpart of
	 * WouldBlockPath: same overlay, same cache, the grid is never written to.
	 * A player is free to fence a whole cell in; that wastes pieces, not paths, and is
	 * their call. Only a spawn losing every way to the goal is refused.
	 */
	bool WouldBlockPathEdges(const UBDGridSubsystem* Grid, const TArray<FBDEdgeCoord>& Candidate) const;

	/** Whether the last WouldBlockPath call was answered from the cache instead of searching. */
	bool WasLastBlockCheckCached() const { return bLastBlockCheckWasCached; }

	/**
	 * Collects every cell currently holding a given state.
	 * Public because finding the Spawn and Goal cells is the first thing anything that
	 * reasons about the board has to do, obstacle generation included.
	 */
	static void GatherCellsWithState(const UBDGridSubsystem& Grid, EBDCellState State, TArray<FBDCellCoord>& OutCells);

	//~ Last query, for debug drawing and profiling -------------------------

	const TArray<FBDCellCoord>& GetLastPath() const { return LastPath; }
	int32 GetLastVisitedCount() const { return LastVisitedCount; }
	double GetLastSearchMicroseconds() const { return LastSearchMicroseconds; }

private:
	/**
	 * The single A* implementation behind every public entry point.
	 *
	 * @param BlockedOverride cells to treat as blocked on top of their real state, or null.
	 * @param BlockedEdgeOverride edges to treat as blocked on top of their real state, indexed like the grid's, or null.
	 * @param OutPath when null the path is not reconstructed, which is what HasAnyPath wants.
	 */
	bool RunSearch(const UBDGridSubsystem& Grid, const FBDCellCoord& Start, const FBDCellCoord& Goal,
		const TBitArray<>* BlockedOverride, const TBitArray<>* BlockedEdgeOverride, TArray<FBDCellCoord>* OutPath) const;

	/**
	 * Finds the spawns and the single goal every blocking check validates against.
	 * @return false when the board has no premise to check, which is logged as an error.
	 */
	static bool GatherSpawnsAndGoal(const UBDGridSubsystem& Grid, TArray<FBDCellCoord>& OutSpawns, FBDCellCoord& OutGoal);

	/** Runs the overlaid search from every spawn. @return true when some spawn lost its way to the goal. */
	bool AnySpawnCutOff(const UBDGridSubsystem& Grid, const TArray<FBDCellCoord>& Spawns, const FBDCellCoord& Goal,
		const TBitArray<>* BlockedOverride, const TBitArray<>* BlockedEdgeOverride) const;

	/** Stores a WouldBlockPath answer and returns it, so the callers stay one-liners. */
	bool CacheBlockResult(const UBDGridSubsystem* Grid, const FBDCellCoord& Origin,
		const FIntPoint& Footprint, bool bResult) const;

	/** Same for WouldBlockPathEdges. The two caches are separate so alternating pieces do not evict each other. */
	bool CacheEdgeBlockResult(const UBDGridSubsystem* Grid, const TArray<FBDEdgeCoord>& Candidate, bool bResult) const;

	void DrawLastPath() const;

	/**
	 * Results of the last query. Mutable because caching what a const search just did
	 * is bookkeeping for the debug view, not part of the observable pathfinding state.
	 */
	mutable TArray<FBDCellCoord> LastPath;
	mutable int32 LastVisitedCount = 0;
	mutable double LastSearchMicroseconds = 0.0;

	/**
	 * Memo of the last WouldBlockPath answer. A search costs tens of microseconds,
	 * which is nothing once but adds up when the same question is asked every frame
	 * of a hover. The grid version is what makes this safe: any Place or Remove bumps
	 * it, so a stale answer can never be served.
	 */
	mutable TWeakObjectPtr<const UBDGridSubsystem> CachedBlockGrid;
	mutable FBDCellCoord CachedBlockOrigin;
	mutable FIntPoint CachedBlockFootprint = FIntPoint::ZeroValue;
	mutable int32 CachedBlockGridVersion = 0;
	mutable bool bCachedBlockResult = false;
	mutable bool bHasCachedBlockResult = false;
	mutable bool bLastBlockCheckWasCached = false;

	mutable TWeakObjectPtr<const UBDGridSubsystem> CachedEdgeBlockGrid;
	mutable TArray<FBDEdgeCoord> CachedEdgeBlockCandidate;
	mutable int32 CachedEdgeBlockGridVersion = 0;
	mutable bool bCachedEdgeBlockResult = false;
	mutable bool bHasCachedEdgeBlockResult = false;
};

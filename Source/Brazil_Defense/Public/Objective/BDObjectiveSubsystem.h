// Brazil Defense. Putting the urn on the board: the Goal cell follows it.

#pragma once

#include "CoreMinimal.h"
#include "Grid/BDGridTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDObjectiveSubsystem.generated.h"

class ABDObjective;
class UBDGridSubsystem;
class UBDPathfinder;
class UStaticMesh;

/** Why a cell cannot take the urn. */
UENUM(BlueprintType)
enum class EBDObjectiveRefusal : uint8
{
	None,
	/** The board has no grid, or the cell is outside it. */
	OffGrid,
	/** The cell is outside the zone of UBDObjectiveSettings. */
	OutOfZone,
	/** The cell is not Free. */
	CellTaken,
	/** Some spawn cannot walk to the cell. */
	Unreachable
};

/**
 * Owns where the urn is. The player puts it down during the building phase, before any
 * defense, and the Goal cell of the grid is a consequence of that: nothing authors a
 * Goal on the map any more. Placing the urn writes its cell as Goal, frees the previous
 * one, moves the ABDObjective actor there (or spawns it), and the routes of every spawn
 * follow through the grid notifications.
 *
 * The gesture itself is UBDPlacementComponent's; this is what it calls once the spot is
 * legal, and what the console calls to skip the gesture.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDObjectiveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~ End USubsystem interface

	/** Convenience accessor. Returns null when the world context has no world. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Objective", meta = (WorldContext = "WorldContextObject"))
	static UBDObjectiveSubsystem* Get(const UObject* WorldContextObject);

	/** Whether a cell may take the urn right now, and why not when it may not. Runs every hover frame; the reachability part is cached by the pathfinder. */
	EBDObjectiveRefusal EvaluateCell(const FBDCellCoord& Coord) const;

	/**
	 * Puts the urn on a cell. Refuses with the same reasons EvaluateCell gives.
	 *
	 * @param ActorClass class of the urn actor to spawn when the level has none. Null or
	 *                   not an ABDObjective falls back to ABDObjective itself.
	 * @param Mesh       given to the actor when it brings no mesh of its own, so a bare
	 *                   ABDObjective still shows the urn.
	 * @param OutRefusal why it was refused, None on success.
	 */
	bool PlaceObjective(const FBDCellCoord& Coord, UClass* ActorClass, UStaticMesh* Mesh, EBDObjectiveRefusal& OutRefusal);

	/** Debug: takes the urn off the board. The Goal cell goes back to Free; the actor stays where it is, unplaced. */
	void ClearObjective();

	/** Whether the urn has been placed this match. Nothing else may be placed before it. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Objective")
	bool IsPlaced() const { return bPlaced; }

	/** The Goal cell, meaningful only once placed. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Objective")
	FBDCellCoord GetGoalCell() const { return GoalCell; }

	/** The urn actor of the level, placed or not. Null when there is none yet. */
	ABDObjective* GetObjective() const;

	/** Paints the zone the urn may stand in. Called by the placement gesture while the urn is held. */
	void DrawZone() const;

	static FString DescribeRefusal(EBDObjectiveRefusal Refusal);

private:
	UBDGridSubsystem* GetGrid() const;
	const UBDPathfinder* GetPathfinder() const;

	/** Floor Z under a point, traced with the placement settings. Falls back to the grid plane. */
	float ResolveGroundZ(const FVector& Point) const;

	/** Weak: the level owns it. Re-found when it goes stale. */
	mutable TWeakObjectPtr<ABDObjective> Objective;

	FBDCellCoord GoalCell;
	bool bPlaced = false;
};

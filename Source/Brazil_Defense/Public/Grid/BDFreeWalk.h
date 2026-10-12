// Brazil Defense. Walking free over the board: the rules shared by everyone who is not a creep.

#pragma once

#include "CoreMinimal.h"
#include "Grid/BDGridTypes.h"

class AActor;
class UBDGridSubsystem;
class UWorld;

/**
 * How a man who is not a creep crosses the board: anywhere in continuous space, through
 * towers, platforms and buildings, stopped only by the cells nobody can stand on
 * (Blocked) and by the dividers. The Agent walks by it and so do the ministers, who step
 * over the dividers as well (bOverDividers).
 *
 * A value made on the spot for one question: the world, the grid, and the actors the
 * floor trace must look through (the walker himself, the building he stands by).
 */
struct BRAZIL_DEFENSE_API FBDFreeWalk
{
	/** Sideways clearance kept from a divider or a blocked cell, as a share of a cell. */
	static constexpr float BodyMargin = 0.2f;

	/** How finely a straight line is checked, as a share of a cell. */
	static constexpr float LineCheckStep = 0.2f;

	/** @param bInOverDividers the dividers stop him not: he steps over them. Blocked cells still do. */
	FBDFreeWalk(const UWorld* InWorld, const UBDGridSubsystem* InGrid, TArray<const AActor*> InIgnored = {}, bool bInOverDividers = false);

	/** Whether a man may stand on the cell: on the board and not Blocked. */
	bool CanStand(const FBDCellCoord& Coord) const;

	/** Whether a man may step between two neighbouring cells: the next can be stood on and no divider is between, unless he steps over them. */
	bool CanStep(const FBDCellCoord& From, const FBDCellCoord& To) const;

	/**
	 * Whether the straight line between two points crosses only cells he may step through.
	 * A corner is passed when either way round it is open. A margin checks two more lines,
	 * that far to each side, so the body clears what the center would graze.
	 */
	bool IsLineClear(const FVector& From, const FVector& To, float Margin) const;

	/** The cells of the shortest way between two cells, both ends included. Empty when there is none. */
	TArray<FBDCellCoord> FindWay(const FBDCellCoord& From, const FBDCellCoord& To) const;

	/**
	 * The points to walk from Here to Goal: the goal itself when it is in plain sight, else
	 * the way around by the cells pulled straight. Empty when there is no way at all.
	 */
	TArray<FVector> PlanWay(const FVector& Here, const FVector& Goal) const;

	/** The middle of a cell on the floor under it, not on the board plane. */
	FVector CellPoint(const FBDCellCoord& Coord) const;

	/** The floor under a point, traced on the ground channel. @return false when nothing is there. */
	bool TraceGround(const FVector& Point, float& OutGroundZ) const;

private:
	const UWorld* World = nullptr;
	const UBDGridSubsystem* Grid = nullptr;
	TArray<const AActor*> Ignored;
	bool bOverDividers = false;
};

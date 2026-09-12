// Brazil Defense. Core types shared by every grid related system.

#pragma once

#include "CoreMinimal.h"
#include "BDGridTypes.generated.h"

/**
 * Logical state of a single grid cell.
 * Movement and building rules are derived from this state, never from geometry.
 */
UENUM(BlueprintType)
enum class EBDCellState : uint8
{
	/** Free to build on and free to walk through. */
	Free UMETA(DisplayName = "Free"),

	/** Holds a tower. Occupied for building, but creeps walk straight through it. */
	Tower UMETA(DisplayName = "Tower"),

	/** Platform placed by the player. Blocks movement, can be removed and refunded. */
	Platform UMETA(DisplayName = "Platform"),

	/** Permanent level layout. Blocks movement and can never be removed. */
	Blocked UMETA(DisplayName = "Blocked"),

	/** Enemy entry point. Walkable, never occupiable. */
	Spawn UMETA(DisplayName = "Spawn"),

	/** Objective the enemies walk towards. Walkable, never occupiable. */
	Goal UMETA(DisplayName = "Goal"),

	/** Number of valid states. Keep last. */
	Count UMETA(Hidden)
};

/**
 * What a player placed piece is, for budgets and removal rules.
 * Not a cell state on purpose: a divider lives on an edge between cells and never
 * writes a cell, so the match cannot key its budget on EBDCellState alone.
 */
UENUM(BlueprintType)
enum class EBDPieceKind : uint8
{
	Divider UMETA(DisplayName = "Divider"),
	Platform UMETA(DisplayName = "Platform"),
	Tower UMETA(DisplayName = "Tower"),

	/** The urn. Placed by the player before anything else, exactly once; its cell becomes the Goal. */
	Objective UMETA(DisplayName = "Objective")
};

/**
 * Integer coordinate of a cell inside the grid.
 * X grows along the world +X axis, Y grows along the world +Y axis,
 * both starting at the grid origin (bottom-left corner).
 */
USTRUCT(BlueprintType)
struct FBDCellCoord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	int32 X = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	int32 Y = 0;

	FBDCellCoord() = default;

	FBDCellCoord(const int32 InX, const int32 InY)
		: X(InX)
		, Y(InY)
	{
	}

	bool operator==(const FBDCellCoord& Other) const
	{
		return X == Other.X && Y == Other.Y;
	}

	bool operator!=(const FBDCellCoord& Other) const
	{
		return !(*this == Other);
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%d,%d"), X, Y);
	}

	friend uint32 GetTypeHash(const FBDCellCoord& Coord)
	{
		return HashCombine(::GetTypeHash(Coord.X), ::GetTypeHash(Coord.Y));
	}
};

/**
 * One edge between two four-adjacent cells: the side of Cell facing +X or +Y.
 *
 * Only two directions exist because the -X side of a cell is the +X side of its
 * neighbour. Between() normalizes a pair of cells to that single representation, so
 * the same edge always hashes and compares the same whichever cell it was reached from.
 *
 * A blocked edge stops movement across it without touching either cell: cells say
 * where a unit may stand, edges say where it may cross. They are different graphs.
 */
USTRUCT(BlueprintType)
struct FBDEdgeCoord
{
	GENERATED_BODY()

	static constexpr uint8 DirectionX = 0;
	static constexpr uint8 DirectionY = 1;
	static constexpr int32 DirectionCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
	FBDCellCoord Cell;

	/** 0 = the +X side of Cell, 1 = the +Y side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid", meta = (ClampMin = "0", ClampMax = "1"))
	uint8 Direction = DirectionX;

	FBDEdgeCoord() = default;

	FBDEdgeCoord(const FBDCellCoord& InCell, const uint8 InDirection)
		: Cell(InCell)
		, Direction(InDirection)
	{
	}

	/**
	 * The edge separating two four-adjacent cells, normalized onto the lower one.
	 * @param bOutAdjacent false when the cells do not share an edge; the result is then meaningless.
	 */
	static FBDEdgeCoord Between(const FBDCellCoord& A, const FBDCellCoord& B, bool& bOutAdjacent)
	{
		const int32 DeltaX = B.X - A.X;
		const int32 DeltaY = B.Y - A.Y;
		bOutAdjacent = FMath::Abs(DeltaX) + FMath::Abs(DeltaY) == 1;

		if (DeltaX != 0)
		{
			return FBDEdgeCoord(DeltaX > 0 ? A : B, DirectionX);
		}

		return FBDEdgeCoord(DeltaY > 0 ? A : B, DirectionY);
	}

	/** The cell on the far side of the edge: Cell's +X or +Y neighbour. */
	FBDCellCoord GetOtherCell() const
	{
		return Direction == DirectionX
			? FBDCellCoord(Cell.X + 1, Cell.Y)
			: FBDCellCoord(Cell.X, Cell.Y + 1);
	}

	bool operator==(const FBDEdgeCoord& Other) const
	{
		return Cell == Other.Cell && Direction == Other.Direction;
	}

	bool operator!=(const FBDEdgeCoord& Other) const
	{
		return !(*this == Other);
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("%s|%s"), *Cell.ToString(), Direction == DirectionX ? TEXT("+X") : TEXT("+Y"));
	}

	friend uint32 GetTypeHash(const FBDEdgeCoord& Edge)
	{
		return HashCombine(GetTypeHash(Edge.Cell), ::GetTypeHash(Edge.Direction));
	}
};

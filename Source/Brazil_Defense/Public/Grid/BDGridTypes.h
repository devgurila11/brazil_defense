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

	/** Divider placed by the player. Blocks movement, can be removed. */
	Divider UMETA(DisplayName = "Divider"),

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

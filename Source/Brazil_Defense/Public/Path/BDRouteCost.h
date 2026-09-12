// Brazil Defense. The cost a single creep sees on the board, so no two walk the same line.

#pragma once

#include "CoreMinimal.h"

/**
 * Per cell cost multiplier of one route search, drawn from a seed.
 *
 * A* with a uniform cost always answers the same shortest path, and a horde that repeats
 * it wave after wave is a horde the player only has to read once. Handing every creep a
 * cost map of its own, each walkable cell somewhere between 1 and Variance, spreads the
 * same wave over every corridor that is nearly as short as the shortest one. It never
 * finds a way the maze does not offer: a corridor the player closed costs infinity in
 * every map.
 *
 * The map is not stored; it is the hash of the seed and the cell, computed when a cell
 * is expanded. Same seed, same cell, same multiplier, for the whole life of the creep,
 * so a reroute after the board changed keeps its preferences. Nothing to allocate, and
 * a hundred creeps carry a hundred integers instead of a hundred grids.
 *
 * The blocking validation never uses one of these: whether a fence closes the maze is
 * about a path existing, not about which path, and a seeded answer would accept a fence
 * in one match and refuse it in the next.
 */
struct FBDRouteCost
{
	/** What the map is drawn from. Ignored while Variance is 1. */
	int32 Seed = 0;

	/** Highest multiplier a cell may get. 1 is the uniform cost, which is the shortest path as always. */
	float Variance = 1.0f;

	/** Whether this is the plain shortest path search. */
	bool IsUniform() const
	{
		return Variance <= 1.0f + UE_KINDA_SMALL_NUMBER;
	}

	/**
	 * Cost of stepping into a cell, relative to the uniform step, in [1, Variance].
	 * @param CellIndex the grid's own index of the cell, Y x SizeX + X.
	 */
	float MultiplierAt(const int32 CellIndex) const
	{
		if (IsUniform())
		{
			return 1.0f;
		}

		// The murmur3 finaliser over seed and cell: neighbouring cells and neighbouring
		// seeds must land nowhere near each other, or every map would favour one side of
		// the board.
		uint32 Hash = static_cast<uint32>(Seed) ^ (static_cast<uint32>(CellIndex) * 0x9E3779B1u);
		Hash ^= Hash >> 16;
		Hash *= 0x85EBCA6Bu;
		Hash ^= Hash >> 13;
		Hash *= 0xC2B2AE35u;
		Hash ^= Hash >> 16;

		const float Unit = static_cast<float>(Hash & 0xFFFFFFu) / static_cast<float>(0x1000000u);
		return 1.0f + Unit * (Variance - 1.0f);
	}
};

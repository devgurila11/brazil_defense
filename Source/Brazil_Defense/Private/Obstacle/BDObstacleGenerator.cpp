// Brazil Defense. Seeded, validated generation of the permanent obstacles of a board.

#include "Obstacle/BDObstacleGenerator.h"

#include "BDLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Math/RandomStream.h"
#include "Obstacle/BDObstacleSettings.h"
#include "Path/BDPathfinder.h"

UBDObstacleGenerator* UBDObstacleGenerator::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	return World != nullptr ? World->GetSubsystem<UBDObstacleGenerator>() : nullptr;
}

const UBDPathfinder* UBDObstacleGenerator::GetPathfinder() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDPathfinder>() : nullptr;
}

void UBDObstacleGenerator::BuildCandidates(const UBDGridSubsystem& Grid, const TArray<FBDCellCoord>& Spawns,
	const TArray<FBDCellCoord>& Goals, const int32 Clearance, TArray<FBDCellCoord>& OutCandidates)
{
	OutCandidates.Reset();

	auto IsTooClose = [Clearance](const FBDCellCoord& Cell, const TArray<FBDCellCoord>& Protected)
	{
		for (const FBDCellCoord& Other : Protected)
		{
			// Chebyshev distance: a square of free room around the cell, diagonals included.
			if (FMath::Max(FMath::Abs(Cell.X - Other.X), FMath::Abs(Cell.Y - Other.Y)) <= Clearance)
			{
				return true;
			}
		}

		return false;
	};

	for (int32 Y = 0; Y < Grid.GetSizeY(); ++Y)
	{
		for (int32 X = 0; X < Grid.GetSizeX(); ++X)
		{
			const FBDCellCoord Coord(X, Y);
			if (Grid.GetCellState(Coord) != EBDCellState::Free)
			{
				continue;
			}

			if (IsTooClose(Coord, Spawns) || IsTooClose(Coord, Goals))
			{
				continue;
			}

			OutCandidates.Add(Coord);
		}
	}
}

void UBDObstacleGenerator::PickCells(FRandomStream& Stream, const TArray<FBDCellCoord>& Candidates,
	const int32 Count, TArray<FBDCellCoord>& OutPicked)
{
	OutPicked.Reset();

	TArray<FBDCellCoord> Pool = Candidates;
	const int32 Take = FMath::Clamp(Count, 0, Pool.Num());

	// Partial Fisher-Yates: every draw is distinct and every attempt consumes the stream
	// in the same order, which is what keeps a seed reproducible across runs.
	for (int32 Index = 0; Index < Take; ++Index)
	{
		const int32 Swap = Stream.RandRange(Index, Pool.Num() - 1);
		Pool.Swap(Index, Swap);
		OutPicked.Add(Pool[Index]);
	}
}

void UBDObstacleGenerator::WriteCells(UBDGridSubsystem& Grid, const TArray<FBDCellCoord>& Cells, const EBDCellState State)
{
	for (const FBDCellCoord& Coord : Cells)
	{
		Grid.SetCellState(Coord, State);
	}
}

float UBDObstacleGenerator::ComputeFreeRatio(const UBDGridSubsystem& Grid)
{
	const int32 CellCount = Grid.GetCellCount();
	if (CellCount <= 0)
	{
		return 0.0f;
	}

	int32 FreeCount = 0;
	for (int32 Y = 0; Y < Grid.GetSizeY(); ++Y)
	{
		for (int32 X = 0; X < Grid.GetSizeX(); ++X)
		{
			FreeCount += Grid.GetCellState(FBDCellCoord(X, Y)) == EBDCellState::Free ? 1 : 0;
		}
	}

	return static_cast<float>(FreeCount) / static_cast<float>(CellCount);
}

bool UBDObstacleGenerator::IsLayoutPlayable(const UBDGridSubsystem& Grid, const TArray<FBDCellCoord>& Spawns,
	const TArray<FBDCellCoord>& Goals, FString& OutReason)
{
	const UBDPathfinder* Pathfinder = GetPathfinder();
	if (Pathfinder == nullptr)
	{
		OutReason = TEXT("no pathfinder in this world");
		return false;
	}

	const UBDObstacleSettings& Settings = UBDObstacleSettings::Get();

	const float FreeRatio = ComputeFreeRatio(Grid);
	if (FreeRatio < Settings.MinFreeRatio)
	{
		OutReason = FString::Printf(TEXT("only %.0f%% of the board is free, minimum is %.0f%%"),
			FreeRatio * 100.0f, Settings.MinFreeRatio * 100.0f);
		return false;
	}

	int32 ShortestRoute = MAX_int32;
	TArray<FBDCellCoord> Path;

	for (const FBDCellCoord& Spawn : Spawns)
	{
		// The route that matters is the one the creeps would actually take, so a spawn is
		// measured by its shortest way out, not by whichever goal happens to be first.
		int32 BestForSpawn = MAX_int32;
		for (const FBDCellCoord& Goal : Goals)
		{
			if (Pathfinder->FindPath(&Grid, Spawn, Goal, Path))
			{
				BestForSpawn = FMath::Min(BestForSpawn, Path.Num());
			}
		}

		if (BestForSpawn == MAX_int32)
		{
			OutReason = FString::Printf(TEXT("spawn %s has no route to any goal"), *Spawn.ToString());
			return false;
		}

		if (BestForSpawn < Settings.MinPathLength)
		{
			OutReason = FString::Printf(TEXT("spawn %s reaches a goal in %d cells, minimum is %d"),
				*Spawn.ToString(), BestForSpawn, Settings.MinPathLength);
			return false;
		}

		if (BestForSpawn > Settings.MaxPathLength)
		{
			OutReason = FString::Printf(TEXT("spawn %s needs %d cells to reach a goal, maximum is %d"),
				*Spawn.ToString(), BestForSpawn, Settings.MaxPathLength);
			return false;
		}

		ShortestRoute = FMath::Min(ShortestRoute, BestForSpawn);
	}

	LastPathLength = ShortestRoute == MAX_int32 ? 0 : ShortestRoute;
	return true;
}

int32 UBDObstacleGenerator::ClearGeneratedObstacles(UBDGridSubsystem* Grid)
{
	if (Grid == nullptr || GeneratedCells.Num() == 0)
	{
		GeneratedCells.Reset();
		return 0;
	}

	int32 ClearedCount = 0;
	for (const FBDCellCoord& Coord : GeneratedCells)
	{
		// Only cells still holding our obstacle go back to Free. Anything that took the
		// cell in the meantime is not ours to hand back.
		if (Grid->GetCellState(Coord) == EBDCellState::Blocked)
		{
			ClearedCount += Grid->SetCellState(Coord, EBDCellState::Free) ? 1 : 0;
		}
	}

	GeneratedCells.Reset();
	return ClearedCount;
}

bool UBDObstacleGenerator::GenerateObstacles(UBDGridSubsystem* Grid, const int32 Seed, const int32 ObstacleCountOverride)
{
	LastSeed = Seed;
	LastAttemptCount = 0;
	LastPathLength = 0;
	bLastGenerationValidated = false;

	if (Grid == nullptr)
	{
		UE_LOG(LogBDObstacle, Error, TEXT("GenerateObstacles was given no grid."));
		return false;
	}

	ClearGeneratedObstacles(Grid);

	TArray<FBDCellCoord> Spawns;
	TArray<FBDCellCoord> Goals;
	UBDPathfinder::GatherCellsWithState(*Grid, EBDCellState::Spawn, Spawns);
	UBDPathfinder::GatherCellsWithState(*Grid, EBDCellState::Goal, Goals);

	if (Spawns.Num() == 0 || Goals.Num() == 0)
	{
		UE_LOG(LogBDObstacle, Error,
			TEXT("The board has %d spawn(s) and %d goal(s), so no layout could be validated and nothing was placed. "
				 "Author them first: BD.Grid.SetCells then BD.Grid.SaveLayout."),
			Spawns.Num(), Goals.Num());
		return false;
	}

	const UBDObstacleSettings& Settings = UBDObstacleSettings::Get();

	TArray<FBDCellCoord> Candidates;
	BuildCandidates(*Grid, Spawns, Goals, FMath::Max(0, Settings.ClearanceFromSpawnGoal), Candidates);

	const int32 RequestedCount = ObstacleCountOverride >= 0 ? ObstacleCountOverride : Settings.ObstacleCount;
	const int32 Count = FMath::Min(RequestedCount, Candidates.Num());
	if (Count < RequestedCount)
	{
		UE_LOG(LogBDObstacle, Warning,
			TEXT("Asked for %d obstacles but only %d cell(s) are free and clear of the spawns and goals."),
			RequestedCount, Candidates.Num());
	}

	FRandomStream Stream(Seed);
	TArray<FBDCellCoord> Picked;
	FString Reason;

	const int32 MaxAttempts = FMath::Max(1, Settings.MaxAttempts);
	for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
	{
		LastAttemptCount = Attempt;

		PickCells(Stream, Candidates, Count, Picked);
		WriteCells(*Grid, Picked, EBDCellState::Blocked);

		if (IsLayoutPlayable(*Grid, Spawns, Goals, Reason))
		{
			GeneratedCells = Picked;
			bLastGenerationValidated = true;

			UE_LOG(LogBDObstacle, Log,
				TEXT("Seed %d: %d obstacle(s) accepted on attempt %d of %d. Shortest route %d cells."),
				Seed, Picked.Num(), Attempt, MaxAttempts, LastPathLength);
			return true;
		}

		// Candidates were Free by construction, so handing them back is exact.
		WriteCells(*Grid, Picked, EBDCellState::Free);

		UE_LOG(LogBDObstacle, Verbose, TEXT("Seed %d attempt %d rejected: %s."), Seed, Attempt, *Reason);
	}

	UE_LOG(LogBDObstacle, Warning,
		TEXT("Seed %d: %d attempts all rejected (last reason: %s). Falling back to the authored layout."),
		Seed, MaxAttempts, *Reason);

	TArray<FBDCellCoord> Fallback;
	for (const FBDCellCoord& Coord : Settings.FallbackObstacles)
	{
		if (Grid->GetCellState(Coord) == EBDCellState::Free)
		{
			Fallback.Add(Coord);
		}
	}

	WriteCells(*Grid, Fallback, EBDCellState::Blocked);
	GeneratedCells = Fallback;

	UE_LOG(LogBDObstacle, Warning, TEXT("Authored fallback placed %d obstacle(s)."), Fallback.Num());
	return false;
}

namespace BDObstacleCommands
{
	static constexpr int32 ArgCountGenerate = 1;

	/**
	 * A cheap fingerprint of a layout, so two seeds can be told apart from the log alone
	 * and the same seed can be shown to rebuild the same board.
	 */
	static uint32 HashLayout(const TArray<FBDCellCoord>& Cells)
	{
		uint32 Hash = 0;
		for (const FBDCellCoord& Coord : Cells)
		{
			Hash = HashCombine(Hash, GetTypeHash(Coord));
		}

		return Hash;
	}

	static void ExecGenerate(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogBDObstacle, Error, TEXT("BD.Obstacles.Generate needs a world."));
			return;
		}

		if (Args.Num() != ArgCountGenerate)
		{
			UE_LOG(LogBDObstacle, Error, TEXT("Usage: BD.Obstacles.Generate <seed>"));
			return;
		}

		UBDGridSubsystem* Grid = World->GetSubsystem<UBDGridSubsystem>();
		UBDObstacleGenerator* Generator = World->GetSubsystem<UBDObstacleGenerator>();
		if (Grid == nullptr || Generator == nullptr)
		{
			UE_LOG(LogBDObstacle, Error, TEXT("BD.Obstacles.Generate: this world has no grid or no generator."));
			return;
		}

		const int32 Seed = FCString::Atoi(*Args[0]);
		const bool bValidated = Generator->GenerateObstacles(Grid, Seed);

		UE_LOG(LogBDObstacle, Log,
			TEXT("BD.Obstacles.Generate %d: %s, %d cell(s), %d attempt(s), shortest route %d, layout %08x."),
			Seed,
			bValidated ? TEXT("validated") : TEXT("FELL BACK to the authored layout"),
			Generator->GetGeneratedCells().Num(),
			Generator->GetLastAttemptCount(),
			Generator->GetLastPathLength(),
			HashLayout(Generator->GetGeneratedCells()));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdGenerate(
		TEXT("BD.Obstacles.Generate"),
		TEXT("BD.Obstacles.Generate <seed>: lays down a seeded, validated obstacle layout."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecGenerate));
}

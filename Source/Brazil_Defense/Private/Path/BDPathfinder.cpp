// Brazil Defense. A* over the logical grid.

#include "Path/BDPathfinder.h"

#include "Algo/Reverse.h"
#include "BDLog.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Grid/BDGridDebug.h"
#include "Grid/BDGridSettings.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Stats/Stats.h"

namespace BDPathfinderPrivate
{
	/** Draws the last path found. On by default, but idle until a search actually runs. */
	static int32 GPathDebugEnabled = 1;

	static FAutoConsoleVariableRef CVarPathDebug(
		TEXT("BD.Path.Debug"),
		GPathDebugEnabled,
		TEXT("Draw the last path found by the Brazil Defense pathfinder, joining the cell centers. 0 to disable."),
		ECVF_Cheat);

	/** Four directional movement. See the class comment for why diagonals are out. */
	static constexpr int32 NeighbourOffsetX[] = { 1, -1, 0, 0 };
	static constexpr int32 NeighbourOffsetY[] = { 0, 0, 1, -1 };
	static constexpr int32 NeighbourCount = UE_ARRAY_COUNT(NeighbourOffsetX);

	/** Uniform step cost, which is what makes the Manhattan heuristic admissible. */
	static constexpr int32 StepCost = 1;

	static constexpr int32 ArgCountPathTest = 4;

	/** One entry of the open list. */
	struct FOpenNode
	{
		int32 CellIndex = INDEX_NONE;
		int32 FScore = 0;
	};

	/** Lowest FScore comes out of the heap first. */
	struct FOpenNodeCompare
	{
		bool operator()(const FOpenNode& A, const FOpenNode& B) const
		{
			return A.FScore < B.FScore;
		}
	};

	static int32 ManhattanDistance(const FBDCellCoord& A, const FBDCellCoord& B)
	{
		return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
	}
}

void UBDPathfinder::Initialize(FSubsystemCollectionBase& Collection)
{
	// Required by UTickableWorldSubsystem: ticking only starts once this runs.
	Super::Initialize(Collection);
}

void UBDPathfinder::Deinitialize()
{
	LastPath.Empty();

	Super::Deinitialize();
}

TStatId UBDPathfinder::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBDPathfinder, STATGROUP_Tickables);
}

void UBDPathfinder::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (BDGridDebug::IsEnabled() && BDPathfinderPrivate::GPathDebugEnabled != 0)
	{
		DrawLastPath();
	}
}

UBDPathfinder* UBDPathfinder::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	return World != nullptr ? World->GetSubsystem<UBDPathfinder>() : nullptr;
}

//~ Public queries -------------------------------------------------------------

bool UBDPathfinder::FindPath(const UBDGridSubsystem* Grid, const FBDCellCoord Start, const FBDCellCoord Goal,
	TArray<FBDCellCoord>& OutPath) const
{
	OutPath.Reset();
	if (Grid == nullptr)
	{
		return false;
	}

	const double SearchStart = FPlatformTime::Seconds();
	const bool bFound = RunSearch(*Grid, Start, Goal, nullptr, &OutPath);
	LastSearchMicroseconds = (FPlatformTime::Seconds() - SearchStart) * 1000000.0;

	LastPath = OutPath;
	return bFound;
}

bool UBDPathfinder::HasAnyPath(const UBDGridSubsystem* Grid, const FBDCellCoord Start, const FBDCellCoord Goal) const
{
	if (Grid == nullptr)
	{
		return false;
	}

	const double SearchStart = FPlatformTime::Seconds();
	const bool bFound = RunSearch(*Grid, Start, Goal, nullptr, nullptr);
	LastSearchMicroseconds = (FPlatformTime::Seconds() - SearchStart) * 1000000.0;

	return bFound;
}

bool UBDPathfinder::WouldBlockPath(const UBDGridSubsystem* Grid, const FBDCellCoord Origin, const FIntPoint Footprint) const
{
	if (Grid == nullptr)
	{
		return false;
	}

	const int32 SizeX = Grid->GetSizeX();
	const int32 CellCount = Grid->GetCellCount();
	if (CellCount <= 0)
	{
		return false;
	}

	// The player drags a piece around and this gets asked every frame, but the board
	// only moves when something is actually placed. The grid version is what makes
	// reusing the answer safe: any Place or Remove bumps it.
	if (bHasCachedBlockResult
		&& CachedBlockGrid.Get() == Grid
		&& CachedBlockGridVersion == Grid->GetVersion()
		&& CachedBlockOrigin == Origin
		&& CachedBlockFootprint == Footprint)
	{
		bLastBlockCheckWasCached = true;
		return bCachedBlockResult;
	}

	bLastBlockCheckWasCached = false;

	TArray<FBDCellCoord> Spawns;
	TArray<FBDCellCoord> Goals;
	GatherCellsWithState(*Grid, EBDCellState::Spawn, Spawns);
	GatherCellsWithState(*Grid, EBDCellState::Goal, Goals);

	if (Spawns.Num() == 0 || Goals.Num() == 0)
	{
		// Nothing to cut off yet. Worth saying out loud: on a finished map this means
		// the Spawn and Goal cells were never marked.
		UE_LOG(LogBDPath, Warning,
			TEXT("WouldBlockPath called on a grid with %d spawn(s) and %d goal(s). Cannot evaluate blocking."),
			Spawns.Num(), Goals.Num());
		return false;
	}

	if (Goals.Num() > 1)
	{
		// There is one urn in the game. More than one Goal on the board is a setup
		// mistake, so say so and keep going deterministically with the first.
		UE_LOG(LogBDPath, Warning,
			TEXT("WouldBlockPath found %d goal cells. The game has a single goal; using %s and ignoring the rest."),
			Goals.Num(), *Goals[0].ToString());
	}

	const FBDCellCoord GoalCoord = Goals[0];

	// The footprint is only blocked inside this check. The grid is never written to,
	// so a rejected placement cannot leave the board in a wrong state.
	TBitArray<> BlockedOverride;
	BlockedOverride.Init(false, CellCount);

	const int32 SpanX = FMath::Max(1, Footprint.X);
	const int32 SpanY = FMath::Max(1, Footprint.Y);

	for (int32 Y = 0; Y < SpanY; ++Y)
	{
		for (int32 X = 0; X < SpanX; ++X)
		{
			const FBDCellCoord Coord(Origin.X + X, Origin.Y + Y);
			if (!Grid->IsValidCoord(Coord))
			{
				continue;
			}

			// Covering a spawn or a goal is blocking by definition, whatever the rest
			// of the board looks like.
			const EBDCellState State = Grid->GetCellState(Coord);
			if (State == EBDCellState::Spawn || State == EBDCellState::Goal)
			{
				return CacheBlockResult(Grid, Origin, Footprint, true);
			}

			BlockedOverride[Coord.Y * SizeX + Coord.X] = true;
		}
	}

	// Every spawn has to keep a way to the goal.
	for (const FBDCellCoord& Spawn : Spawns)
	{
		if (!RunSearch(*Grid, Spawn, GoalCoord, &BlockedOverride, nullptr))
		{
			return CacheBlockResult(Grid, Origin, Footprint, true);
		}
	}

	return CacheBlockResult(Grid, Origin, Footprint, false);
}

bool UBDPathfinder::CacheBlockResult(const UBDGridSubsystem* Grid, const FBDCellCoord& Origin,
	const FIntPoint& Footprint, const bool bResult) const
{
	CachedBlockGrid = Grid;
	CachedBlockGridVersion = Grid != nullptr ? Grid->GetVersion() : 0;
	CachedBlockOrigin = Origin;
	CachedBlockFootprint = Footprint;
	bCachedBlockResult = bResult;
	bHasCachedBlockResult = true;

	return bResult;
}

//~ The search -----------------------------------------------------------------

bool UBDPathfinder::RunSearch(const UBDGridSubsystem& Grid, const FBDCellCoord& Start, const FBDCellCoord& Goal,
	const TBitArray<>* BlockedOverride, TArray<FBDCellCoord>* OutPath) const
{
	using namespace BDPathfinderPrivate;

	LastVisitedCount = 0;

	const int32 SizeX = Grid.GetSizeX();
	const int32 CellCount = Grid.GetCellCount();
	if (CellCount <= 0 || !Grid.IsValidCoord(Start) || !Grid.IsValidCoord(Goal))
	{
		return false;
	}

	const auto IsPassable = [&Grid, BlockedOverride](const FBDCellCoord& Coord, const int32 CellIndex) -> bool
	{
		if (BlockedOverride != nullptr && BlockedOverride->IsValidIndex(CellIndex) && (*BlockedOverride)[CellIndex])
		{
			return false;
		}

		return Grid.IsWalkable(Coord);
	};

	const int32 StartIndex = Start.Y * SizeX + Start.X;
	const int32 GoalIndex = Goal.Y * SizeX + Goal.X;

	if (!IsPassable(Start, StartIndex) || !IsPassable(Goal, GoalIndex))
	{
		return false;
	}

	if (StartIndex == GoalIndex)
	{
		LastVisitedCount = 1;
		if (OutPath != nullptr)
		{
			OutPath->Add(Start);
		}
		return true;
	}

	TArray<int32> GScore;
	GScore.Init(TNumericLimits<int32>::Max(), CellCount);

	TArray<int32> CameFrom;
	CameFrom.Init(INDEX_NONE, CellCount);

	TBitArray<> Closed;
	Closed.Init(false, CellCount);

	TArray<FOpenNode> Open;
	Open.Reserve(CellCount);

	GScore[StartIndex] = 0;
	Open.HeapPush(FOpenNode{ StartIndex, ManhattanDistance(Start, Goal) }, FOpenNodeCompare());

	while (Open.Num() > 0)
	{
		FOpenNode Current;
		Open.HeapPop(Current, FOpenNodeCompare(), EAllowShrinking::No);

		// A cell can sit in the heap more than once, with the stale copies carrying a
		// worse score. The first pop is the good one; the rest are skipped here.
		if (Closed[Current.CellIndex])
		{
			continue;
		}

		Closed[Current.CellIndex] = true;
		++LastVisitedCount;

		if (Current.CellIndex == GoalIndex)
		{
			if (OutPath != nullptr)
			{
				for (int32 Step = GoalIndex; Step != INDEX_NONE; Step = CameFrom[Step])
				{
					OutPath->Add(FBDCellCoord(Step % SizeX, Step / SizeX));
				}
				Algo::Reverse(*OutPath);
			}
			return true;
		}

		const FBDCellCoord CurrentCoord(Current.CellIndex % SizeX, Current.CellIndex / SizeX);

		for (int32 Neighbour = 0; Neighbour < NeighbourCount; ++Neighbour)
		{
			const FBDCellCoord NeighbourCoord(
				CurrentCoord.X + NeighbourOffsetX[Neighbour],
				CurrentCoord.Y + NeighbourOffsetY[Neighbour]);

			if (!Grid.IsValidCoord(NeighbourCoord))
			{
				continue;
			}

			const int32 NeighbourIndex = NeighbourCoord.Y * SizeX + NeighbourCoord.X;
			if (Closed[NeighbourIndex] || !IsPassable(NeighbourCoord, NeighbourIndex))
			{
				continue;
			}

			const int32 TentativeG = GScore[Current.CellIndex] + StepCost;
			if (TentativeG >= GScore[NeighbourIndex])
			{
				continue;
			}

			GScore[NeighbourIndex] = TentativeG;
			CameFrom[NeighbourIndex] = Current.CellIndex;
			Open.HeapPush(FOpenNode{ NeighbourIndex, TentativeG + ManhattanDistance(NeighbourCoord, Goal) }, FOpenNodeCompare());
		}
	}

	return false;
}

void UBDPathfinder::GatherCellsWithState(const UBDGridSubsystem& Grid, const EBDCellState State, TArray<FBDCellCoord>& OutCells)
{
	OutCells.Reset();

	for (int32 Y = 0; Y < Grid.GetSizeY(); ++Y)
	{
		for (int32 X = 0; X < Grid.GetSizeX(); ++X)
		{
			const FBDCellCoord Coord(X, Y);
			if (Grid.GetCellState(Coord) == State)
			{
				OutCells.Add(Coord);
			}
		}
	}
}

//~ Debug drawing --------------------------------------------------------------

void UBDPathfinder::DrawLastPath() const
{
	if (LastPath.Num() == 0)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
	if (Grid == nullptr)
	{
		return;
	}

	const UBDGridSettings& Settings = UBDGridSettings::Get();
	const FVector HeightOffset(0.0f, 0.0f, Settings.PathDrawHeightOffset);

	for (int32 Step = 0; Step < LastPath.Num() - 1; ++Step)
	{
		DrawDebugLine(World,
			Grid->CellToWorld(LastPath[Step]) + HeightOffset,
			Grid->CellToWorld(LastPath[Step + 1]) + HeightOffset,
			Settings.PathLineColor, BDGridDebug::bPersistentLines,
			BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, Settings.PathLineThickness);
	}

	// Both ends marked, so a path of a single cell is still visible.
	DrawDebugSphere(World, Grid->CellToWorld(LastPath[0]) + HeightOffset,
		Settings.PathEndpointRadius, Settings.PathEndpointSegments, Settings.PathEndpointColor,
		BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, Settings.LineThickness);

	DrawDebugSphere(World, Grid->CellToWorld(LastPath.Last()) + HeightOffset,
		Settings.PathEndpointRadius, Settings.PathEndpointSegments, Settings.PathEndpointColor,
		BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, Settings.LineThickness);
}

//~ Console command ------------------------------------------------------------

namespace BDPathfinderPrivate
{
	static void ExecPathTest(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogBDPath, Error, TEXT("BD.Path.Test needs a world."));
			return;
		}

		if (Args.Num() != ArgCountPathTest)
		{
			UE_LOG(LogBDPath, Error, TEXT("Usage: BD.Path.Test <x1> <y1> <x2> <y2>"));
			return;
		}

		const UBDGridSubsystem* Grid = World->GetSubsystem<UBDGridSubsystem>();
		const UBDPathfinder* Pathfinder = World->GetSubsystem<UBDPathfinder>();
		if (Grid == nullptr || Pathfinder == nullptr)
		{
			UE_LOG(LogBDPath, Error, TEXT("BD.Path.Test: grid or pathfinder subsystem missing in this world."));
			return;
		}

		const FBDCellCoord Start(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		const FBDCellCoord Goal(FCString::Atoi(*Args[2]), FCString::Atoi(*Args[3]));

		TArray<FBDCellCoord> Path;
		const bool bFound = Pathfinder->FindPath(Grid, Start, Goal, Path);

		// Both entry points run the same search, so disagreeing would mean the
		// path-free variant drifted from the real one.
		const bool bHasAny = Pathfinder->HasAnyPath(Grid, Start, Goal);
		if (bHasAny != bFound)
		{
			UE_LOG(LogBDPath, Error, TEXT("BD.Path.Test: HasAnyPath says %s but FindPath says %s."),
				bHasAny ? TEXT("reachable") : TEXT("unreachable"),
				bFound ? TEXT("reachable") : TEXT("unreachable"));
		}

		if (bFound)
		{
			UE_LOG(LogBDPath, Log,
				TEXT("BD.Path.Test %s -> %s: found, %d cells long, %d visited, %.1f us. HasAnyPath agrees."),
				*Start.ToString(), *Goal.ToString(), Path.Num(),
				Pathfinder->GetLastVisitedCount(), Pathfinder->GetLastSearchMicroseconds());
		}
		else
		{
			UE_LOG(LogBDPath, Warning,
				TEXT("BD.Path.Test %s -> %s: no path, %d visited, %.1f us. HasAnyPath agrees."),
				*Start.ToString(), *Goal.ToString(),
				Pathfinder->GetLastVisitedCount(), Pathfinder->GetLastSearchMicroseconds());
		}
	}

	static constexpr int32 ArgCountWouldBlock = 4;

	/** Asks the placement validation whether a footprint here would cut the creeps off. */
	static void ExecWouldBlock(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || Args.Num() != ArgCountWouldBlock)
		{
			UE_LOG(LogBDPath, Error, TEXT("Usage: BD.Path.WouldBlock <x> <y> <footprintX> <footprintY>"));
			return;
		}

		const UBDGridSubsystem* Grid = World->GetSubsystem<UBDGridSubsystem>();
		const UBDPathfinder* Pathfinder = World->GetSubsystem<UBDPathfinder>();
		if (Grid == nullptr || Pathfinder == nullptr)
		{
			UE_LOG(LogBDPath, Error, TEXT("BD.Path.WouldBlock: grid or pathfinder subsystem missing in this world."));
			return;
		}

		const FBDCellCoord Origin(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		const FIntPoint Footprint(FCString::Atoi(*Args[2]), FCString::Atoi(*Args[3]));

		const bool bWouldBlock = Pathfinder->WouldBlockPath(Grid, Origin, Footprint);
		UE_LOG(LogBDPath, Log, TEXT("BD.Path.WouldBlock at %s footprint %dx%d: %s [%s, grid version %d]."),
			*Origin.ToString(), Footprint.X, Footprint.Y,
			bWouldBlock ? TEXT("BLOCKS, placement must be rejected") : TEXT("does not block, placement allowed"),
			Pathfinder->WasLastBlockCheckCached() ? TEXT("cached") : TEXT("recomputed"),
			Grid->GetVersion());
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdWouldBlock(
		TEXT("BD.Path.WouldBlock"),
		TEXT("BD.Path.WouldBlock <x> <y> <footprintX> <footprintY>: tests the placement blocking validation."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecWouldBlock));

	// Registered once for the whole module. The world comes from the console context,
	// which is what keeps the editor world and the PIE world apart.
	static FAutoConsoleCommandWithWorldAndArgs CmdPathTest(
		TEXT("BD.Path.Test"),
		TEXT("BD.Path.Test <x1> <y1> <x2> <y2>: runs a search between two cells and logs the timing."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecPathTest));
}

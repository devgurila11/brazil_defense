// Brazil Defense. Where the creeps come from, which way they walk and who is still out.

#include "Wave/BDWaveSubsystem.h"

#include "BDLog.h"
#include "Candidate/BDCandidateSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Enemy/BDEnemyBase.h"
#include "Enemy/BDEnemyData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Grid/BDGridDebug.h"
#include "Grid/BDGridSettings.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Match/BDGameBalanceSettings.h"
#include "Match/BDMatchManager.h"
#include "Objective/BDObjective.h"
#include "Path/BDPathfinder.h"
#include "Stats/Stats.h"
#include "Wave/BDWaveSettings.h"

namespace BDWavePrivate
{
	/** Draws the route of every spawn point. Off by default: the routes are a tuning aid, not gameplay. */
	static int32 GShowRoutes = 0;

	static FAutoConsoleVariableRef CVarShowRoutes(
		TEXT("BD.Path.ShowRoutes"),
		GShowRoutes,
		TEXT("1 draws, per mouth of the current wave, the shortest route to the urn thin and the route every living creep is actually walking thick, one color per mouth. Every mouth before the first wave. 0 to hide."),
		ECVF_Cheat);

	/**
	 * Groups cells into runs of four-adjacent cells. Cells are visited in the order given,
	 * so with row major input the runs come out in row major order of their first cell,
	 * which keeps the spawn point indices stable across sessions.
	 */
	static void GroupAdjacent(const TArray<FBDCellCoord>& Cells, TArray<TArray<FBDCellCoord>>& OutGroups)
	{
		OutGroups.Reset();

		TSet<FBDCellCoord> Remaining(Cells);
		for (const FBDCellCoord& Seed : Cells)
		{
			if (!Remaining.Contains(Seed))
			{
				continue;
			}

			TArray<FBDCellCoord>& Group = OutGroups.AddDefaulted_GetRef();
			TArray<FBDCellCoord> Frontier;
			Frontier.Add(Seed);
			Remaining.Remove(Seed);

			while (Frontier.Num() > 0)
			{
				const FBDCellCoord Cell = Frontier.Pop(EAllowShrinking::No);
				Group.Add(Cell);

				const FBDCellCoord Neighbours[] = {
					FBDCellCoord(Cell.X + 1, Cell.Y), FBDCellCoord(Cell.X - 1, Cell.Y),
					FBDCellCoord(Cell.X, Cell.Y + 1), FBDCellCoord(Cell.X, Cell.Y - 1) };

				for (const FBDCellCoord& Neighbour : Neighbours)
				{
					if (Remaining.Remove(Neighbour) > 0)
					{
						Frontier.Add(Neighbour);
					}
				}
			}

			Group.Sort([](const FBDCellCoord& A, const FBDCellCoord& B)
			{
				return A.Y != B.Y ? A.Y < B.Y : A.X < B.X;
			});
		}
	}
}

void UBDWaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	// Required by UTickableWorldSubsystem: ticking only starts once this runs.
	Super::Initialize(Collection);

	// The grid is read, never written, so it must be there first.
	Collection.InitializeDependency<UBDGridSubsystem>();
	Collection.InitializeDependency<UBDPathfinder>();

	if (UBDGridSubsystem* Grid = GetGrid())
	{
		GridRebuiltHandle = Grid->OnGridRebuilt.AddUObject(this, &UBDWaveSubsystem::HandleGridRebuilt);
		CellStateChangedHandle = Grid->OnCellStateChanged.AddUObject(this, &UBDWaveSubsystem::HandleCellStateChanged);
		EdgeBlockedChangedHandle = Grid->OnEdgeBlockedChanged.AddUObject(this, &UBDWaveSubsystem::HandleEdgeBlockedChanged);
	}
}

void UBDWaveSubsystem::Deinitialize()
{
	if (UBDGridSubsystem* Grid = GetGrid())
	{
		Grid->OnGridRebuilt.Remove(GridRebuiltHandle);
		Grid->OnCellStateChanged.Remove(CellStateChangedHandle);
		Grid->OnEdgeBlockedChanged.Remove(EdgeBlockedChangedHandle);
	}

	StopSpawnLoop();
	if (ABDMatchManager* Match = BoundMatch.Get())
	{
		Match->OnWaveStarted.Remove(WaveStartedHandle);
		Match->OnPhaseChanged.Remove(PhaseChangedHandle);
	}
	BoundMatch.Reset();
	LivingEnemies.Empty();
	SpawnPoints.Empty();
	GoalCells.Empty();

	Super::Deinitialize();
}

TStatId UBDWaveSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBDWaveSubsystem, STATGROUP_Tickables);
}

UBDWaveSubsystem* UBDWaveSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	return World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr;
}

UBDGridSubsystem* UBDWaveSubsystem::GetGrid() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
}

UBDPathfinder* UBDWaveSubsystem::GetPathfinder() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDPathfinder>() : nullptr;
}

ABDMatchManager* UBDWaveSubsystem::GetMatch() const
{
	return ABDMatchManager::Get(GetWorld());
}

//~ Grid changes ---------------------------------------------------------------

void UBDWaveSubsystem::MarkBoardChanged()
{
	bRoutesDirty = true;

	// Rerouted right here rather than on the next tick: the creeps tick before this
	// subsystem does, and a creep must not get one frame of walking through a fence that
	// is already on the board. Nothing blocking can be placed while a wave is out, so in
	// play this only ever runs on an empty board and costs nothing; the console is the
	// one thing that fences creeps in mid walk.
	if (LivingEnemies.Num() > 0)
	{
		GetSpawnPoints();
		RerouteLivingEnemies();
	}
}

void UBDWaveSubsystem::HandleGridRebuilt()
{
	MarkBoardChanged();
}

void UBDWaveSubsystem::HandleCellStateChanged(const FBDCellCoord& Coord, const EBDCellState NewState)
{
	MarkBoardChanged();
}

void UBDWaveSubsystem::HandleEdgeBlockedChanged(const FBDEdgeCoord& Edge, const bool bBlocked)
{
	MarkBoardChanged();
}

void UBDWaveSubsystem::EnsureMatchBinding()
{
	if (BoundMatch.IsValid())
	{
		return;
	}

	ABDMatchManager* Match = GetMatch();
	if (Match == nullptr)
	{
		return;
	}

	WaveStartedHandle = Match->OnWaveStarted.AddUObject(this, &UBDWaveSubsystem::HandleWaveStarted);
	PhaseChangedHandle = Match->OnPhaseChanged.AddUObject(this, &UBDWaveSubsystem::HandlePhaseChanged);
	BoundMatch = Match;
}

void UBDWaveSubsystem::HandleWaveStarted(const int32 Wave)
{
	const UBDEnemyData* Data = UBDWaveSettings::Get().ResolveWaveEnemy();
	if (Data == nullptr)
	{
		UE_LOG(LogBDWave, Error, TEXT("Wave %d has no enemy to send: set Wave Enemy in Project Settings > Brazil Defense - Waves."), Wave);
		return;
	}

	// One more creep per spawn point every wave: the pressure grows with the board, not
	// with a flat number, so a map with more mouths is a harder map.
	const int32 PointCount = GetSpawnPointCount();
	const int32 PerPoint = UBDGameBalanceSettings::Get().GetCreepsPerSpawnPoint(Wave);

	// The buses shuffle along the edge first, then the draw picks among them.
	WanderSpawnPoints(Wave);

	// Which mouths open is drawn per wave; how many creeps come out is not. The total
	// stays PerPoint x every mouth of the board, so a wave out of two mouths is the same
	// horde as a wave out of six, arriving in a thicker stream.
	DrawActiveSpawnPoints(Wave);

	WaveEnemyData = Data;
	WaveSpawnsRemaining = PerPoint * PointCount;

	// A wave that starts under the parade of the fallen sends nobody of its own.
	if (IsSpawningHeld())
	{
		UE_LOG(LogBDWave, Log, TEXT("Wave %d is the candidates' parade: its %d creep(s) are not sent."), Wave, WaveSpawnsRemaining);
		WaveSpawnsRemaining = 0;
	}
	WaveSpawnCursor = 0;
	WaveSpawnInterval = UBDGameBalanceSettings::Get().GetWaveSpawnInterval(Wave);
	// The first one goes out on the next tick, not an interval from now.
	WaveSpawnTimer = WaveSpawnInterval;

	WaveNumber = Wave;
	WaveSpawnedTotal = 0;
	WavePeakAlive = LivingEnemies.Num();
	WaveArrived = 0;
	WaveKilled = 0;
	WaveWastedDamage = 0.0f;
	WaveLostShots = 0;
	WaveShotsFired = 0;
	WaveDamageDealt = 0.0f;
	WaveTotalHealth = 0.0f;
	WaveRouteHashes.Reset();

	FString Mouths;
	for (const int32 Index : ActiveSpawnPoints)
	{
		Mouths += Mouths.IsEmpty() ? FString::FromInt(Index) : FString::Printf(TEXT(", %d"), Index);
	}

	UE_LOG(LogBDWave, Log, TEXT("Wave %d: %d x %s (%d per spawn point x %d points) at x%.2f health, one every %.2fs, out of %d mouth(s) [%s]."),
		Wave, WaveSpawnsRemaining, *Data->GetName(), PerPoint, PointCount,
		UBDGameBalanceSettings::Get().GetHealthScale(Wave), WaveSpawnInterval,
		ActiveSpawnPoints.Num(), *Mouths);
}

namespace BDWavePrivate
{
	/** The way into the board from a border run, or zero for a run not on any edge. */
	static FBDCellCoord InwardStep(const FBDCellCoord& Cell, const bool bAlongX, const int32 SizeX, const int32 SizeY)
	{
		if (bAlongX)
		{
			return FBDCellCoord(0, Cell.Y == 0 ? 1 : (Cell.Y == SizeY - 1 ? -1 : 0));
		}
		return FBDCellCoord(Cell.X == 0 ? 1 : (Cell.X == SizeX - 1 ? -1 : 0), 0);
	}
}

bool UBDWaveSubsystem::IsExitOpen(const FBDSpawnPoint& Point, const bool bAlongX) const
{
	const UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr || Point.Cells.Num() == 0)
	{
		return false;
	}

	const FBDCellCoord Step = BDWavePrivate::InwardStep(Point.ExitCell, bAlongX, Grid->GetSizeX(), Grid->GetSizeY());
	if (Step.X == 0 && Step.Y == 0)
	{
		return true;
	}

	const FBDCellCoord Inward(Point.ExitCell.X + Step.X, Point.ExitCell.Y + Step.Y);
	return Grid->IsValidCoord(Inward) && Grid->IsWalkable(Inward) && !Grid->IsEdgeBlocked(Point.ExitCell, Inward);
}

bool UBDWaveSubsystem::TryShiftSpawnPoint(const TArray<FBDSpawnPoint>& Points, const int32 PointIndex, const bool bAlongX, const int32 Shift)
{
	UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr || !Points.IsValidIndex(PointIndex) || Shift == 0)
	{
		return false;
	}
	const FBDSpawnPoint& Point = Points[PointIndex];

	TArray<FBDCellCoord> NewCells;
	for (const FBDCellCoord& Cell : Point.Cells)
	{
		const FBDCellCoord Moved(Cell.X + (bAlongX ? Shift : 0), Cell.Y + (bAlongX ? 0 : Shift));
		const bool bOwn = Point.Cells.Contains(Moved);
		if (!Grid->IsValidCoord(Moved) || (!bOwn && Grid->GetCellState(Moved) != EBDCellState::Free))
		{
			return false;
		}
		// Not up against another mouth: two runs touching would read as one.
		for (int32 Other = 0; Other < Points.Num(); ++Other)
		{
			if (Other == PointIndex)
			{
				continue;
			}
			for (const FBDCellCoord& OtherCell : Points[Other].Cells)
			{
				if (FMath::Abs(OtherCell.X - Moved.X) + FMath::Abs(OtherCell.Y - Moved.Y) <= 1)
				{
					return false;
				}
			}
		}
		NewCells.Add(Moved);
	}

	// Applied, then checked: the exit must open onto the board and reach the urn, or it
	// all goes back.
	for (const FBDCellCoord& Cell : Point.Cells) { Grid->SetCellState(Cell, EBDCellState::Free); }
	for (const FBDCellCoord& Cell : NewCells) { Grid->SetCellState(Cell, EBDCellState::Spawn); }

	FBDSpawnPoint Shifted;
	Shifted.Cells = NewCells;
	Shifted.ExitCell = NewCells[NewCells.Num() / 2];

	// Never past the leash from the bus, by either rule: the random walk and the escape
	// from a blocked exit share the one ceiling.
	const int32 Leash = FMath::Max(0, UBDGameBalanceSettings::Get().MouthWanderMaxCells);
	const int32 FromAnchor = FMath::Abs(Shifted.ExitCell.X - Point.AnchorExit.X) + FMath::Abs(Shifted.ExitCell.Y - Point.AnchorExit.Y);
	if (FromAnchor > Leash)
	{
		for (const FBDCellCoord& Cell : NewCells) { Grid->SetCellState(Cell, EBDCellState::Free); }
		for (const FBDCellCoord& Cell : Point.Cells) { Grid->SetCellState(Cell, EBDCellState::Spawn); }
		return false;
	}

	TArray<FBDCellCoord> Route;
	if (!IsExitOpen(Shifted, bAlongX) || (GoalCells.Num() > 0 && !FindRouteToGoal(Shifted.ExitCell, Route)))
	{
		for (const FBDCellCoord& Cell : NewCells) { Grid->SetCellState(Cell, EBDCellState::Free); }
		for (const FBDCellCoord& Cell : Point.Cells) { Grid->SetCellState(Cell, EBDCellState::Spawn); }
		return false;
	}

	UE_LOG(LogBDWave, Log, TEXT("Mouth %d slid %+d cell(s) along its edge: exit %s -> %s (%d from its bus at %s)."),
		PointIndex, Shift, *Point.ExitCell.ToString(), *Shifted.ExitCell.ToString(), FromAnchor, *Point.AnchorExit.ToString());
	return true;
}

void UBDWaveSubsystem::WanderSpawnPoints(const int32 Wave)
{
	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
	UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr)
	{
		return;
	}

	// Its own draw, from the seed and the wave, so the same match wanders the same way.
	const ABDMatchManager* Match = GetMatch();
	const int32 Seed = Match != nullptr ? Match->ObstacleSeed : 0;
	FRandomStream Stream(static_cast<int32>(HashCombine(HashCombine(::GetTypeHash(Seed), ::GetTypeHash(Wave)), 0x5EEDu)));

	const int32 SizeX = Grid->GetSizeX();
	const int32 SizeY = Grid->GetSizeY();
	int32 Moved = 0;

	// Each mouth is judged against the board as the ones before it left it.
	for (int32 PointIndex = 0; PointIndex < GetSpawnPoints().Num(); ++PointIndex)
	{
		const TArray<FBDSpawnPoint> Points = GetSpawnPoints();
		const FBDSpawnPoint& Point = Points[PointIndex];
		if (Point.Cells.Num() == 0)
		{
			continue;
		}

		// The mouth slides along the edge it sits on: X along the top and bottom rows, Y
		// along the left and right columns. A mouth not on any edge stays put.
		const FBDCellCoord& First = Point.Cells[0];
		const bool bOnRow = First.Y == 0 || First.Y == SizeY - 1;
		const bool bOnColumn = First.X == 0 || First.X == SizeX - 1;
		if (!bOnRow && !bOnColumn)
		{
			continue;
		}
		const bool bAlongX = bOnRow && (!bOnColumn || Point.Cells.Num() == 1 || Point.Cells[1].Y == First.Y);

		bool bShifted = false;

		// A mouth the player built in front of is no exit: it takes the nearest open spot
		// along its edge, either way, before any dice are thrown.
		if (!IsExitOpen(Point, bAlongX))
		{
			const int32 Reach = FMath::Max(Balance.MouthWanderMaxCells, 1) * 2;
			// Twice the leash in steps from here, since the mouth may already stand off its
			// bus; the shift itself refuses anything past the leash from the anchor.
			for (int32 Distance = 1; Distance <= Reach && !bShifted; ++Distance)
			{
				const int32 FirstWay = Stream.RandBool() ? 1 : -1;
				bShifted = TryShiftSpawnPoint(Points, PointIndex, bAlongX, Distance * FirstWay)
					|| TryShiftSpawnPoint(Points, PointIndex, bAlongX, -Distance * FirstWay);
			}
			if (bShifted)
			{
				UE_LOG(LogBDWave, Log, TEXT("Mouth %d was blocked at %s and moved to the nearest open spot."), PointIndex, *Point.ExitCell.ToString());
			}
			else
			{
				UE_LOG(LogBDWave, Warning, TEXT("Mouth %d is blocked at %s and found no open spot within %d cell(s)."), PointIndex, *Point.ExitCell.ToString(), Reach);
			}
		}
		else if (Balance.MouthWanderMaxCells > 0 && Stream.FRand() <= Balance.MouthWanderChance)
		{
			// Up to a few tries at a random slide, first one that fits wins.
			for (int32 Try = 0; Try < 6 && !bShifted; ++Try)
			{
				bShifted = TryShiftSpawnPoint(Points, PointIndex, bAlongX, Stream.RandRange(-Balance.MouthWanderMaxCells, Balance.MouthWanderMaxCells));
			}
		}

		if (bShifted)
		{
			++Moved;
			// The grid changed under the routes: read the mouths again before the next one.
			bRoutesDirty = true;
			GetSpawnPoints();
		}
	}

	if (Moved > 0)
	{
		UE_LOG(LogBDWave, Verbose, TEXT("%d mouth(s) moved before wave %d."), Moved, Wave);
	}
}

void UBDWaveSubsystem::DrawActiveSpawnPoints(const int32 Wave)
{
	ActiveSpawnPoints.Reset();

	// Only mouths that can actually send a creep out are drawn from: one the player
	// walled off is not a mouth this wave can use.
	TArray<int32> Usable;
	const TArray<FBDSpawnPoint>& Points = GetSpawnPoints();
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		if (Points[Index].Route.Num() > 0)
		{
			Usable.Add(Index);
		}
	}

	if (Usable.Num() == 0)
	{
		return;
	}

	// Seed of the match mixed with the wave number: the same board played again opens the
	// same mouths in the same order, and no two waves of a match draw alike.
	const ABDMatchManager* Match = GetMatch();
	const int32 Seed = Match != nullptr ? Match->ObstacleSeed : 0;
	FRandomStream Stream(static_cast<int32>(HashCombine(::GetTypeHash(Seed), ::GetTypeHash(Wave))));

	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
	const int32 Fewest = FMath::Clamp(Balance.MinActiveSpawnPoints, 1, Usable.Num());
	const int32 Most = FMath::Clamp(
		Balance.MaxActiveSpawnPoints > 0 ? Balance.MaxActiveSpawnPoints : Usable.Num(),
		Fewest, Usable.Num());

	const int32 Count = Stream.RandRange(Fewest, Most);
	for (int32 Index = Usable.Num() - 1; Index > 0; --Index)
	{
		Usable.Swap(Index, Stream.RandRange(0, Index));
	}

	ActiveSpawnPoints.Append(Usable.GetData(), Count);
	// Back into board order: the draw picked which, not in what order they are dealt.
	ActiveSpawnPoints.Sort();
}

void UBDWaveSubsystem::HandlePhaseChanged(const EBDMatchPhase NewPhase)
{
	// A wave ended from outside (BD.Match.ClearWave, defeat): whatever it had not sent stays unsent.
	if (NewPhase != EBDMatchPhase::WaveActive && WaveSpawnsRemaining > 0)
	{
		UE_LOG(LogBDWave, Warning, TEXT("Wave left with %d creep(s) unsent."), WaveSpawnsRemaining);
		WaveSpawnsRemaining = 0;
	}
}

void UBDWaveSubsystem::SpawnNextOfWave()
{
	const int32 PointCount = GetSpawnPointCount();
	if (PointCount == 0 || WaveEnemyData == nullptr)
	{
		UE_LOG(LogBDWave, Error, TEXT("Wave cannot send its creeps: no spawn point with a route. %d dropped."), WaveSpawnsRemaining);
		WaveSpawnsRemaining = 0;
		return;
	}

	const auto Accept = [this](const int32 Index)
	{
		if (SpawnEnemy(WaveEnemyData, Index) == nullptr)
		{
			return false;
		}

		--WaveSpawnsRemaining;
		++WaveSpawnedTotal;
		WavePeakAlive = FMath::Max(WavePeakAlive, LivingEnemies.Num());
		if (LivingEnemies.Num() > 0 && LivingEnemies.Last() != nullptr)
		{
			WaveTotalHealth += LivingEnemies.Last()->GetMaxHealth();
		}
		return true;
	};

	// Round robin over the mouths this wave drew open; one that refuses is skipped, not
	// retried forever.
	for (int32 Attempt = 0; Attempt < ActiveSpawnPoints.Num(); ++Attempt)
	{
		const int32 Index = ActiveSpawnPoints[WaveSpawnCursor % ActiveSpawnPoints.Num()];
		++WaveSpawnCursor;
		if (Accept(Index))
		{
			return;
		}
	}

	// Every mouth the wave drew has been closed since it opened: the player fenced them
	// off mid wave. What is left comes out of whatever mouth still works rather than
	// being dropped.
	for (int32 Index = 0; Index < PointCount; ++Index)
	{
		if (Accept(Index))
		{
			UE_LOG(LogBDWave, Warning, TEXT("Every mouth drawn for this wave is blocked; creep sent out of spawn point %d instead."), Index);
			return;
		}
	}

	UE_LOG(LogBDWave, Error, TEXT("Wave cannot send its creeps: every spawn point refused. %d dropped."), WaveSpawnsRemaining);
	WaveSpawnsRemaining = 0;
}

void UBDWaveSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	EnsureMatchBinding();

	// Held during the pause a candidate kill buys: the timer does not run, so the wave
	// picks up its rhythm where it left it rather than dumping the backlog at once.
	if (WaveSpawnsRemaining > 0 && !IsSpawningHeld())
	{
		WaveSpawnTimer += DeltaTime;
		const float Interval = FMath::Max(0.0f, WaveSpawnInterval);
		while (WaveSpawnsRemaining > 0 && WaveSpawnTimer >= Interval)
		{
			WaveSpawnTimer -= Interval;
			SpawnNextOfWave();
			if (Interval <= 0.0f)
			{
				WaveSpawnTimer = 0.0f;
			}
		}
	}

	const UWorld* World = GetWorld();
	if (BDWavePrivate::GShowRoutes != 0 && World != nullptr && BDGridDebug::ShouldDrawInWorld(*World))
	{
		GetSpawnPoints();
		DrawRoutes();
	}

	if (bSpawnLoopRunning)
	{
		// DeltaTime is dilated, so the loop follows the game speed like everything else.
		SpawnLoopTimer += DeltaTime;
		while (SpawnLoopTimer >= SpawnLoopInterval)
		{
			SpawnLoopTimer -= SpawnLoopInterval;

			const int32 Before = SpawnLoopSpawned;
			SpawnLoopSpawned += SpawnEnemyAtEveryPoint(SpawnLoopData);
			SpawnLoopPeak = FMath::Max(SpawnLoopPeak, LivingEnemies.Num());

			if (SpawnLoopSpawned == Before)
			{
				UE_LOG(LogBDWave, Error, TEXT("Spawn loop stopped: no spawn point could send a creep out."));
				StopSpawnLoop();
				return;
			}

			// One line per LoopLogEvery creeps rather than per creep: the log has to stay readable
			// under the very load this exists to create.
			if (SpawnLoopSpawned / LoopLogEvery != Before / LoopLogEvery)
			{
				UE_LOG(LogBDWave, Log, TEXT("Spawn loop: %d spawned, %d on the board (peak %d), %d arrived, %d killed, %.1fs."),
					SpawnLoopSpawned, LivingEnemies.Num(), SpawnLoopPeak, SpawnLoopArrived, SpawnLoopKilled,
					World != nullptr ? World->GetTimeSeconds() - SpawnLoopStartSeconds : 0.0);
			}
		}
	}
}

void UBDWaveSubsystem::StartSpawnLoop(const UBDEnemyData* Data, const float Interval)
{
	const UWorld* World = GetWorld();
	if (Data == nullptr || Interval <= 0.0f || World == nullptr || !World->IsGameWorld())
	{
		UE_LOG(LogBDWave, Error, TEXT("Spawn loop needs enemy data, an interval above zero and a game world."));
		return;
	}

	SpawnLoopData = Data;
	SpawnLoopInterval = Interval;
	// Fires on the next tick rather than a full interval from now.
	SpawnLoopTimer = Interval;
	SpawnLoopSpawned = 0;
	SpawnLoopArrived = 0;
	SpawnLoopKilled = 0;
	SpawnLoopPeak = LivingEnemies.Num();
	SpawnLoopStartSeconds = World->GetTimeSeconds();
	bSpawnLoopRunning = true;

	UE_LOG(LogBDWave, Log, TEXT("Spawn loop started: one %s out of every spawn point every %.2fs, summary every %d creeps."),
		*Data->GetName(), Interval, LoopLogEvery);
}

void UBDWaveSubsystem::StopSpawnLoop()
{
	if (!bSpawnLoopRunning)
	{
		return;
	}

	bSpawnLoopRunning = false;
	SpawnLoopData = nullptr;

	const UWorld* World = GetWorld();
	UE_LOG(LogBDWave, Log, TEXT("Spawn loop stopped after %.1fs: %d spawned, %d arrived, %d killed, peak %d on the board, %d still out."),
		World != nullptr ? World->GetTimeSeconds() - SpawnLoopStartSeconds : 0.0,
		SpawnLoopSpawned, SpawnLoopArrived, SpawnLoopKilled, SpawnLoopPeak, LivingEnemies.Num());
}

//~ Spawn points and routes ----------------------------------------------------

const TArray<FBDSpawnPoint>& UBDWaveSubsystem::GetSpawnPoints()
{
	const UBDGridSubsystem* Grid = GetGrid();
	if (bRoutesDirty || (Grid != nullptr && Grid->GetVersion() != RoutesGridVersion))
	{
		RefreshRoutes();
	}

	return SpawnPoints;
}

int32 UBDWaveSubsystem::GetSpawnPointCount()
{
	return GetSpawnPoints().Num();
}

void UBDWaveSubsystem::RefreshRoutes()
{
	BuildSpawnPoints();
	bRoutesDirty = false;
}

void UBDWaveSubsystem::BuildSpawnPoints()
{
	SpawnPoints.Reset();
	GoalCells.Reset();

	const UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr)
	{
		return;
	}

	RoutesGridVersion = Grid->GetVersion();

	TArray<FBDCellCoord> SpawnCells;
	UBDPathfinder::GatherCellsWithState(*Grid, EBDCellState::Spawn, SpawnCells);
	UBDPathfinder::GatherCellsWithState(*Grid, EBDCellState::Goal, GoalCells);

	TArray<TArray<FBDCellCoord>> Runs;
	BDWavePrivate::GroupAdjacent(SpawnCells, Runs);

	int32 Unreachable = 0;
	for (TArray<FBDCellCoord>& Run : Runs)
	{
		FBDSpawnPoint& Point = SpawnPoints.AddDefaulted_GetRef();
		Point.Cells = MoveTemp(Run);
		Point.ExitCell = Point.Cells[Point.Cells.Num() / 2];
		Point.AnchorExit = Point.ExitCell;

		if (!FindRouteToGoal(Point.ExitCell, Point.Route))
		{
			++Unreachable;
		}
	}

	// The anchors are the authored mouths, where the buses stand: taken from the first
	// read of the board and matched back to every run since by the nearest one on the
	// same edge. A mouth never wanders further than the balance allows from its anchor.
	if (MouthAnchors.Num() == 0)
	{
		for (const FBDSpawnPoint& Point : SpawnPoints)
		{
			MouthAnchors.Add(Point.ExitCell);
		}
	}
	else
	{
		for (FBDSpawnPoint& Point : SpawnPoints)
		{
			int32 Best = MAX_int32;
			for (const FBDCellCoord& Anchor : MouthAnchors)
			{
				const bool bSameEdge = Anchor.X == Point.ExitCell.X || Anchor.Y == Point.ExitCell.Y;
				const int32 Distance = FMath::Abs(Anchor.X - Point.ExitCell.X) + FMath::Abs(Anchor.Y - Point.ExitCell.Y);
				if (bSameEdge && Distance < Best)
				{
					Best = Distance;
					Point.AnchorExit = Anchor;
				}
			}
		}
	}

	if (SpawnPoints.Num() == 0 || GoalCells.Num() == 0)
	{
		UE_LOG(LogBDWave, Warning, TEXT("Routes read off a grid with %d spawn point(s) and %d goal cell(s): nothing can walk."),
			SpawnPoints.Num(), GoalCells.Num());
	}
	else if (Unreachable > 0)
	{
		UE_LOG(LogBDWave, Warning, TEXT("%d of %d spawn point(s) have no route to the urn (grid version %d)."),
			Unreachable, SpawnPoints.Num(), RoutesGridVersion);
	}
	else
	{
		UE_LOG(LogBDWave, Verbose, TEXT("%d spawn point(s) routed to the urn (grid version %d)."),
			SpawnPoints.Num(), RoutesGridVersion);
	}
}

bool UBDWaveSubsystem::GetObjectiveLocation(FVector& OutLocation)
{
	if (!Objective.IsValid())
	{
		Objective = ABDObjective::Get(GetWorld());
	}

	if (const ABDObjective* Urn = Objective.Get())
	{
		OutLocation = Urn->GetActorLocation();
		return true;
	}

	const UBDGridSubsystem* Grid = GetGrid();
	GetSpawnPoints();
	if (Grid == nullptr || GoalCells.Num() == 0)
	{
		return false;
	}

	if (!bWarnedNoObjective)
	{
		bWarnedNoObjective = true;
		UE_LOG(LogBDWave, Warning,
			TEXT("No BD Objective actor in the level: creeps converge on the middle of the Goal cells. Place one on the urn."));
	}

	OutLocation = FVector::ZeroVector;
	for (const FBDCellCoord& Goal : GoalCells)
	{
		OutLocation += Grid->CellToWorld(Goal);
	}
	OutLocation /= GoalCells.Num();
	return true;
}

bool UBDWaveSubsystem::FindRouteToGoal(const FBDCellCoord& From, TArray<FBDCellCoord>& OutRoute, const FBDRouteCost* Cost) const
{
	OutRoute.Reset();

	const UBDGridSubsystem* Grid = GetGrid();
	const UBDPathfinder* Pathfinder = GetPathfinder();
	if (Grid == nullptr || Pathfinder == nullptr)
	{
		return false;
	}

	// The urn is a few cells wide. A search per goal cell is a handful of searches of a
	// few dozen microseconds, and it lets each spawn end at the near side of the urn.
	// Over a cost map the goal cells are compared by what they cost, not by how many
	// cells away they are: that is the whole question the map is asking.
	const auto CostOf = [Grid, Cost](const TArray<FBDCellCoord>& Route) -> float
	{
		if (Cost == nullptr || Cost->IsUniform())
		{
			return static_cast<float>(Route.Num());
		}

		float Total = 0.0f;
		for (int32 Step = 1; Step < Route.Num(); ++Step)
		{
			Total += Cost->MultiplierAt(Route[Step].Y * Grid->GetSizeX() + Route[Step].X);
		}
		return Total;
	};

	TArray<FBDCellCoord> Candidate;
	float BestCost = 0.0f;
	for (const FBDCellCoord& Goal : GoalCells)
	{
		if (!Pathfinder->FindPath(Grid, From, Goal, Candidate, Cost))
		{
			continue;
		}

		const float CandidateCost = CostOf(Candidate);
		if (OutRoute.Num() == 0 || CandidateCost < BestCost)
		{
			OutRoute = Candidate;
			BestCost = CandidateCost;
		}
	}

	return OutRoute.Num() > 0;
}

FBDRouteCost UBDWaveSubsystem::DrawRouteCost(const int32 SpawnPointIndex)
{
	const UBDWaveSettings& Settings = UBDWaveSettings::Get();

	FBDRouteCost Cost;
	Cost.Variance = FMath::Max(1.0f, Settings.RouteCostVariance);
	if (Cost.IsUniform())
	{
		return Cost;
	}

	// Same ingredients as the draw of the open mouths, so a seed reproduces the whole
	// wave: which mouths, and which way out of each. Per creep, the running count is what
	// tells two creeps of the same mouth apart; per mouth and wave, it is left out and
	// every creep of the mouth lands on the same map.
	const ABDMatchManager* Match = GetMatch();
	uint32 Hash = HashCombine(::GetTypeHash(Match != nullptr ? Match->ObstacleSeed : 0), ::GetTypeHash(WaveNumber));
	Hash = HashCombine(Hash, ::GetTypeHash(SpawnPointIndex));
	if (Settings.RouteVarianceMode == EBDRouteVarianceMode::PerCreep)
	{
		Hash = HashCombine(Hash, ::GetTypeHash(RouteCostDraws));
	}
	++RouteCostDraws;

	Cost.Seed = static_cast<int32>(Hash);
	return Cost;
}

void UBDWaveSubsystem::RerouteLivingEnemies()
{
	int32 Rerouted = 0;
	TArray<FBDCellCoord> Route;
	for (ABDEnemyBase* Enemy : LivingEnemies)
	{
		if (Enemy == nullptr || Enemy->HasArrived() || Enemy->IsOnFinalLeg())
		{
			// On the final leg the creep is already inside the urn cells, walking to the
			// actor; there is no route left for the board to change.
			continue;
		}

		// From the cell it is heading to: the crossing under way is finished first, so
		// the creep never reverses in the middle of an edge.
		const FBDCellCoord Heading = Enemy->GetHeadingCell();
		if (FindRouteToGoal(Heading, Route, &Enemy->RouteCost))
		{
			const int32 CellsLeftBefore = Enemy->GetPath().Num() - Enemy->GetCurrentPathIndex();
			Enemy->SetPath(Route);
			++Rerouted;

			UE_LOG(LogBDWave, Verbose, TEXT("%s rerouted from %s: %d cells left, was %d."),
				*Enemy->GetName(), *Heading.ToString(), Route.Num(), CellsLeftBefore);
		}
		else
		{
			// Only possible when the player fenced a cell in with a creep inside it, which
			// the blocking check allows on purpose. It stays put; nothing else to do.
			UE_LOG(LogBDWave, Warning, TEXT("%s at %s has no route to the urn after the board changed; it stops there."),
				*Enemy->GetName(), *Heading.ToString());
			Enemy->SetPath({ Heading });
		}
	}

	UE_LOG(LogBDWave, Log, TEXT("Board changed (grid version %d): %d creep(s) rerouted."), RoutesGridVersion, Rerouted);
}

//~ Spawning -------------------------------------------------------------------

ABDEnemyBase* UBDWaveSubsystem::SpawnEnemy(const UBDEnemyData* Data, const int32 SpawnPointIndex)
{
	return SpawnEnemyAs(nullptr, Data, SpawnPointIndex, 0.0f);
}

ABDEnemyBase* UBDWaveSubsystem::SpawnEnemyAs(const TSubclassOf<ABDEnemyBase> EnemyClassOverride, const UBDEnemyData* Data, const int32 SpawnPointIndex, const float MaxHealthOverride)
{
	UWorld* World = GetWorld();
	if (World == nullptr || !World->IsGameWorld())
	{
		UE_LOG(LogBDWave, Error, TEXT("Creeps can only be spawned in a game world."));
		return nullptr;
	}

	if (Data == nullptr)
	{
		UE_LOG(LogBDWave, Error, TEXT("SpawnEnemy called with no enemy data."));
		return nullptr;
	}

	const TArray<FBDSpawnPoint>& Points = GetSpawnPoints();
	if (!Points.IsValidIndex(SpawnPointIndex))
	{
		UE_LOG(LogBDWave, Error, TEXT("Spawn point %d does not exist; the board has %d."), SpawnPointIndex, Points.Num());
		return nullptr;
	}

	const FBDSpawnPoint& Point = Points[SpawnPointIndex];
	if (Point.Route.Num() == 0)
	{
		UE_LOG(LogBDWave, Error, TEXT("Spawn point %d at %s has no route to the urn."), SpawnPointIndex, *Point.ExitCell.ToString());
		return nullptr;
	}

	UClass* EnemyClass = EnemyClassOverride != nullptr
		? EnemyClassOverride.Get()
		: (Data->EnemyClass.IsNull() ? ABDEnemyBase::StaticClass() : Data->EnemyClass.LoadSynchronous());
	if (EnemyClass == nullptr)
	{
		UE_LOG(LogBDWave, Error, TEXT("Enemy class %s of %s failed to load."), *Data->EnemyClass.ToString(), *Data->GetName());
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const UBDGridSubsystem* Grid = GetGrid();
	const FVector SpawnLocation = Grid != nullptr ? Grid->CellToWorld(Point.ExitCell) : FVector::ZeroVector;

	ABDEnemyBase* Enemy = World->SpawnActor<ABDEnemyBase>(EnemyClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (Enemy == nullptr)
	{
		UE_LOG(LogBDWave, Error, TEXT("Failed to spawn %s at spawn point %d."), *EnemyClass->GetName(), SpawnPointIndex);
		return nullptr;
	}

	// The route goes in before the creep ticks once: SpawnActor returns before the first
	// tick. It is the creep's own, searched over its cost map; the mouth's shortest route
	// is the fallback, which the same graph should never need.
	Enemy->SpawnPointIndex = SpawnPointIndex;
	Enemy->RouteCost = DrawRouteCost(SpawnPointIndex);

	TArray<FBDCellCoord> OwnRoute;
	if (Enemy->RouteCost.IsUniform() || !FindRouteToGoal(Point.ExitCell, OwnRoute, &Enemy->RouteCost))
	{
		OwnRoute = Point.Route;
	}

	const ABDMatchManager* Match = GetMatch();
	Enemy->InitializeEnemy(Data, OwnRoute, Match != nullptr ? Match->GetHealthScale() : 1.0f, MaxHealthOverride);
	LivingEnemies.Add(Enemy);

	// The spread of the wave is how many different routes it was dealt; the wave summary
	// reports it, and the route itself is there for whoever wants to see where they split.
	uint32 RouteHash = 0;
	FString RouteText;
	for (const FBDCellCoord& Cell : OwnRoute)
	{
		RouteHash = HashCombine(RouteHash, ::GetTypeHash(Cell.Y * 4096 + Cell.X));
		RouteText += Cell.ToString() + TEXT(" ");
	}
	WaveRouteHashes.Add(RouteHash);
	UE_LOG(LogBDWave, VeryVerbose, TEXT("%s route (seed %d, variance %.2f): %s"),
		*Enemy->GetName(), Enemy->RouteCost.Seed, Enemy->RouteCost.Variance, *RouteText);

	// Under the spawn loop this is the line that would flood the log; the loop summarizes instead.
	UE_CLOG(!bSpawnLoopRunning, LogBDWave, Log, TEXT("%s (%s) out of spawn point %d at %s, %d cells to the urn (shortest %d). %d creep(s) on the board."),
		*Enemy->GetName(), *Data->GetName(), SpawnPointIndex, *Point.ExitCell.ToString(),
		OwnRoute.Num(), Point.Route.Num(), LivingEnemies.Num());
	UE_CLOG(bSpawnLoopRunning, LogBDWave, Verbose, TEXT("%s (%s) out of spawn point %d, %d cells to the urn (shortest %d)."),
		*Enemy->GetName(), *Data->GetName(), SpawnPointIndex, OwnRoute.Num(), Point.Route.Num());

	return Enemy;
}

int32 UBDWaveSubsystem::SpawnEnemyAtEveryPoint(const UBDEnemyData* Data)
{
	const int32 PointCount = GetSpawnPointCount();

	int32 Spawned = 0;
	for (int32 Index = 0; Index < PointCount; ++Index)
	{
		if (SpawnEnemy(Data, Index) != nullptr)
		{
			++Spawned;
		}
	}

	return Spawned;
}

int32 UBDWaveSubsystem::DespawnAll()
{
	StopSpawnLoop();
	WaveSpawnsRemaining = 0;

	TArray<ABDEnemyBase*> Enemies;
	GetLivingEnemies(Enemies);
	// Destroying reports back through NotifyEnemyRemoved, which scores nothing and drops the entry.
	for (ABDEnemyBase* Enemy : Enemies)
	{
		Enemy->Destroy();
	}
	LivingEnemies.Reset();

	return Enemies.Num();
}

int32 UBDWaveSubsystem::KillAll()
{
	// Kill removes the creep from LivingEnemies through its report, so walk a copy.
	TArray<ABDEnemyBase*> Enemies;
	GetLivingEnemies(Enemies);

	for (ABDEnemyBase* Enemy : Enemies)
	{
		Enemy->Kill();
	}

	return Enemies.Num();
}

int32 UBDWaveSubsystem::GetLivingWaveCreepCount() const
{
	int32 Count = 0;
	for (const ABDEnemyBase* Enemy : LivingEnemies)
	{
		if (Enemy != nullptr && !Enemy->IsCandidate())
		{
			++Count;
		}
	}
	return Count;
}

bool UBDWaveSubsystem::IsSpawningHeld() const
{
	// While the fallen candidates parade, no ordinary creep walks.
	const UWorld* World = GetWorld();
	const UBDCandidateSubsystem* Candidates = World != nullptr ? World->GetSubsystem<UBDCandidateSubsystem>() : nullptr;
	return Candidates != nullptr && Candidates->IsReturnActive();
}

void UBDWaveSubsystem::CancelRemainingSpawns(const TCHAR* Why)
{
	if (WaveSpawnsRemaining <= 0)
	{
		return;
	}
	UE_LOG(LogBDWave, Log, TEXT("Wave %d drops its %d unsent creep(s): %s."), WaveNumber, WaveSpawnsRemaining, Why);
	WaveSpawnsRemaining = 0;
}

void UBDWaveSubsystem::GetLivingEnemies(TArray<ABDEnemyBase*>& OutEnemies) const
{
	OutEnemies.Reset(LivingEnemies.Num());
	for (ABDEnemyBase* Enemy : LivingEnemies)
	{
		if (Enemy != nullptr)
		{
			OutEnemies.Add(Enemy);
		}
	}
}

//~ Reports from the creeps ----------------------------------------------------

void UBDWaveSubsystem::ReportWastedDamage(const float Damage, const bool bLostShot)
{
	WaveWastedDamage += FMath::Max(0.0f, Damage);
	if (bLostShot)
	{
		++WaveLostShots;
	}

	// Null votes: the waste, at the same rate as the score, handed over in whole votes.
	NullDamageOwed += FMath::Max(0.0f, Damage);
	const float HealthPerVote = FMath::Max(0.01f, UBDGameBalanceSettings::Get().HealthPerVote);
	const int32 Whole = FMath::FloorToInt(NullDamageOwed / HealthPerVote);
	if (Whole > 0)
	{
		NullDamageOwed -= Whole * HealthPerVote;
		if (ABDMatchManager* Match = GetMatch())
		{
			Match->AddVotesNull(Whole);
		}
	}
}

void UBDWaveSubsystem::NotifyEnemyArrived(ABDEnemyBase* Enemy)
{
	if (Enemy == nullptr || !LivingEnemies.Contains(Enemy))
	{
		return;
	}

	// Worth its health: the later the leak, the more it costs. A candidate is the defeat, not votes.
	// The red side carries its own weight: an arrival is meant to land harder than a kill,
	// so the count is still a contest once the defense starts to give.
	const int32 Votes = Enemy->IsCandidate() ? 0 : UBDGameBalanceSettings::Get().RedVotesForHealth(Enemy->GetMaxHealth());

	if (ABDMatchManager* Match = GetMatch())
	{
		Match->AddVotesRed(Votes);
	}

	++SpawnLoopArrived;
	++WaveArrived;
	UE_CLOG(!bSpawnLoopRunning, LogBDWave, Log, TEXT("%s reached the urn: red +%d."), *Enemy->GetName(), Votes);
	UE_CLOG(bSpawnLoopRunning, LogBDWave, Verbose, TEXT("%s reached the urn: red +%d."), *Enemy->GetName(), Votes);
	ForgetEnemy(Enemy);
}

void UBDWaveSubsystem::NotifyEnemyDied(ABDEnemyBase* Enemy)
{
	if (Enemy == nullptr || !LivingEnemies.Contains(Enemy))
	{
		return;
	}

	// A candidate scores nothing either way: the candidate subsystem says what his end does.
	const UBDEnemyData* Data = Enemy->GetData();
	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
	const int32 Votes = Enemy->IsCandidate() ? 0
		: (Balance.bBlueVotesByHealth ? Balance.VotesForHealth(Enemy->GetMaxHealth()) : (Data != nullptr ? Data->VotesOnDeath : 0));

	if (ABDMatchManager* Match = GetMatch())
	{
		Match->AddVotesBlue(Votes);
	}

	++SpawnLoopKilled;
	++WaveKilled;
	UE_CLOG(!bSpawnLoopRunning, LogBDWave, Log, TEXT("%s killed: blue +%d."), *Enemy->GetName(), Votes);
	UE_CLOG(bSpawnLoopRunning, LogBDWave, Verbose, TEXT("%s killed: blue +%d."), *Enemy->GetName(), Votes);
	ForgetEnemy(Enemy);
}

void UBDWaveSubsystem::NotifyEnemyRemoved(ABDEnemyBase* Enemy)
{
	// A creep that already arrived or died is no longer in the list; this is for the ones
	// that vanished some other way, which score nothing and do not clear a wave either.
	if (Enemy != nullptr && LivingEnemies.Contains(Enemy))
	{
		UE_LOG(LogBDWave, Verbose, TEXT("%s removed from the board without arriving or dying."), *Enemy->GetName());
		LivingEnemies.Remove(Enemy);
	}
}

void UBDWaveSubsystem::ForgetEnemy(ABDEnemyBase* Enemy)
{
	LivingEnemies.Remove(Enemy);

	// The candidate is not a creep of the wave: it walks for minutes and the waves keep
	// coming while it does, so a board with only the candidate left on it is empty here.
	if (GetLivingWaveCreepCount() > 0)
	{
		return;
	}

	// Creeps spawned from the console during the building phase leave a board that was
	// never "in a wave"; only a wave actually out, with nothing left to send, is cleared.
	ABDMatchManager* Match = GetMatch();
	if (Match != nullptr && Match->GetPhase() == EBDMatchPhase::WaveActive && WaveSpawnsRemaining <= 0 && !bSpawnLoopRunning)
	{
		// The peak is the number to watch: a wave whose creeps pile up faster than they die
		// is the wave the balance got wrong, whatever the kill count says.
		UE_LOG(LogBDWave, Log, TEXT("Wave %d done: %d sent over %d distinct route(s), peak %d alive at once, %d killed, %d reached the urn | %d shot(s), %.0f of %.0f health dealt, %.0f wasted (%d lost on dead creeps)."),
			WaveNumber, WaveSpawnedTotal, WaveRouteHashes.Num(), WavePeakAlive, WaveKilled, WaveArrived,
			WaveShotsFired, WaveDamageDealt, WaveTotalHealth, WaveWastedDamage, WaveLostShots);
		Match->OnWaveCleared();
	}
}

//~ Debug ----------------------------------------------------------------------

void UBDWaveSubsystem::DrawRoutes() const
{
	const UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = GetGrid();
	if (World == nullptr || Grid == nullptr)
	{
		return;
	}

	const UBDWaveSettings& Settings = UBDWaveSettings::Get();
	const UBDGridSettings& GridSettings = UBDGridSettings::Get();
	const FVector HeightOffset(0.0f, 0.0f, Settings.RouteDrawHeightOffset);

	const auto DrawRoute = [World, Grid, &HeightOffset](const TArray<FBDCellCoord>& Route, const int32 FromStep,
		const FColor& Color, const float Thickness, const float Height)
	{
		const FVector Offset = HeightOffset + FVector(0.0f, 0.0f, Height);
		for (int32 Step = FMath::Max(0, FromStep); Step < Route.Num() - 1; ++Step)
		{
			DrawDebugLine(World,
				Grid->CellToWorld(Route[Step]) + Offset,
				Grid->CellToWorld(Route[Step + 1]) + Offset,
				Color, BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime,
				BDGridDebug::DepthPriority, Thickness);
		}
	};

	// The mouths of the current wave, or every mouth while no wave has drawn yet: before
	// the first wave the question is whether the board routes at all.
	TArray<int32> Mouths = ActiveSpawnPoints;
	if (Mouths.Num() == 0)
	{
		for (int32 Index = 0; Index < SpawnPoints.Num(); ++Index)
		{
			Mouths.Add(Index);
		}
	}

	for (const int32 Index : Mouths)
	{
		if (!SpawnPoints.IsValidIndex(Index))
		{
			continue;
		}

		const FBDSpawnPoint& Point = SpawnPoints[Index];
		if (Point.Route.Num() == 0)
		{
			DrawDebugSphere(World, Grid->CellToWorld(Point.ExitCell) + HeightOffset,
				Settings.RouteBlockedMarkerRadius, GridSettings.PathEndpointSegments, Settings.RouteBlockedColor,
				BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, GridSettings.LineThickness);
			continue;
		}

		DrawRoute(Point.Route, 0, Settings.GetRouteColor(Index), Settings.RouteLineThickness, 0.0f);
	}

	// What is left of every living creep's own route, thick, a little under the shortest
	// line so that one stays visible on top. Where the horde spread, the thick lines fan
	// out of the thin one; where every creep drew the same map, they pile into one.
	const float CreepHeight = -Settings.RouteDrawHeightOffset * 0.25f;
	for (const ABDEnemyBase* Enemy : LivingEnemies)
	{
		if (Enemy == nullptr || Enemy->HasArrived())
		{
			continue;
		}

		DrawRoute(Enemy->GetPath(), Enemy->GetCurrentPathIndex() - 1,
			Settings.GetRouteColor(Enemy->SpawnPointIndex), Settings.CreepRouteLineThickness, CreepHeight);
	}
}

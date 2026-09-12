// Brazil Defense. Where the creeps come from, which way they walk and who is still out.

#include "Wave/BDWaveSubsystem.h"

#include "BDLog.h"
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
		TEXT("1 draws the route from every spawn point to the urn, one color per point. 0 to hide."),
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
	const UBDWaveSettings& Settings = UBDWaveSettings::Get();
	const UBDEnemyData* Data = Settings.WaveEnemy.LoadSynchronous();
	if (Data == nullptr)
	{
		Data = Settings.DebugEnemy.LoadSynchronous();
	}

	if (Data == nullptr)
	{
		UE_LOG(LogBDWave, Error, TEXT("Wave %d has no enemy to send: set Wave Enemy in Project Settings > Brazil Defense - Waves."), Wave);
		return;
	}

	// One more creep per spawn point every wave: the pressure grows with the board, not
	// with a flat number, so a map with more mouths is a harder map.
	const int32 PointCount = GetSpawnPointCount();
	const int32 PerPoint = UBDGameBalanceSettings::Get().GetCreepsPerSpawnPoint(Wave);

	WaveEnemyData = Data;
	WaveSpawnsRemaining = PerPoint * PointCount;
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

	UE_LOG(LogBDWave, Log, TEXT("Wave %d: %d x %s (%d per spawn point x %d points) at x%.2f health, one every %.2fs."),
		Wave, WaveSpawnsRemaining, *Data->GetName(), PerPoint, PointCount,
		UBDGameBalanceSettings::Get().GetHealthScale(Wave), WaveSpawnInterval);
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

	// Round robin over the points; a point with no route is skipped, not retried forever.
	for (int32 Attempt = 0; Attempt < PointCount; ++Attempt)
	{
		const int32 Index = WaveSpawnCursor % PointCount;
		++WaveSpawnCursor;
		if (SpawnEnemy(WaveEnemyData, Index) != nullptr)
		{
			--WaveSpawnsRemaining;
			++WaveSpawnedTotal;
			WavePeakAlive = FMath::Max(WavePeakAlive, LivingEnemies.Num());
			if (LivingEnemies.Num() > 0 && LivingEnemies.Last() != nullptr)
			{
				WaveTotalHealth += LivingEnemies.Last()->GetMaxHealth();
			}
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

	if (WaveSpawnsRemaining > 0)
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

		if (!FindRouteToGoal(Point.ExitCell, Point.Route))
		{
			++Unreachable;
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

bool UBDWaveSubsystem::FindRouteToGoal(const FBDCellCoord& From, TArray<FBDCellCoord>& OutRoute) const
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
	TArray<FBDCellCoord> Candidate;
	for (const FBDCellCoord& Goal : GoalCells)
	{
		if (Pathfinder->FindPath(Grid, From, Goal, Candidate) && (OutRoute.Num() == 0 || Candidate.Num() < OutRoute.Num()))
		{
			OutRoute = Candidate;
		}
	}

	return OutRoute.Num() > 0;
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
		if (FindRouteToGoal(Heading, Route))
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

	UClass* EnemyClass = Data->EnemyClass.IsNull() ? ABDEnemyBase::StaticClass() : Data->EnemyClass.LoadSynchronous();
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

	// The route goes in before the creep ticks once: SpawnActor returns before the first tick.
	Enemy->SpawnPointIndex = SpawnPointIndex;
	const ABDMatchManager* Match = GetMatch();
	Enemy->InitializeEnemy(Data, Point.Route, Match != nullptr ? Match->GetHealthScale() : 1.0f);
	LivingEnemies.Add(Enemy);

	// Under the spawn loop this is the line that would flood the log; the loop summarizes instead.
	UE_CLOG(!bSpawnLoopRunning, LogBDWave, Log, TEXT("%s (%s) out of spawn point %d at %s, %d cells to the urn. %d creep(s) on the board."),
		*Enemy->GetName(), *Data->GetName(), SpawnPointIndex, *Point.ExitCell.ToString(),
		Point.Route.Num(), LivingEnemies.Num());
	UE_CLOG(bSpawnLoopRunning, LogBDWave, Verbose, TEXT("%s (%s) out of spawn point %d, %d cells to the urn."),
		*Enemy->GetName(), *Data->GetName(), SpawnPointIndex, Point.Route.Num());

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
}

void UBDWaveSubsystem::NotifyEnemyArrived(ABDEnemyBase* Enemy)
{
	if (Enemy == nullptr || !LivingEnemies.Contains(Enemy))
	{
		return;
	}

	const UBDEnemyData* Data = Enemy->GetData();
	const int32 Votes = Data != nullptr ? Data->VotesOnArrival : 0;

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

	const UBDEnemyData* Data = Enemy->GetData();
	const int32 Votes = Data != nullptr ? Data->VotesOnDeath : 0;

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

	if (LivingEnemies.Num() > 0)
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
		UE_LOG(LogBDWave, Log, TEXT("Wave %d done: %d sent, peak %d alive at once, %d killed, %d reached the urn | %d shot(s), %.0f of %.0f health dealt, %.0f wasted (%d lost on dead creeps)."),
			WaveNumber, WaveSpawnedTotal, WavePeakAlive, WaveKilled, WaveArrived,
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

	for (int32 Index = 0; Index < SpawnPoints.Num(); ++Index)
	{
		const FBDSpawnPoint& Point = SpawnPoints[Index];

		if (Point.Route.Num() == 0)
		{
			DrawDebugSphere(World, Grid->CellToWorld(Point.ExitCell) + HeightOffset,
				Settings.RouteBlockedMarkerRadius, GridSettings.PathEndpointSegments, Settings.RouteBlockedColor,
				BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, GridSettings.LineThickness);
			continue;
		}

		const FColor Color = Settings.GetRouteColor(Index);
		for (int32 Step = 0; Step < Point.Route.Num() - 1; ++Step)
		{
			DrawDebugLine(World,
				Grid->CellToWorld(Point.Route[Step]) + HeightOffset,
				Grid->CellToWorld(Point.Route[Step + 1]) + HeightOffset,
				Color, BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime,
				BDGridDebug::DepthPriority, Settings.RouteLineThickness);
		}
	}
}

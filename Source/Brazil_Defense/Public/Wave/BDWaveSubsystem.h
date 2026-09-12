// Brazil Defense. Where the creeps come from, which way they walk and who is still out.

#pragma once

#include "CoreMinimal.h"
#include "Grid/BDGridTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDWaveSubsystem.generated.h"

class ABDEnemyBase;
class ABDMatchManager;
class ABDObjective;
class UBDEnemyData;
class UBDGridSubsystem;
class UBDPathfinder;

/**
 * One entry point of the creeps: a run of adjacent Spawn cells on the border of the
 * board, and the route from it to the urn.
 */
USTRUCT(BlueprintType)
struct FBDSpawnPoint
{
	GENERATED_BODY()

	/** Every Spawn cell of the run, in row major order. */
	UPROPERTY(BlueprintReadOnly, Category = "Brazil Defense|Wave")
	TArray<FBDCellCoord> Cells;

	/** Cell the creeps actually come out of: the middle of the run. */
	UPROPERTY(BlueprintReadOnly, Category = "Brazil Defense|Wave")
	FBDCellCoord ExitCell;

	/** Route from ExitCell to the urn, inclusive at both ends. Empty when there is none. */
	UPROPERTY(BlueprintReadOnly, Category = "Brazil Defense|Wave")
	TArray<FBDCellCoord> Route;
};

/**
 * Spawns the creeps and keeps track of them until they arrive or die.
 *
 * The spawn points are read off the grid, not authored twice: every run of adjacent
 * Spawn cells is one entry point, and its route is asked from UBDPathfinder. Routes are
 * recomputed whenever the grid changes, and every creep already walking is handed the
 * new route from the cell it is heading to, so a fence placed while creeps are out is
 * respected by them as well.
 *
 * The urn is a run of Goal cells too. A route ends at whichever Goal cell is the
 * closest, and from there the creep walks to the ABDObjective actor, so creeps from
 * the top and from the bottom of the board enter the urn from their own side and still
 * converge on the same point.
 *
 * This is the seam ABDMatchManager was waiting for: arrivals and kills go to its vote
 * counters, and the board being empty again ends the wave.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDWaveSubsystem : public UTickableWorldSubsystem
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
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Wave", meta = (WorldContext = "WorldContextObject"))
	static UBDWaveSubsystem* Get(const UObject* WorldContextObject);

	//~ Spawn points and routes ----------------------------------------------

	/** The entry points of the board, refreshed from the grid if it changed since the last call. */
	const TArray<FBDSpawnPoint>& GetSpawnPoints();

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Wave")
	int32 GetSpawnPointCount();

	/** Forces the spawn points and routes to be read again from the grid. */
	void RefreshRoutes();

	/**
	 * Where the creeps converge once their route is walked: the ABDObjective actor of the
	 * level, or the middle of the Goal cells when none is placed, which is logged once.
	 * @return false when there is neither.
	 */
	bool GetObjectiveLocation(FVector& OutLocation);

	//~ Spawning -------------------------------------------------------------

	/**
	 * Spawns one creep at a spawn point and sends it down the route.
	 * @return the creep, or null when the point does not exist, has no route, or the data cannot be spawned.
	 */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Wave")
	ABDEnemyBase* SpawnEnemy(const UBDEnemyData* Data, int32 SpawnPointIndex);

	/** Spawns one creep at every spawn point. @return how many were spawned. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Wave")
	int32 SpawnEnemyAtEveryPoint(const UBDEnemyData* Data);

	//~ Spawn loop, for load testing ------------------------------------------

	/**
	 * Sends one creep out of every spawn point every Interval seconds until stopped.
	 * Debug tooling: the point is dozens of creeps at once, to watch the frame rate and
	 * whether towers pick sensible targets out of a queue. While it runs the per creep
	 * log lines drop to Verbose and a summary goes out every LoopLogEvery spawns.
	 */
	void StartSpawnLoop(const UBDEnemyData* Data, float Interval);
	void StopSpawnLoop();

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Wave")
	bool IsSpawnLoopRunning() const { return bSpawnLoopRunning; }

	/** Kills every creep on the board. Counts as kills: their VotesOnDeath are scored. @return how many were killed. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Wave")
	int32 KillAll();

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Wave")
	int32 GetLivingEnemyCount() const { return LivingEnemies.Num(); }

	/** Every creep currently out. Dead entries are already dropped. */
	void GetLivingEnemies(TArray<ABDEnemyBase*>& OutEnemies) const;

	/** The same list without a copy, for the towers scanning it every tick. May hold nulls for a frame. */
	const TArray<TObjectPtr<ABDEnemyBase>>& GetLivingEnemiesRef() const { return LivingEnemies; }

	//~ Reports from the creeps. Not meant to be called by anything else. -----

	void NotifyEnemyArrived(ABDEnemyBase* Enemy);
	void NotifyEnemyDied(ABDEnemyBase* Enemy);
	void NotifyEnemyRemoved(ABDEnemyBase* Enemy);

private:
	UBDGridSubsystem* GetGrid() const;
	UBDPathfinder* GetPathfinder() const;
	ABDMatchManager* GetMatch() const;

	/** Reads the runs of Spawn cells off the grid and pathfinds each to the closest Goal cell. */
	void BuildSpawnPoints();

	/** Shortest route from a cell to any Goal cell. @return false when no Goal is reachable. */
	bool FindRouteToGoal(const FBDCellCoord& From, TArray<FBDCellCoord>& OutRoute) const;

	/** Hands every creep still walking a fresh route from the cell it is heading to. */
	void RerouteLivingEnemies();

	/** Drops a creep from the living list and ends the wave when it was the last one. */
	void ForgetEnemy(ABDEnemyBase* Enemy);

	void DrawRoutes() const;

	/** Grid change handlers: the spawn point routes go stale and every creep out is rerouted. */
	void MarkBoardChanged();
	void HandleGridRebuilt();
	void HandleCellStateChanged(const FBDCellCoord& Coord, EBDCellState NewState);
	void HandleEdgeBlockedChanged(const FBDEdgeCoord& Edge, bool bBlocked);

	UPROPERTY(Transient)
	TArray<FBDSpawnPoint> SpawnPoints;

	/** Goal cells, cached with the spawn points because every route ends at one of them. */
	TArray<FBDCellCoord> GoalCells;

	/** The urn actor, found once. Weak: the level owns it. */
	TWeakObjectPtr<ABDObjective> Objective;

	/** So the missing objective is reported once per world, not once per creep. */
	bool bWarnedNoObjective = false;

	/** Grid version the spawn points were read at. Anything else means they are stale. */
	int32 RoutesGridVersion = 0;

	/** Set by the grid handlers; cleared when the spawn points are read again. */
	bool bRoutesDirty = true;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ABDEnemyBase>> LivingEnemies;

	/** See StartSpawnLoop. */
	static constexpr int32 LoopLogEvery = 10;
	bool bSpawnLoopRunning = false;
	float SpawnLoopInterval = 0.0f;
	float SpawnLoopTimer = 0.0f;
	int32 SpawnLoopSpawned = 0;
	int32 SpawnLoopArrived = 0;
	int32 SpawnLoopKilled = 0;
	int32 SpawnLoopPeak = 0;
	double SpawnLoopStartSeconds = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<const UBDEnemyData> SpawnLoopData;

	FDelegateHandle GridRebuiltHandle;
	FDelegateHandle CellStateChangedHandle;
	FDelegateHandle EdgeBlockedChangedHandle;
};

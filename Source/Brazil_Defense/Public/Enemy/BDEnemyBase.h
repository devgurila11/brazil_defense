// Brazil Defense. A creep walking a route over the grid.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Grid/BDGridTypes.h"
#include "BDEnemyBase.generated.h"

class UBDEnemyData;
class UBDGridSubsystem;
class UBDWaveSubsystem;
class UStaticMeshComponent;

/**
 * The one creep class. It is handed a route of cells at spawn and walks it, center to
 * center, until the last cell, where it reports to UBDWaveSubsystem and disappears.
 *
 * Movement is done by hand, in Tick, along the cell centers. No character movement
 * component and no navmesh on purpose: the route already is the answer, computed by
 * UBDPathfinder over the same grid the player builds on, and the maze the player made
 * has to be respected exactly, fence by fence. A physics driven mover sliding along
 * collision would cut corners the grid says are closed.
 *
 * Speeds live on the data asset in cells per second and are turned into centimetres
 * here, with the cell size of the grid it walks on.
 */
UCLASS(Blueprintable, meta = (DisplayName = "BD Enemy Base"))
class BRAZIL_DEFENSE_API ABDEnemyBase : public APawn
{
	GENERATED_BODY()

public:
	ABDEnemyBase();

	//~ Begin AActor interface
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor interface

	/**
	 * Called once by the wave subsystem right after spawning, before the first tick.
	 * Applies the data (health, mesh) and the route.
	 */
	void InitializeEnemy(const UBDEnemyData* InData, const TArray<FBDCellCoord>& InPath);

	/**
	 * Replaces the route. The new path is expected to start at the cell the creep is
	 * heading to, so the crossing already under way is finished and then the new route
	 * is followed. Used when the board changes under a creep already walking.
	 */
	void SetPath(const TArray<FBDCellCoord>& InPath);

	/** Ends this creep as a kill: its VotesOnDeath go to the blue counter and it is removed. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Enemy")
	void Kill();

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	const UBDEnemyData* GetData() const { return Data; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	const TArray<FBDCellCoord>& GetPath() const { return Path; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	int32 GetCurrentPathIndex() const { return CurrentPathIndex; }

	/**
	 * The cell the creep is walking towards, or the cell it stands on once the route is
	 * done. This is where a new route has to start from.
	 */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	FBDCellCoord GetHeadingCell() const;

	/** Whether the route has been walked to its end. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	bool HasArrived() const { return Path.Num() > 0 && CurrentPathIndex >= Path.Num(); }

	/** Current speed along the route, in centimetres per second. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	float GetCurrentSpeed() const { return CurrentSpeed; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	UStaticMeshComponent* GetMesh() const { return Mesh; }

	/** Index of the spawn point this creep came out of, set by the wave subsystem. Debug and logging only. */
	int32 SpawnPointIndex = INDEX_NONE;

protected:
	/**
	 * Where the route ends for this creep: reports the arrival and removes it.
	 * Virtual so a Blueprint child can play something before the base removes the actor.
	 */
	virtual void Arrive();

	/** Same as Arrive for a kill. */
	virtual void Die();

private:
	UBDGridSubsystem* GetGrid() const;
	UBDWaveSubsystem* GetWaves() const;

	/** World location of the floor under a cell center. Traces once; the result is cached per route. */
	FVector ResolveWaypoint(const FBDCellCoord& Coord) const;

	/** Rebuilds the cached world positions of the route from Path. */
	void RebuildWaypoints();

	/** Sets the mesh from the data and rests it on the root, whatever its pivot. */
	void ApplyMesh();

	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense|Enemy")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Brazil Defense|Enemy")
	TObjectPtr<const UBDEnemyData> Data;

	/** Route received at spawn, from the spawn cell to the goal cell inclusive. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Brazil Defense|Enemy")
	TArray<FBDCellCoord> Path;

	/** World location of each cell of Path, resolved to the floor once so ticking never traces. */
	TArray<FVector> Waypoints;

	/** Cell of Path the creep is currently walking towards. Path.Num() once it arrived. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Brazil Defense|Enemy")
	int32 CurrentPathIndex = 0;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Brazil Defense|Enemy")
	float CurrentHealth = 0.0f;

	/** Centimetres per second. Ramps up from zero with the data acceleration. */
	float CurrentSpeed = 0.0f;

	/** Guards against reporting twice: Arrive and Kill both end in Destroy, which is deferred. */
	bool bFinished = false;
};

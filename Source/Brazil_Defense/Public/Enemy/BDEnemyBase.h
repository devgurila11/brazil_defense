// Brazil Defense. A creep walking a route over the grid.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Grid/BDGridTypes.h"
#include "Path/BDRouteCost.h"
#include "BDEnemyBase.generated.h"

class UBDEnemyData;
class UBDGridSubsystem;
class UBDWaveSubsystem;
class UPrimitiveComponent;
class USkeletalMeshComponentBudgeted;
class UStaticMeshComponent;

/**
 * The one creep class. It is handed a route of cells at spawn and walks it, center to
 * center, to the last cell, then on to the objective actor itself, where it reports to
 * UBDWaveSubsystem and disappears. See ABDObjective for why the route does not simply
 * end on the last cell.
 *
 * Movement is done by hand, in Tick, along the cell centers. No character movement
 * component and no navmesh on purpose: the route already is the answer, computed by
 * UBDPathfinder over the same grid the player builds on, and the maze the player made
 * has to be respected exactly, fence by fence. A physics driven mover sliding along
 * collision would cut corners the grid says are closed.
 *
 * Speeds live on the data asset in cells per second and are turned into centimetres
 * here, with the cell size of the grid it walks on.
 *
 * The route is the same for every creep of a spawn point; the line walked over it is
 * not. Each creep draws a lateral offset and a speed of its own at spawn and keeps
 * them, so a wave fills the width of the corridor instead of queueing on one line, and
 * the corners are cut with a short curve wherever the grid around them is open. Both
 * are cosmetic in the sense that the cells walked never change - but they are what
 * makes the maze readable: where the horde spreads there is room, where it files into a
 * line the player closed something.
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
	 * @param HealthScale multiplier of the wave on MaxHealth: waves get tougher, the data does not change.
	 * @param MaxHealthOverride when above zero, the health outright, data and scale ignored. The candidate is sized this way.
	 */
	void InitializeEnemy(const UBDEnemyData* InData, const TArray<FBDCellCoord>& InPath, float HealthScale = 1.0f, float MaxHealthOverride = 0.0f);

	/**
	 * Whether this is the red candidate rather than a creep of the wave: shot before
	 * anything else by every defender that sees it, and not waited for when the wave
	 * asks whether the board is empty. See ABDCandidate.
	 */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	virtual bool IsCandidate() const { return false; }

	/** Health this creep spawned with, wave scaling applied. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	float GetMaxHealth() const { return MaxHealth; }

	//~ Damage on its way ---------------------------------------------------
	// Every projectile in flight towards this creep books its damage here. A creep whose
	// booked damage covers its health is already dead, it just does not know yet, and no
	// tower should spend another shot on it.

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	float GetIncomingDamage() const { return IncomingDamage; }

	/** Whether the shots already in the air are enough to kill this creep. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	bool IsDoomed() const { return CurrentHealth - IncomingDamage <= 0.0f; }

	/** Called by a projectile when it is launched at this creep. */
	void AddIncomingDamage(float Damage) { IncomingDamage += FMath::Max(0.0f, Damage); }

	/** Called by that projectile when it hits, misses or vanishes. */
	void RemoveIncomingDamage(float Damage) { IncomingDamage = FMath::Max(0.0f, IncomingDamage - FMath::Max(0.0f, Damage)); }

	/**
	 * Replaces the route. The new path is expected to start at the cell the creep is
	 * heading to, so the crossing already under way is finished and then the new route
	 * is followed. Used when the board changes under a creep already walking.
	 */
	void SetPath(const TArray<FBDCellCoord>& InPath);

	/** Ends this creep as a kill: its VotesOnDeath go to the blue counter and it is removed. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Enemy")
	void Kill();

	/**
	 * Takes health off the creep; at zero it dies as a kill. Not AActor::TakeDamage: the
	 * engine damage pipeline carries types, events and controllers this game has no use
	 * for, and a plain number is easier to reason about.
	 * @param Source what dealt the hit, credited with the kill when there is one. May be null.
	 */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Enemy")
	void ApplyDamage(float Damage, AActor* Source);

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	const UBDEnemyData* GetData() const { return Data; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	const TArray<FBDCellCoord>& GetPath() const { return Path; }

	/**
	 * Cell of the route the creep is walking towards. Several waypoints may share it: a
	 * cut corner is a handful of points inside one cell.
	 */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	int32 GetCurrentPathIndex() const;

	/**
	 * The cell the creep is walking towards, or the cell it stands on once the route is
	 * done. This is where a new route has to start from.
	 */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	FBDCellCoord GetHeadingCell() const;

	/** Whether the route has been walked to its end: the last cell and the objective beyond it. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	bool HasArrived() const { return Waypoints.Num() > 0 && CurrentWaypoint >= Waypoints.Num(); }

	/** Whether every cell of the route is behind the creep and it is walking to the objective actor. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	bool IsOnFinalLeg() const { return Path.Num() > 0 && GetCurrentPathIndex() >= Path.Num() && !HasArrived(); }

	/** Current speed along the route, in centimetres per second. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	float GetCurrentSpeed() const { return CurrentSpeed; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	UStaticMeshComponent* GetMesh() const { return Mesh; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	USkeletalMeshComponentBudgeted* GetSkeletalBody() const { return SkeletalBody; }

	/** Whichever of the two bodies is showing, the animated one first. Null when neither has an asset. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Enemy")
	UPrimitiveComponent* GetBody() const;

	/** Index of the spawn point this creep came out of, set by the wave subsystem. Debug and logging only. */
	int32 SpawnPointIndex = INDEX_NONE;

	/**
	 * The cost map this creep's route was searched over, set by the wave subsystem at
	 * spawn and kept: a reroute after the board changed is searched over the same map, so
	 * the creep keeps its own preferences rather than snapping onto the shortest line.
	 */
	FBDRouteCost RouteCost;

protected:
	/**
	 * Where the route ends for this creep: reports the arrival and removes it.
	 * Virtual so a Blueprint child can play something before the base removes the actor.
	 */
	virtual void Arrive();

	/** Same as Arrive for a kill. */
	virtual void Die();

	/** Walking speed before the per creep variance, in cells per second. The data's, unless a child answers otherwise. */
	virtual float GetBaseMoveSpeed() const;

private:
	UBDGridSubsystem* GetGrid() const;
	UBDWaveSubsystem* GetWaves() const;

	/** World location of the floor under a cell center. Traces once; the result is cached per route. */
	FVector ResolveWaypoint(const FBDCellCoord& Coord) const;

	/**
	 * Rebuilds the cached world positions of the route from Path, plus the objective as
	 * the final one. This is where the creep's own line is drawn: the cell centers are
	 * pushed sideways by its lateral offset and the corners that may be cut are replaced
	 * by a short curve.
	 */
	void RebuildWaypoints();

	/**
	 * The lane the creep is on at a cell of the route, as a signed fraction of a cell: the
	 * lane it leans to plus where its wander has got to. Bounded by LateralOffsetMax.
	 */
	float LaneFractionAt(int32 Index) const;

	/**
	 * How far sideways the waypoint of a cell is pushed, mitred so both legs of a turn
	 * keep the same distance from the middle of the route.
	 * @param Centers world position of every cell of the route, floor resolved.
	 */
	FVector ComputeLateralOffset(int32 Index, const TArray<FVector>& Centers, float CellSize) const;

	/**
	 * Radius of the curve cutting the corner at a waypoint, 0 when it is not cut: a
	 * straight run, an end of the route, or a turn the board does not leave room for.
	 *
	 * Whether there is a corner at all is read off Centers, the route itself, never off
	 * Line: a creep drifting across its lane bends its own line every cell, and that is
	 * not the route turning. The radius is then capped against the legs of Line, which is
	 * what the creep actually walks.
	 */
	float ComputeCornerRadius(int32 Index, const TArray<FVector>& Centers, const TArray<FVector>& Line, float CellSize) const;

	/**
	 * Whether the turn at a cell of the route happens in open ground. A turn forced by a
	 * fence, a platform or scenery is walked into and taken on the spot, flush with the
	 * barrier; only a turn with free cells and free edges all around is cut.
	 */
	bool IsCornerOpen(int32 Index) const;

	/** Sets the mesh from the data and rests it on the root, whatever its pivot. */
	void ApplyMesh();

	/** ApplyMesh for a data with a SkeletalMesh: the animated body, its loop and its material. */
	bool ApplySkeletalMesh();

	/** Keeps the feet of the loop in step with CurrentSpeed, so the creep neither skates nor pedals. */
	void UpdateAnimationRate();

	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense|Enemy")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/**
	 * The animated body, used when the data has a SkeletalMesh; Mesh is left empty then.
	 * Budgeted: with hundreds of creeps on the board the animation budget allocator decides
	 * which ones tick at full rate, the nearest to the camera first.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense|Enemy")
	TObjectPtr<USkeletalMeshComponentBudgeted> SkeletalBody;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Brazil Defense|Enemy")
	TObjectPtr<const UBDEnemyData> Data;

	/** Route received at spawn, from the spawn cell to the goal cell inclusive. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Brazil Defense|Enemy")
	TArray<FBDCellCoord> Path;

	/**
	 * The line this creep actually walks: the cell centers of Path resolved to the floor
	 * once so ticking never traces, pushed aside by its lateral offset, with a curve in
	 * place of every corner that may be cut, and the objective as the last point. Longer
	 * than Path whenever a corner was cut.
	 */
	TArray<FVector> Waypoints;

	/** Cell of Path each waypoint belongs to, Path.Num() for the objective. Same length as Waypoints. */
	TArray<int32> WaypointPathIndex;

	/** Waypoint the creep is currently walking towards. Waypoints.Num() once it arrived. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Brazil Defense|Enemy")
	int32 CurrentWaypoint = 0;

	/** The lane this creep leans to, as a signed fraction of a cell. What it wanders around. */
	float LateralOffsetFrac = 0.0f;

	/** How far it wanders off that lane, in cell fractions, and over how many cells one full swing takes. */
	float LaneWanderFrac = 0.0f;
	float LaneWanderCells = 0.0f;

	/** Where in its swing the creep starts, so two creeps side by side never drift together. */
	float LaneWanderPhase = 0.0f;

	/** Multiplier on the corner radius, drawn at spawn: some hug the inside of a turn, some swing wide. */
	float CornerRadiusScale = 1.0f;

	/** Multiplier on the data MoveSpeed, drawn at spawn. Around 1. */
	float SpeedScale = 1.0f;

	/** The slow breath around that pace: how deep, how long one cycle takes, and where it starts. */
	float SpeedBreathFrac = 0.0f;
	float SpeedBreathPeriod = 0.0f;
	float SpeedBreathPhase = 0.0f;

	/** Seconds this creep has been walking, for the breath. Dilated like everything else. */
	float Age = 0.0f;

	/** Half the width of the mesh, so the offset never pushes the body into a fence. */
	float BodyRadius = 0.0f;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Brazil Defense|Enemy")
	float CurrentHealth = 0.0f;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Brazil Defense|Enemy")
	float MaxHealth = 0.0f;

	/** Sum of the damage of every projectile currently flying at this creep. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Brazil Defense|Enemy")
	float IncomingDamage = 0.0f;

	/** Centimetres per second. Ramps up from zero with the data acceleration. */
	float CurrentSpeed = 0.0f;

	/** Guards against reporting twice: Arrive and Kill both end in Destroy, which is deferred. */
	bool bFinished = false;
};

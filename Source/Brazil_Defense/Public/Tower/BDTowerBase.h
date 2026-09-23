// Brazil Defense. The single defender class, wherever it stands.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/BDGridTypes.h"
#include "BDTowerBase.generated.h"

class ABDEnemyBase;
class ABDProjectileBase;
class UBDPlatformComponent;
class UBDTowerData;
class UBDWaveSubsystem;
class UStaticMeshComponent;
struct FBDTowerLevel;

/**
 * The one and only defender type. It behaves the same on a free ground cell and on a
 * platform slot: same class, same data asset, same levels. Standing on a platform only
 * multiplies its range, through the multiplier of that platform.
 *
 * Every tick it looks for a creep in range and holds on to it while it stays there.
 * Seeing one is not shooting it: the tower first waits out its acquisition delay, then
 * turns its weapon onto the creep, and only once aligned fires at the rate of its level.
 * The weapon is a component of its own, so a tower on a slot keeps the facing the slot
 * gave the actor and still tracks. A magazine, when the data has one, empties at that
 * rate and then the tower stops to reload, target or no target. Ranges are in cells on
 * the data and turned into centimetres here, with the cell size of the grid it stands on.
 */
UCLASS(Blueprintable, meta = (DisplayName = "BD Tower Base"))
class BRAZIL_DEFENSE_API ABDTowerBase : public AActor
{
	GENERATED_BODY()

public:
	ABDTowerBase();

	virtual void Tick(float DeltaSeconds) override;

	/** Called once by whoever spawned the tower, before its first tick. Applies the data and the mesh. */
	void InitializeTower(const UBDTowerData* InData);

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	const UBDTowerData* GetData() const { return Data; }

	/** Current level, 1 to UBDTowerData::MaxLevels. Not GetLevel: AActor already owns that name for the ULevel. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	int32 GetTowerLevel() const { return Level; }

	/** The authored stats behind the current level, or null when the tower has no data. */
	const FBDTowerLevel* GetCurrentLevel() const;

	/** Damage of one hit at the current level: authored when the level is authored, the upgrade formula otherwise. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	float GetEffectiveDamage() const;

	//~ Upgrades ---------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	bool IsMaxLevel() const;

	/** Public money the next level costs on the current wave. 0 at max level. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	int32 GetUpgradeCost() const;

	/** Damage one hit would do after the next upgrade, for showing the deal before it is taken. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	float GetDamageAtNextLevel() const;

	/** Whether the next level can be bought right now, and why not when it cannot. */
	bool CanUpgrade(FString& OutReason) const;

	/** The deal in one line: levels, cost, the public money it leaves and the damage it buys. */
	FString DescribeUpgrade() const;

	/** Buys the next level with public money. @return false, nothing spent, when it cannot. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Tower")
	bool Upgrade();

	/** Debug: sets the level outright, for nothing. Clamped to the valid range. */
	void DebugSetLevel(int32 NewLevel);

	/** Public money sunk into this defender's levels so far, at the prices they were bought at. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	int32 GetEvolutionSpent() const { return EvolutionSpent; }

	/** Puts a saved defender back at its level, with what its levels cost when they were bought. */
	void RestoreEvolution(int32 NewLevel, int32 Spent);

	/** Range actually used in combat, in centimetres: the level range in cells, times the cell size, times the platform multiplier when on one. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	float GetEffectiveRange() const;

	/** Range actually used in combat, in cells. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	float GetEffectiveRangeCells() const;

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	ABDEnemyBase* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	int32 GetShotsFired() const { return ShotsFired; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	int32 GetKills() const { return Kills; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	UStaticMeshComponent* GetMesh() const { return Mesh; }

	/** The part that turns: the weapon. The mesh hangs off it. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	USceneComponent* GetTurret() const { return Turret; }

	/** Seconds still to wait before the current target may be shot. 0 once acquired. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	float GetAcquisitionRemaining() const { return AcquisitionRemaining; }

	/** Whether the weapon is pointing at the current target within the aim tolerance. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	bool IsAligned() const { return bAligned; }

	/** Degrees between where the weapon points and where the target is. 0 with no target. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	float GetAimError() const;

	//~ Magazine, for the HUD. Ammunition is infinite: a magazine only paces the fire. ---

	/** Shots left before the next reload. Meaningless when the data has no magazine. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	int32 GetShotsLeftInMagazine() const { return ShotsInMagazine; }

	/** How far the reload has come, 0..1. 0 when not reloading. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	float GetReloadProgress() const;

	/** Seconds of reload still to wait. 0 when not reloading. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	float GetReloadRemaining() const { return ReloadRemaining; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	bool IsReloading() const { return ReloadRemaining > 0.0f; }

	/** Credited by the creep that died to a shot of this tower. Not meant to be called directly. */
	void NotifyKill() { ++Kills; }

	/** Where a projectile of this tower lands: applies the damage by the damage type of the data. Called by the projectile. */
	void ApplyHit(ABDEnemyBase* HitTarget, const FVector& HitLocation, float Damage);

	//~ Where it stands --------------------------------------------------------

	/** Platform this tower stands on, or null when it sits on a ground cell. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	UBDPlatformComponent* GetPlatform() const;

	/** Index of the occupied slot, or INDEX_NONE when the tower is on the ground. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	int32 GetPlatformSlotIndex() const { return PlatformSlotIndex; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	bool IsOnPlatform() const;

	/** Ground cell the tower occupies. Only meaningful while it is not on a platform. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	FBDCellCoord GetGroundCoord() const { return GroundCoord; }

	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Tower")
	void SetGroundCoord(const FBDCellCoord& Coord) { GroundCoord = Coord; }

	/** Called by UBDPlatformComponent when this tower takes a slot. Not meant to be called directly. */
	void NotifyOccupiedSlot(UBDPlatformComponent* Platform, int32 SlotIndex);

	/** Called by UBDPlatformComponent when this tower leaves a slot. Not meant to be called directly. */
	void NotifyReleasedSlot();

protected:
	/** Spawns and launches one projectile at the current target. Virtual so a Blueprint child can add a muzzle effect. */
	virtual void Fire(ABDEnemyBase* Target, const FBDTowerLevel& LevelStats);

private:
	UBDWaveSubsystem* GetWaves() const;

	/** Sets the mesh from the data and rests it on the root, whatever its pivot. */
	void ApplyMesh();

	/** Whether a creep is alive, still walking and inside the range. */
	bool IsValidTarget(const ABDEnemyBase* Enemy, float RangeSquared) const;

	/** Picks a creep in range by the priority of the data. Null when none is in range. */
	ABDEnemyBase* AcquireTarget(float RangeSquared) const;

	/** Turns the weapon towards the target by the turn rate of the data. @return true when aligned within the tolerance. */
	bool TurnTowards(const ABDEnemyBase* Target, float DeltaSeconds);

	/** Forgets the target and starts the acquisition over. */
	void DropTarget();

	/** Where shots leave from: the top of the mesh, or the weapon pivot when there is none. */
	FVector GetMuzzleLocation() const;

	void DrawDebug() const;

	/** Turns to follow the target. Yaw only: the board is flat. */
	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense|Tower")
	TObjectPtr<USceneComponent> Turret;

	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense|Tower")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Brazil Defense|Tower")
	TObjectPtr<const UBDTowerData> Data;

	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Brazil Defense|Tower")
	int32 Level = 1;

	/** See GetEvolutionSpent. Kept rather than recomputed: prices move with the waves. */
	UPROPERTY(Transient, VisibleInstanceOnly, Category = "Brazil Defense|Tower")
	int32 EvolutionSpent = 0;

	/** Weak: the creep dies on its own schedule. */
	TWeakObjectPtr<ABDEnemyBase> CurrentTarget;

	/** Seconds until the next shot may go out. */
	float FireCooldown = 0.0f;

	/** Seconds of recognition still owed on the current target. */
	float AcquisitionRemaining = 0.0f;

	/** Rounds left before a reload. Filled from the data at initialization and after every reload. */
	int32 ShotsInMagazine = 0;

	/** Seconds of reload left. Counts down whether or not a target is in sight. */
	float ReloadRemaining = 0.0f;

	bool bAligned = false;

	int32 ShotsFired = 0;
	int32 Kills = 0;

	/** Weak on purpose: the platform actor and the tower have independent lifetimes. */
	UPROPERTY(Transient)
	TWeakObjectPtr<UBDPlatformComponent> Platform;

	UPROPERTY(Transient)
	int32 PlatformSlotIndex = INDEX_NONE;

	UPROPERTY(Transient)
	FBDCellCoord GroundCoord;
};

// Brazil Defense. The Agent: the one man the palace sends out on patrol.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/BDGridTypes.h"
#include "BDAgent.generated.h"

class ABDEnemyBase;
class ABDPalace;
class UAnimSequenceBase;
class UBDGridSubsystem;
class UBDPalaceData;
class USkeletalMeshComponent;
struct FBDAgentWeapon;

/** What the Agent is doing. A kick is played over any of them and hands back to it. */
UENUM(BlueprintType)
enum class EBDAgentState : uint8
{
	/** Wandering: a few cells one way, now and then a stop. */
	Walking,
	Idle,

	/** Stopped, turned to a creep in reach, firing until none is left. */
	Shooting,

	/** Patrol time ran out: walking home to sleep, defending nothing. */
	Returning,

	/** Lying in front of the palace until the wave after the bar ran out is over. */
	Sleeping,
};

/**
 * The Agent ("o Mito"). Born with his palace, in front of it, and gone with it.
 *
 * He wanders the board cell to cell in a random direction, a few cells at a time, and
 * stops now and then. Nothing built is in his way - towers, platforms, the palace - and
 * neither are the creeps: only a divider on the edge he would cross turns him, and the
 * permanent scenery, like any creep. Movement is by hand, as the creeps', no collision.
 *
 * A creep inside his detection radius stops him on the spot: he turns to it and shoots,
 * one creep at a time, the nearest, until none is left in reach; then he walks on. He
 * never chases. Shots are instant hits with a tracer; damage and rate are the weapon of
 * the palace's level (UBDPalaceData::Weapons).
 *
 * His patrol time runs down always, waves or not. Each kill puts some back. At zero he
 * walks home, lies down in front of the palace and sleeps through the next wave, not
 * defending it, then gets up on a full bar.
 */
UCLASS(meta = (DisplayName = "BD Agent"))
class BRAZIL_DEFENSE_API ABDAgent : public AActor
{
	GENERATED_BODY()

public:
	ABDAgent();

	virtual void Tick(float DeltaSeconds) override;

	/** Called once by the palace that spawned him, before his first tick. */
	void InitializeAgent(ABDPalace* InPalace);

	ABDPalace* GetPalace() const { return Palace.Get(); }

	EBDAgentState GetState() const { return State; }

	/** Seconds of patrol left. Above PatrolTime when kills came faster than the clock. */
	float GetPatrolRemaining() const { return PatrolRemaining; }

	/** The bar as drawn: patrol left over a full bar, clamped to 0..1. */
	float GetPatrolFraction() const;

	bool IsAsleep() const { return State == EBDAgentState::Sleeping; }

	/** Whether he is playing a kick right now. */
	bool IsKicking() const { return KickRemaining > 0.0f; }

	/** The weapon in his hands: the palace level's entry. Null without data. */
	const FBDAgentWeapon* GetWeapon() const;

	/** Detection radius in centimetres, on the board plane. */
	float GetDetectionRadius() const;

	ABDEnemyBase* GetCurrentTarget() const { return CurrentTarget.Get(); }

	int32 GetShotsFired() const { return ShotsFired; }
	int32 GetKills() const { return Kills; }

	/** Patrol seconds the kills have put back so far. */
	float GetBonusEarned() const { return BonusEarned; }

	/** Where he lies down: the center of a free cell next to the palace, in front first. */
	FVector GetSleepSpot() const;

	/** Where the middle of his time bar floats: over his head. */
	FVector GetBarAnchor() const;

	/** Credited by a creep that died to his shot: puts the weapon's kill bonus on the bar. */
	void NotifyKill();

	/**
	 * Plays the kick once, wherever he is and whatever he does, then carries on. Nothing
	 * calls it in play yet: the ministers it is for come in a later slice.
	 * @return false when there is no kick animation or he is asleep.
	 */
	bool Kick();

	//~ Debug ---------------------------------------------------------------------

	/** Sets the patrol time left. 0 sends him home now. */
	void DebugSetPatrolRemaining(float Seconds);

	/** Wakes him on a full bar wherever he is. */
	void DebugWake();

	/** One line on his state, for the console and the regression. */
	FString Describe() const;

private:
	UBDGridSubsystem* GetGrid() const;
	const UBDPalaceData* GetData() const;

	/** Whether he may stand on a cell: on the grid and not permanent scenery. */
	bool CanStand(const FBDCellCoord& Coord) const;

	/** Whether he may step from one cell to its neighbour: both standable, no divider between. */
	bool CanStep(const FBDCellCoord& From, const FBDCellCoord& To) const;

	/**
	 * The cell he sleeps on: one beside the palace, the front first, then its right, its
	 * back and its left; a free cell before one with something built on it.
	 * @return false when none of them is on the board.
	 */
	bool FindSleepCell(FBDCellCoord& OutCell) const;

	/**
	 * The cells from one to another, crossing only edges he may step over; breadth first.
	 * Both ends included. Empty when there is no way.
	 */
	TArray<FBDCellCoord> FindWay(const FBDCellCoord& From, const FBDCellCoord& To) const;

	/** World point of a cell center on the board plane. */
	FVector CellPoint(const FBDCellCoord& Coord) const;

	/** Picks what to do next after a walk or a stop: another walk, or a stop first. */
	void DecideNext();

	/** Starts a walk of a few cells in a random open direction. Stops instead when boxed in. */
	void StartWalk();
	void StartIdle();

	/** Patrol over: the cells home, divider by divider, or lying down where he is when home cannot be reached. */
	void StartReturn();
	void StartSleep();
	void WakeUp();

	/** Walks the queued cells. @return true once the last one is reached. */
	bool StepAlong(float DeltaSeconds);

	/** Turns the actor towards a yaw by the turn rate. @return true when within the tolerance. */
	bool TurnTowards(float WantedYaw, float DeltaSeconds, float Tolerance);

	/** A creep alive, walking, not already doomed, and inside the radius. */
	bool IsValidTarget(const ABDEnemyBase* Enemy, float RadiusSquared) const;
	ABDEnemyBase* AcquireTarget(float RadiusSquared) const;

	void Fire(ABDEnemyBase* Target, const FBDAgentWeapon& Weapon);

	/** Loops an animation of the data, unless it is already the one playing. */
	void PlayLoop(const TSoftObjectPtr<UAnimSequenceBase>& Animation, float Rate = 1.0f);

	/** The loop of the current state, put back after a kick. */
	void PlayStateAnimation();

	FVector GetMuzzleLocation() const;

	void DrawDebug() const;

	UPROPERTY(VisibleAnywhere, Category = "Agent")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Agent")
	TObjectPtr<USkeletalMeshComponent> Body;

	/** Weak: the palace takes him down with it, but never the other way round. */
	TWeakObjectPtr<ABDPalace> Palace;

	EBDAgentState State = EBDAgentState::Idle;

	/** Cells still to walk, the next one first. Empty when standing. */
	TArray<FBDCellCoord> Route;

	/** Cell he stands on or is walking into. Where a new walk or the way home starts from. */
	FBDCellCoord HeadingCell;

	float IdleRemaining = 0.0f;
	float PatrolRemaining = 0.0f;
	float FireCooldown = 0.0f;
	float KickRemaining = 0.0f;

	/** Wave number when the bar ran out; he wakes once SleepWaves more have started and ended. */
	int32 WaveAtSleep = 0;

	TWeakObjectPtr<ABDEnemyBase> CurrentTarget;

	/** The loop playing, so asking for it again does not restart it. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> CurrentLoop;

	int32 ShotsFired = 0;
	int32 Kills = 0;
	float BonusEarned = 0.0f;
};

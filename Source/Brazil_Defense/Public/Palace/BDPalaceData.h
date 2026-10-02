// Brazil Defense. What the Palácio do Governo is: its look and how far it has evolved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BDPalaceData.generated.h"

class UAnimSequenceBase;
class USkeletalMesh;
class UStaticMesh;

/**
 * One weapon the Agent carries, picked by the palace's level: entry 0 is the pistol he
 * comes out with, each star up swaps it for the next. A stronger weapon kills more
 * easily, so each kill buys him less patrol time than the last weapon's did.
 */
USTRUCT(BlueprintType)
struct FBDAgentWeapon
{
	GENERATED_BODY()

	/** For the log and the designer: what this entry is. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName Name;

	/** Not designed yet: the entry holds the pistol's numbers until its weapon exists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool bPlaceholder = false;

	/** Health taken off a creep by one shot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float Damage = 10.0f;

	/** Shots per second. The pistol fires one at a time, slower than a machine gun. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.05"))
	float FireRate = 1.5f;

	/** Seconds of patrol a kill with this weapon puts back on the bar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float KillBonusSeconds = 1.0f;
};

/**
 * The palace: a 2x2 building the player buys with public money, where the Agent will
 * come out of. Evolving it is what gives the Agent better weapons - but the Agent is not
 * built yet, so for now the level is only shown: five stars over the roof, as many
 * filled as the level says.
 *
 * The footprint, the price and the cell it writes are on the placeable that builds it
 * (UBDPlaceableData::PalaceData points here); this asset is only the building itself.
 */
UCLASS(BlueprintType)
class BRAZIL_DEFENSE_API UBDPalaceData : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Stars on the indicator, and the highest level the palace reaches. */
	static constexpr int32 MaxLevel = 5;

	/**
	 * The building's look. Empty falls back to the engine cube: a placeholder block until
	 * the final model arrives. Swap it here and nothing else changes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/**
	 * Scale the mesh uniformly so its widest side spans the footprint, and stand it on the
	 * floor whatever its pivot. Off, the mesh is taken as authored, pivot on the ground.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look")
	bool bFitToFootprint = true;

	/** Share of the footprint the fitted mesh covers, so neighbours do not touch. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look", meta = (ClampMin = "0.1", ClampMax = "1.0", EditCondition = "bFitToFootprint"))
	float FootprintFill = 0.9f;

	/** Applied on top of the fit, per axis. The placeholder cube flattens itself into a block with Z below 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look")
	FVector MeshScale = FVector(1.0f, 1.0f, 0.6f);

	/**
	 * How far the palace has evolved: 0 is not at all, 5 is every star filled. Has no
	 * effect on play yet - the Agent who will profit from it does not exist - it only
	 * sets how many stars the indicator fills.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Evolution", meta = (ClampMin = "0", ClampMax = "5", UIMin = "0", UIMax = "5"))
	int32 Level = 0;

	/** How high over the roof the stars float. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Evolution", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float StarsLift = 80.0f;

	UBDPalaceData();

	//~ The Agent ---------------------------------------------------------------
	// The one man the palace sends out: he wanders the board, stops and shoots whatever
	// creep comes near, and when his patrol time runs out he walks home and sleeps a
	// wave in front of the door. See ABDAgent.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Look")
	TSoftObjectPtr<USkeletalMesh> AgentMesh;

	/** Uniform scale of the agent's mesh. The model is about four metres tall at 1, on cells of seven. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Look", meta = (ClampMin = "0.01"))
	float AgentMeshScale = 1.0f;

	/** Turns the model so it faces the actor's forward. The imported model faces +Y, hence -90. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Look")
	float AgentMeshYaw = -90.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Animation")
	TSoftObjectPtr<UAnimSequenceBase> WalkAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Animation")
	TSoftObjectPtr<UAnimSequenceBase> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Animation")
	TSoftObjectPtr<UAnimSequenceBase> ShootAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Animation")
	TSoftObjectPtr<UAnimSequenceBase> SleepAnimation;

	/** Played once by a kick. Nothing asks for one yet: the ministers it is for come later. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Animation")
	TSoftObjectPtr<UAnimSequenceBase> KickAnimation;

	/** Walking pace, in cells per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Patrol", meta = (ClampMin = "0.05"))
	float WalkSpeed = 0.5f;

	/** Play rate of the walk loop at WalkSpeed, so the feet keep up with the ground. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Patrol", meta = (ClampMin = "0.1"))
	float WalkAnimRate = 1.0f;

	/** How fast he turns to face where he walks or shoots, in degrees per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Patrol", meta = (ClampMin = "1.0"))
	float TurnRate = 540.0f;

	/** Cells walked in one go before deciding again, drawn between these two. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Patrol", meta = (ClampMin = "1"))
	int32 WalkCellsMin = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Patrol", meta = (ClampMin = "1"))
	int32 WalkCellsMax = 3;

	/** Chance, after each walk, that he stands still for a while before the next. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Patrol", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IdleChance = 0.4f;

	/** How long a stop lasts, drawn between these two. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Patrol", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float IdleTimeMin = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Patrol", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float IdleTimeMax = 3.5f;

	/**
	 * Seconds of patrol on a full bar. The time runs always, waves or not; at zero he
	 * walks home and sleeps. Kills put time back (FBDAgentWeapon::KillBonusSeconds),
	 * beyond this if they come fast enough: there is no hard ceiling.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Patrol", meta = (ClampMin = "1.0", ForceUnits = "s"))
	float PatrolTime = 120.0f;

	/** Waves he sleeps through once home: the next one to start after the bar ran out, and he does not defend it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Patrol", meta = (ClampMin = "1"))
	int32 SleepWaves = 1;

	/** How far he sees, in cells, around himself on the board plane. A creep inside stops him and is shot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Combat", meta = (ClampMin = "0.1"))
	float DetectionRadiusCells = 3.0f;

	/**
	 * Off: the creep nearest to him is the one shot. On: the one furthest along its route,
	 * as the towers choose. Either way, one at a time.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Combat")
	bool bTargetFurthestAlong = false;

	/** Degrees off the target he may still fire at: he turns first, then shoots. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Combat", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float AimTolerance = 12.0f;

	/**
	 * One weapon per palace level, 0 to MaxLevel: the level picks the entry. Only the
	 * pistol (entry 0) is designed; the others hold its numbers, marked placeholder, and
	 * only their kill bonus already steps down 0.15 s a level.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Combat", EditFixedSize)
	TArray<FBDAgentWeapon> Weapons;

	/** The weapon of a palace level, clamped to the authored entries. Null when there are none. */
	const FBDAgentWeapon* GetWeapon(int32 PalaceLevel) const;
};

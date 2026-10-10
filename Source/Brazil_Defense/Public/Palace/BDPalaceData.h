// Brazil Defense. What the Palácio do Governo is: its look and how far it has evolved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BDPalaceData.generated.h"

class UAnimSequenceBase;
class USkeletalMesh;
class USoundBase;
class UStaticMesh;

/**
 * How a weapon keeps its target. A weapon that fires shot by shot looks again before
 * every shot and takes whatever creep is now clearly nearer; one that fires continuously
 * (a laser, later) stays on the creep it started on, since holding it is what pays.
 */
UENUM(BlueprintType)
enum class EBDAimMode : uint8
{
	/** Re-picks the nearest before every shot, switching only for one clearly nearer. */
	Dynamic,

	/** Keeps the creep it took until it dies or leaves the reach. */
	Locked,
};

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

	/** The weapon's model, held on the body's hand socket. Empty takes the one below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Where the weapon sits against the hand socket, to fit the grip to the fingers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FTransform Grip;

	/** Socket of the weapon mesh at the tip of the barrel: the shot and its sound leave there. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName MuzzleSocket = TEXT("Muzzle");

	/** Health taken off a creep by one shot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float Damage = 10.0f;

	/** Dynamic for anything that fires shot by shot; Locked is for a continuous weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	EBDAimMode AimMode = EBDAimMode::Dynamic;

	/**
	 * Shots per second. The pistol fires one at a time, slower than a machine gun. With a
	 * shot notify on the shooting animation the loop is played at the rate that makes one
	 * gesture per shot, and the shot leaves on the gesture.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.05"))
	float FireRate = 1.5f;

	/** Seconds of patrol a kill with this weapon puts back on the bar. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float KillBonusSeconds = 1.0f;

	/**
	 * Played at every shot, through UBDShotSoundSubsystem. Empty fires in silence: the
	 * slot waits for a cue of two or three variations (see UBDShotSoundSettings for how
	 * to build one).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<USoundBase> FireSound;
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

	/** Socket of the body the weapon is held on. Made in the skeleton editor; the name must match. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Look")
	FName HandSocket = TEXT("hand_r");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Animation")
	TSoftObjectPtr<UAnimSequenceBase> WalkAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Animation")
	TSoftObjectPtr<UAnimSequenceBase> IdleAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Animation")
	TSoftObjectPtr<UAnimSequenceBase> ShootAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Animation")
	TSoftObjectPtr<UAnimSequenceBase> SleepAnimation;

	/**
	 * Centimetres the body rises while he sleeps. The sleeping loop lies him on his side and
	 * rolls him: with the feet on the floor, the arm under him went about 12 cm into it.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Animation", meta = (ClampMin = "0", Units = "cm"))
	float SleepLift = 14.0f;

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

	/**
	 * How far one walk goes, in cells, drawn anywhere between these two: straight to a
	 * point that far off, at any angle, before deciding again.
	 */
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

	/**
	 * Seconds he rests once lying down, before a single kill: the bar fills over the rest
	 * and he gets up on his own when it is full, mid wave or not. Each kill of the patrol
	 * just ended takes RestReductionPerKill off it, down to RestTimeMin: the better he
	 * did, the sooner he is back.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Rest", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RestTimeBase = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Rest", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RestReductionPerKill = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Rest", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float RestTimeMin = 10.0f;

	/** The rest after a patrol of this many kills. */
	float GetRestTime(int32 KillsOnPatrol) const;

	/** Multiplier on WalkSpeed while he runs after a candidate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Patrol", meta = (ClampMin = "0.1"))
	float ChaseSpeedScale = 1.5f;

	/**
	 * How far into his reach a chased candidate must be before he stops to shoot, as a
	 * share of the detection radius; once shooting he keeps at it while the candidate is
	 * anywhere in reach. The gap keeps him from stopping and starting on the edge of the
	 * radius without the gun ever going off.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Patrol", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ChaseCloseIn = 0.75f;

	/** How far he sees, in cells, around himself on the board plane. A creep inside stops him and is shot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Combat", meta = (ClampMin = "0.1"))
	float DetectionRadiusCells = 3.0f;

	/**
	 * Off: the creep nearest to him is the one shot. On: the one furthest along its route.
	 * Either way, one at a time, and a candidate in reach before anything else.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Combat")
	bool bTargetFurthestAlong = false;

	/**
	 * With a Dynamic weapon, how much nearer another creep must be before he switches to
	 * it: 0.15 is 15% nearer than the one he has. Keeps him from stuttering between two
	 * creeps at almost the same distance.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Agent|Combat", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float AimSwitchMargin = 0.15f;

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

	/** The model of a palace level: its own, or the nearest entry below with one. */
	TSoftObjectPtr<UStaticMesh> ResolveWeaponMesh(int32 PalaceLevel) const;
};

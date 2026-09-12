// Brazil Defense. Definition of one kind of defender and its levels.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BDTowerData.generated.h"

class ABDProjectileBase;
class ABDTowerBase;
class UStaticMesh;

/** Which creep a tower shoots at when several are in range. Only First is implemented today. */
UENUM(BlueprintType)
enum class EBDTargetPriority : uint8
{
	/** The creep furthest along its route: the one about to reach the urn. */
	First UMETA(DisplayName = "First"),
	/** The creep least far along its route. */
	Last UMETA(DisplayName = "Last"),
	/** The creep with the most health left. */
	Strongest UMETA(DisplayName = "Strongest"),
	/** The creep nearest to the tower. */
	Closest UMETA(DisplayName = "Closest")
};

/** How a hit lands. Single and Area are implemented; Pierce is stored and, for now, treated as Single. */
UENUM(BlueprintType)
enum class EBDDamageType : uint8
{
	/** The projectile hurts the creep it hits. */
	Single UMETA(DisplayName = "Single"),
	/** The projectile hurts every creep within AreaRadius of where it lands. */
	Area UMETA(DisplayName = "Area"),
	/** The projectile goes on through the creeps it hits. Not implemented yet. */
	Pierce UMETA(DisplayName = "Pierce")
};

/**
 * One level of a tower. Distances speak in cells and time in seconds, the same units
 * the creeps use, so the balance reads off the numbers: a tower of range 4 against a
 * creep at 1 cell/s keeps it under fire for 8 seconds.
 */
USTRUCT(BlueprintType)
struct FBDTowerLevel
{
	GENERATED_BODY()

	/** Health taken from the creep by one hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Damage = 10.0f;

	/** Shots per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float FireRate = 1.0f;

	/** Reach of the tower, in cells, before any platform bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Range = 4.0f;

	/** Projectile fired. Loaded on demand. Falls back to ABDProjectileBase itself when unset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level")
	TSoftClassPtr<ABDProjectileBase> ProjectileClass;

	/** How fast the projectile flies, in cells per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float ProjectileSpeed = 12.0f;
};

/**
 * One kind of defender: what it costs, when it unlocks, where it may stand, how it looks
 * and what each of its levels does. Upgrades are not wired yet; a defender starts at
 * level 0 of Levels.
 *
 * Two families share this asset and ABDTowerBase. A tower is ground equipment, a tripod
 * gun or a fixed cannon, and stands on a grid cell. A character is a human shooter, an
 * officer with a .38 or a sniper, and stands on a platform slot. The two flags below say
 * which is which; every asset is expected to be one or the other, never both, even if
 * the flags allow it for something that turns up later.
 */
UCLASS(BlueprintType, meta = (DisplayName = "BD Tower"))
class BRAZIL_DEFENSE_API UBDTowerData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Asset type used to discover every tower through the asset manager. */
	static const FPrimaryAssetType TowerAssetType;

	/** Upper bound on Levels, so a tree of upgrades stays a short list a designer can read. */
	static constexpr int32 MaxLevels = 10;

	//~ Begin UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UPrimaryDataAsset interface

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tower")
	FText DisplayName;

	/** Actor spawned for this tower. Loaded on demand. Falls back to ABDTowerBase itself when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tower")
	TSoftClassPtr<ABDTowerBase> TowerClass;

	/** Votes spent to build one. Stored only: nothing spends votes yet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tower", meta = (ClampMin = "0", UIMin = "0"))
	int32 BuildCost = 0;

	/** First wave on which this tower may be built. Stored only: nothing checks it yet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tower", meta = (ClampMin = "0", UIMin = "0"))
	int32 UnlockWave = 0;

	//~ Where it may stand ----------------------------------------------------

	/** A tower: ground equipment, placed on a grid cell. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement")
	bool bCanPlaceOnGround = true;

	/** A character: a human shooter, placed on a platform slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placement")
	bool bCanPlaceOnSlot = false;

	//~ Behaviour, the same at every level ------------------------------------

	/** Which creep to shoot when several are in range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour")
	EBDTargetPriority TargetPriority = EBDTargetPriority::First;

	/** Degrees per second the weapon turns towards its target. 0 snaps instantly. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TurnRate = 360.0f;

	/** How far off the target, in degrees, the weapon may still fire. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "180.0"))
	float AimTolerance = 5.0f;

	/** Seconds between seeing a new target and being allowed to shoot it: recognition. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float AcquisitionDelay = 0.3f;

	/**
	 * Shots fired at FireRate before the tower stops to reload. 0 means no reloading:
	 * continuous fire. This is what gives towers on the same slot different characters:
	 * a revolver empties six fast and reloads short, a sniper fires one and reloads long.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0", UIMin = "0"))
	int32 MagazineSize = 0;

	/** Seconds the tower stops for once the magazine is empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s", EditCondition = "MagazineSize > 0"))
	float ReloadTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour")
	EBDDamageType DamageType = EBDDamageType::Single;

	/** Radius of the burst, in cells. Area only. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Behaviour", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "DamageType == EBDDamageType::Area"))
	float AreaRadius = 1.0f;

	/** The levels, in order. At least one; at most MaxLevels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Levels")
	TArray<FBDTowerLevel> Levels;

	/** Mesh handed to the actor at spawn when its class brings none, so a plain ABDTowerBase can stand in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", meta = (EditCondition = "Mesh != nullptr"))
	FVector MeshScale = FVector::OneVector;

	/** The level at an index, clamped into Levels. Null when there are no levels at all. */
	const FBDTowerLevel* GetLevel(int32 Level) const;
};

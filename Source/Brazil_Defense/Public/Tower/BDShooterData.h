// Brazil Defense. A platform shooter: the man, his animations, and the weapon of each level.

#pragma once

#include "CoreMinimal.h"
#include "Tower/BDTowerData.h"
#include "BDShooterData.generated.h"

class UAnimSequenceBase;
class UNiagaraSystem;
class USkeletalMesh;
class USoundBase;
class UStaticMesh;

/**
 * One weapon a shooter holds, picked by his level: entry 0 is the one he is built with,
 * each star up swaps it for the next. The weapon is its own mesh, put in his hand on the
 * body's hand socket; its muzzle is a socket of the weapon mesh.
 *
 * Every slot may be left empty. An empty mesh, flash or sound is taken from the nearest
 * entry below that has one, so the pistol stays in his hand, flashing and sounding like a
 * pistol, until the next weapon is made. A Damage or FireRate of 0 follows the level ladder
 * of the tower data (Levels and the upgrade formula), which is what the balance is built on.
 */
USTRUCT(BlueprintType)
struct FBDShooterWeapon
{
	GENERATED_BODY()

	/** For the log and the designer: what this entry is. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName Name;

	/** Not designed yet: the entry holds the weapon below it until its own exists. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool bPlaceholder = false;

	/** The weapon's model, held on the body's hand socket. Empty takes the one below. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Where the weapon sits against the hand socket, to fit the grip to the fingers. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FTransform Grip;

	/** Socket of the weapon mesh at the tip of the barrel: the flash and the shot leave there. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName MuzzleSocket = TEXT("Muzzle");

	/** Health taken off a creep by one hit. 0 follows the level ladder of the data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float Damage = 0.0f;

	/** Shots per second. 0 follows the level ladder of the data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.0"))
	float FireRate = 0.0f;

	/** Played on the muzzle socket at every shot. Empty takes the one below; none at all flashes nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<UNiagaraSystem> MuzzleFlash;

	/**
	 * Played at every shot through UBDShotSoundSubsystem: 3D at the muzzle, the effects
	 * class, the shots' budget. A cue of four variations under a Random node (see
	 * UBDShotSoundSettings for the commandlet that builds one). Empty takes the one below;
	 * none at all fires in silence.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<USoundBase> FireSound;
};

/**
 * A character of the platforms with a body of his own: a skinned man standing on the
 * slot, idle while nothing is in reach, a gesture per shot when there is (ABDShooter).
 * Everything the tower data says still holds - cost, range, levels, magazine, aim - and
 * the weapons below say what he holds at each level.
 *
 * Every look slot can stay empty: without a body the tower data's placeholder mesh
 * stands in, without animations the body holds its pose, without a hand socket the
 * weapon hangs at the body's root. Fill them as the art arrives; nothing else changes.
 */
UCLASS(BlueprintType, meta = (DisplayName = "BD Shooter"))
class BRAZIL_DEFENSE_API UBDShooterData : public UBDTowerData
{
	GENERATED_BODY()

public:
	UBDShooterData();

	//~ The man -----------------------------------------------------------------

	/** The shooter's body. Empty keeps the tower data's placeholder mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Look")
	TSoftObjectPtr<USkeletalMesh> BodyMesh;

	/** Uniform scale of the body. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Look", meta = (ClampMin = "0.01"))
	float BodyScale = 1.0f;

	/** Turns the model so it faces the way he aims (local +X). A model facing +Y needs -90. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Look")
	float BodyYaw = -90.0f;

	/** Socket of the body the weapon is held on. Made in the skeleton editor; the name must match. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Look")
	FName HandSocket = TEXT("hand_r");

	/** Looped while nothing is in reach. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Animation")
	TSoftObjectPtr<UAnimSequenceBase> IdleAnimation;

	/**
	 * One gesture per shot, played from the start at every shot and sped up to fit the
	 * weapon's rate. With a BD Shot notify on it, the shot leaves on that frame - flash,
	 * sound and projectile together; without one, at the start of the gesture.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Animation")
	TSoftObjectPtr<UAnimSequenceBase> ShootAnimation;

	/** Seconds without a shot before he goes back to the idle loop. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Animation", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float IdleAfter = 0.6f;

	//~ What he holds -------------------------------------------------------------

	/**
	 * One weapon per level, 1 to MaxLevels: the level picks the entry, so a star bought
	 * swaps the weapon in his hand. Only entry 0 is designed; the others are placeholders
	 * holding the one below.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapons", EditFixedSize)
	TArray<FBDShooterWeapon> Weapons;

	/** Played where a shot of his reaches a creep. Empty plays nothing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Weapons")
	TSoftObjectPtr<UNiagaraSystem> ImpactEffect;

	/** The weapon entry of a level (1 = entry 0), clamped to the entries. Null when there are none. */
	const FBDShooterWeapon* GetWeapon(int32 Level) const;

	/** The mesh, the flash and the sound of a level: its own, or the nearest entry below with one. */
	TSoftObjectPtr<UStaticMesh> ResolveWeaponMesh(int32 Level) const;
	TSoftObjectPtr<UNiagaraSystem> ResolveMuzzleFlash(int32 Level) const;
	TSoftObjectPtr<USoundBase> ResolveFireSound(int32 Level) const;
};

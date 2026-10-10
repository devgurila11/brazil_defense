// Brazil Defense. A platform shooter with a body: the man, the weapon in his hand, the shot.

#pragma once

#include "CoreMinimal.h"
#include "Tower/BDTowerBase.h"
#include "BDShooter.generated.h"

class UAnimSequenceBase;
class UBDShooterData;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * The defender of the platforms once it has a body. It aims, picks and paces its fire
 * exactly as ABDTowerBase does - the creep furthest ahead on its own stretch, looked at
 * again before every shot - and adds what a man shows doing it:
 *
 *  - a skinned body on the weapon pivot, so the whole man turns to aim; idle while
 *    nothing is in reach, one shooting gesture per shot;
 *  - the weapon as a mesh of its own on the body's hand socket, swapped for the weapon of
 *    the level whenever the level changes (UBDShooterData::Weapons);
 *  - at every shot, the weapon's muzzle flash on its muzzle socket and its sound, and
 *    where the shot lands, the impact effect.
 *
 * With a BD Shot notify on the shooting animation the shot leaves on that frame of the
 * gesture, so the flash, the sound and the projectile come out with the kick of the arm;
 * the gesture never outlasts the weapon's rate. Without one, the shot leaves as the
 * gesture starts. Every slot of the data may be empty: the tower data's placeholder mesh
 * stands in for the body, and an empty flash or sound is simply not played.
 */
UCLASS(meta = (DisplayName = "BD Shooter"))
class BRAZIL_DEFENSE_API ABDShooter : public ABDTowerBase
{
	GENERATED_BODY()

public:
	ABDShooter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void InitializeTower(const UBDTowerData* InData) override;

	/** A kill of his: now and then he says a line of his own (UBDCreepSoundSubsystem::TryCelebrate). */
	virtual void NotifyKill() override;

	/** The shooter's own data, or null when the tower data is a plain tower's. */
	const UBDShooterData* GetShooterData() const;

	/** Called by UBDAnimNotify_Shot on the frame the gun goes off. */
	void OnShotFrame();

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Shooter")
	USkeletalMeshComponent* GetBody() const { return Body; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Shooter")
	UStaticMeshComponent* GetWeapon() const { return Weapon; }

	/** Whether the body wears a mesh of the data, as opposed to the placeholder standing in. */
	bool HasBody() const;

	/** The level whose weapon is in his hand now. */
	int32 GetWeaponLevel() const { return WeaponLevel; }

	/** What holds the weapon: the hand socket's name when it is found on the body, None otherwise. */
	FName GetWeaponAttachSocket() const;

	/** Effects asked for and started, for the regression: a flash and a sound per shot, an impact per hit. */
	int32 GetFlashesRequested() const { return FlashesRequested; }
	int32 GetFlashesPlayed() const { return FlashesPlayed; }
	int32 GetImpactsRequested() const { return ImpactsRequested; }
	int32 GetImpactsPlayed() const { return ImpactsPlayed; }
	int32 GetGesturesPlayed() const { return GesturesPlayed; }

	/** Whether a gesture is under way with its shot still to leave on the notify. */
	bool IsShotPending() const { return bShotPending; }

protected:
	//~ Begin ABDTowerBase interface
	virtual void Fire(ABDEnemyBase* Target, const FBDTowerLevel& LevelStats) override;
	virtual void BeginShot(ABDEnemyBase* Target, const FBDTowerLevel& LevelStats) override;
	virtual float GetDamageAtLevel(int32 AtLevel) const override;
	virtual float GetFireRateAtLevel(int32 AtLevel) const override;
	virtual FVector GetMuzzleLocation() const override;
	virtual void OnShotLanded(ABDEnemyBase* HitTarget, const FVector& HitLocation) override;
	//~ End ABDTowerBase interface

private:
	/** Puts the body of the data on, or leaves the placeholder when there is none. */
	void ApplyBody();

	/** Puts the weapon of the current level in his hand, on the hand socket. */
	void ApplyWeapon();

	/** Starts the shooting gesture from the top. @return its length in seconds at the rate played, 0 when none was played. */
	float PlayGesture();

	/** Back to the idle loop, unless it is already playing. */
	void PlayIdle();

	/** Fires the shot the gesture was holding, at whatever creep is right to shoot now. */
	void CommitPendingShot();

	/** Whether the shot waits for the notify of the gesture: a body, and the notify on the animation. */
	bool UsesShotNotify() const;

	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense|Shooter")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense|Shooter")
	TObjectPtr<UStaticMeshComponent> Weapon;

	/** The loop or the gesture playing, so asking for the idle again does not restart it. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> CurrentAnimation;

	/** Level whose weapon is in hand; INDEX_NONE before the first. */
	int32 WeaponLevel = INDEX_NONE;

	/** A gesture is playing and its shot leaves on the notify, or when this runs out. */
	bool bShotPending = false;
	float PendingRemaining = 0.0f;

	/** Seconds since the last gesture started, to go back to idle. */
	float SinceGesture = TNumericLimits<float>::Max();

	/** Length of the last gesture at the rate it was played. */
	float GestureSeconds = 0.0f;

	int32 FlashesRequested = 0;
	int32 FlashesPlayed = 0;
	int32 ImpactsRequested = 0;
	int32 ImpactsPlayed = 0;
	int32 GesturesPlayed = 0;
};

// Brazil Defense. A shot on its way from a tower to a creep.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BDProjectileBase.generated.h"

class ABDEnemyBase;
class ABDTowerBase;
class UStaticMeshComponent;

/**
 * A projectile that flies to its target and hits it. No physics and no collision on
 * purpose: it is interpolated towards the creep every tick and the hit is judged by
 * distance, because at 4x game speed a physical body crosses the creep between two
 * frames and never touches it.
 *
 * A target that dies in flight is not chased into the void: the shot carries on to the
 * last place it saw the creep, and vanishes there doing nothing.
 */
UCLASS(Blueprintable, meta = (DisplayName = "BD Projectile Base"))
class BRAZIL_DEFENSE_API ABDProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	ABDProjectileBase();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Sends the projectile off. Called once by the tower right after spawning.
	 * @param Speed centimetres per second; the tower has already converted from cells.
	 */
	void Launch(ABDTowerBase* InShooter, ABDEnemyBase* InTarget, float InDamage, float Speed);

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	ABDEnemyBase* GetTarget() const { return Target.Get(); }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Tower")
	UStaticMeshComponent* GetMesh() const { return Mesh; }

protected:
	/** The shot arrived on a living target. Virtual so a Blueprint child can add an effect before the base applies the damage. */
	virtual void Hit(ABDEnemyBase* HitTarget);

private:
	/** Where the shot is heading: the body of the target while it lives, then wherever it was last seen. */
	FVector GetAimPoint() const;

	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense|Tower")
	TObjectPtr<UStaticMeshComponent> Mesh;

	TWeakObjectPtr<ABDTowerBase> Shooter;
	TWeakObjectPtr<ABDEnemyBase> Target;

	/** Last aim point taken from a living target, followed once it is gone. */
	FVector LastKnownAimPoint = FVector::ZeroVector;

	float Damage = 0.0f;
	float SpeedCm = 0.0f;
	float Age = 0.0f;
	bool bLaunched = false;
};

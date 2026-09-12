// Brazil Defense. A shot on its way from a tower to a creep.

#include "Tower/BDProjectileBase.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemy/BDEnemyBase.h"
#include "Engine/StaticMesh.h"
#include "Tower/BDTowerBase.h"
#include "Tower/BDTowerSettings.h"
#include "UObject/ConstructorHelpers.h"

namespace BDProjectilePrivate
{
	/** Placeholder look of the base class, so a shot is visible before any projectile Blueprint exists. */
	static const TCHAR* const DefaultMeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	static constexpr float DefaultMeshScale = 0.2f;
}

ABDProjectileBase::ABDProjectileBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	// See the class comment: the hit is a distance, never a contact.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetRelativeScale3D(FVector(BDProjectilePrivate::DefaultMeshScale));

	// Engine content, so it is always there. A Blueprint child replaces it.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(BDProjectilePrivate::DefaultMeshPath);
	if (DefaultMesh.Succeeded())
	{
		Mesh->SetStaticMesh(DefaultMesh.Object);
	}

	SetCanBeDamaged(false);
}

void ABDProjectileBase::Launch(ABDTowerBase* InShooter, ABDEnemyBase* InTarget, const float InDamage, const float Speed)
{
	Shooter = InShooter;
	Target = InTarget;
	Damage = InDamage;
	SpeedCm = Speed;
	Age = 0.0f;
	bLaunched = true;

	LastKnownAimPoint = GetAimPoint();
	SetActorRotation((LastKnownAimPoint - GetActorLocation()).GetSafeNormal().ToOrientationRotator());
}

FVector ABDProjectileBase::GetAimPoint() const
{
	const ABDEnemyBase* Enemy = Target.Get();
	if (Enemy == nullptr)
	{
		return LastKnownAimPoint;
	}

	// The middle of the body, not the feet: the root of a creep is on the floor.
	const UStaticMeshComponent* EnemyMesh = Enemy->GetMesh();
	return EnemyMesh != nullptr && EnemyMesh->GetStaticMesh() != nullptr
		? EnemyMesh->Bounds.Origin
		: Enemy->GetActorLocation();
}

void ABDProjectileBase::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bLaunched)
	{
		return;
	}

	const UBDTowerSettings& Settings = UBDTowerSettings::Get();

	Age += DeltaSeconds;
	if (Age > Settings.MaxLifetime)
	{
		Destroy();
		return;
	}

	// A living target keeps updating the aim; a dead one leaves the last point behind.
	ABDEnemyBase* Enemy = Target.Get();
	if (Enemy != nullptr)
	{
		LastKnownAimPoint = GetAimPoint();
	}

	const FVector Location = GetActorLocation();
	const FVector ToAim = LastKnownAimPoint - Location;
	const float Distance = ToAim.Size();
	const float Step = SpeedCm * DeltaSeconds;

	if (Distance <= FMath::Max(Step, Settings.HitDistance))
	{
		// Arrived. Only a target still alive takes the hit; the void takes nothing.
		SetActorLocation(LastKnownAimPoint);
		if (Enemy != nullptr)
		{
			Hit(Enemy);
		}
		Destroy();
		return;
	}

	const FVector Direction = ToAim / Distance;
	SetActorLocationAndRotation(Location + Direction * Step, Direction.ToOrientationRotator());
}

void ABDProjectileBase::Hit(ABDEnemyBase* HitTarget)
{
	// The tower decides how the hit lands (single creep, area); a shot whose tower is
	// gone still hurts the one creep it reached.
	if (ABDTowerBase* Tower = Shooter.Get())
	{
		Tower->ApplyHit(HitTarget, GetActorLocation(), Damage);
	}
	else
	{
		HitTarget->ApplyDamage(Damage, nullptr);
	}
}

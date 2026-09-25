// Brazil Defense. A shot on its way from a tower to a creep.

#include "Tower/BDProjectileBase.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemy/BDEnemyBase.h"
#include "Engine/StaticMesh.h"
#include "Tower/BDTowerBase.h"
#include "Tower/BDTowerSettings.h"
#include "Wave/BDWaveSubsystem.h"
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

	// Booked at launch, so the next tower to look at this creep sees the shot coming.
	if (InTarget != nullptr)
	{
		InTarget->AddIncomingDamage(Damage);
		bIncomingBooked = true;
	}
}

void ABDProjectileBase::ReleaseIncoming()
{
	if (!bIncomingBooked)
	{
		return;
	}

	bIncomingBooked = false;
	if (ABDEnemyBase* Enemy = Target.Get())
	{
		Enemy->RemoveIncomingDamage(Damage);
	}
}

void ABDProjectileBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Covers every way a shot can end without reaching its Tick again.
	ReleaseIncoming();
	Super::EndPlay(EndPlayReason);
}

FVector ABDProjectileBase::GetAimPoint() const
{
	const ABDEnemyBase* Enemy = Target.Get();
	if (Enemy == nullptr)
	{
		return LastKnownAimPoint;
	}

	// The middle of the body, not the feet: the root of a creep is on the floor.
	const UPrimitiveComponent* Body = Enemy->GetBody();
	return Body != nullptr ? Body->Bounds.Origin : Enemy->GetActorLocation();
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
		ReportLost();
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
		// Arrived. Only a target still alive takes the hit; the void takes nothing, and
		// the shot is counted as wasted.
		SetActorLocation(LastKnownAimPoint);
		ReleaseIncoming();
		if (Enemy != nullptr)
		{
			Hit(Enemy);
		}
		else
		{
			ReportLost();
		}
		Destroy();
		return;
	}

	const FVector Direction = ToAim / Distance;
	SetActorLocationAndRotation(Location + Direction * Step, Direction.ToOrientationRotator());
}

void ABDProjectileBase::ReportLost()
{
	const UWorld* World = GetWorld();
	if (UBDWaveSubsystem* Waves = World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr)
	{
		Waves->ReportWastedDamage(Damage, /*bLostShot*/ true);
	}
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

// Brazil Defense. The single defender class, wherever it stands.

#include "Tower/BDTowerBase.h"

#include "BDLog.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Enemy/BDEnemyBase.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Grid/BDGridDebug.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Platform/BDPlatformComponent.h"
#include "Tower/BDProjectileBase.h"
#include "Tower/BDTowerData.h"
#include "Tower/BDTowerSettings.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDTowerPrivate
{
	static int32 GShowRange = 0;
	static int32 GShowTarget = 0;

	static FAutoConsoleVariableRef CVarShowRange(
		TEXT("BD.Tower.ShowRange"),
		GShowRange,
		TEXT("1 draws the effective range of every tower as a circle on the ground. 0 to hide."),
		ECVF_Cheat);

	static FAutoConsoleVariableRef CVarShowTarget(
		TEXT("BD.Tower.ShowTarget"),
		GShowTarget,
		TEXT("1 draws a line from every tower to the creep it is tracking. 0 to hide."),
		ECVF_Cheat);
}

ABDTowerBase::ABDTowerBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// The weapon turns, the actor does not: a tower on a slot keeps the facing its slot
	// gave it, and still tracks.
	Turret = CreateDefaultSubobject<USceneComponent>(TEXT("Turret"));
	Turret->SetupAttachment(RootComponent);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Turret);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_WorldDynamic);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);

	SetCanBeDamaged(false);
}

UBDWaveSubsystem* ABDTowerBase::GetWaves() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr;
}

void ABDTowerBase::InitializeTower(const UBDTowerData* InData)
{
	Data = InData;
	Level = 0;
	FireCooldown = 0.0f;
	ReloadRemaining = 0.0f;
	ShotsInMagazine = Data != nullptr ? Data->MagazineSize : 0;
	DropTarget();
	ApplyMesh();

	if (Data == nullptr || Data->Levels.Num() == 0)
	{
		UE_LOG(LogBDTower, Error, TEXT("%s initialized with %s: it will stand there and never fire."),
			*GetName(), Data == nullptr ? TEXT("no data") : TEXT("data that has no levels"));
	}
}

void ABDTowerBase::ApplyMesh()
{
	if (Data == nullptr || Data->Mesh.IsNull())
	{
		// A Blueprint child brings its own mesh; leave it where the designer put it.
		return;
	}

	UStaticMesh* LoadedMesh = Data->Mesh.LoadSynchronous();
	if (LoadedMesh == nullptr)
	{
		UE_LOG(LogBDTower, Error, TEXT("%s: mesh %s of %s failed to load."),
			*GetName(), *Data->Mesh.ToString(), *Data->GetName());
		return;
	}

	Mesh->SetStaticMesh(LoadedMesh);
	Mesh->SetRelativeScale3D(Data->MeshScale);

	// Rest the mesh on the root, so the root is the floor contact whatever the pivot of the asset is.
	const FBox Bounds = LoadedMesh->GetBoundingBox();
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -Bounds.Min.Z * Data->MeshScale.Z));
}

float ABDTowerBase::GetReloadProgress() const
{
	if (!IsReloading() || Data == nullptr || Data->ReloadTime <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(1.0f - ReloadRemaining / Data->ReloadTime, 0.0f, 1.0f);
}

const FBDTowerLevel* ABDTowerBase::GetCurrentLevel() const
{
	return Data != nullptr ? Data->GetLevel(Level) : nullptr;
}

float ABDTowerBase::GetEffectiveRangeCells() const
{
	const FBDTowerLevel* LevelStats = GetCurrentLevel();
	if (LevelStats == nullptr)
	{
		return 0.0f;
	}

	const UBDPlatformComponent* CurrentPlatform = Platform.Get();
	return CurrentPlatform != nullptr ? LevelStats->Range * CurrentPlatform->RangeMultiplier : LevelStats->Range;
}

float ABDTowerBase::GetEffectiveRange() const
{
	const UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
	return Grid != nullptr ? GetEffectiveRangeCells() * Grid->GetCellSize() : 0.0f;
}

//~ Where it stands --------------------------------------------------------------

UBDPlatformComponent* ABDTowerBase::GetPlatform() const
{
	return Platform.Get();
}

bool ABDTowerBase::IsOnPlatform() const
{
	return Platform.IsValid() && PlatformSlotIndex != INDEX_NONE;
}

void ABDTowerBase::NotifyOccupiedSlot(UBDPlatformComponent* InPlatform, const int32 SlotIndex)
{
	Platform = InPlatform;
	PlatformSlotIndex = SlotIndex;
}

void ABDTowerBase::NotifyReleasedSlot()
{
	Platform.Reset();
	PlatformSlotIndex = INDEX_NONE;
}

//~ Combat -----------------------------------------------------------------------

bool ABDTowerBase::IsValidTarget(const ABDEnemyBase* Enemy, const float RangeSquared) const
{
	if (Enemy == nullptr || !IsValid(Enemy) || Enemy->HasArrived())
	{
		return false;
	}

	// Range is measured on the board plane: a creep is never above or below a tower in
	// any way that should matter to whether it can be shot.
	return FVector::DistSquared2D(Enemy->GetActorLocation(), GetActorLocation()) <= RangeSquared;
}

ABDEnemyBase* ABDTowerBase::AcquireTarget(const float RangeSquared) const
{
	const UBDWaveSubsystem* Waves = GetWaves();
	if (Waves == nullptr)
	{
		return nullptr;
	}

	// Only First today: the creep furthest along its route is the one about to score,
	// so it is the one worth the shot. The other priorities fall back to it until they
	// are written, rather than silently doing nothing.
	ABDEnemyBase* Best = nullptr;
	int32 BestProgress = -1;

	for (ABDEnemyBase* Enemy : Waves->GetLivingEnemiesRef())
	{
		if (!IsValidTarget(Enemy, RangeSquared))
		{
			continue;
		}

		const int32 Progress = Enemy->GetCurrentPathIndex();
		if (Progress > BestProgress)
		{
			Best = Enemy;
			BestProgress = Progress;
		}
	}

	return Best;
}

void ABDTowerBase::DropTarget()
{
	CurrentTarget.Reset();
	AcquisitionRemaining = 0.0f;
	bAligned = false;
}

bool ABDTowerBase::TurnTowards(const ABDEnemyBase* Target, const float DeltaSeconds)
{
	const FVector ToTarget = Target->GetActorLocation() - Turret->GetComponentLocation();
	if (ToTarget.IsNearlyZero())
	{
		return true;
	}

	// Yaw only, in world space: the turret is under the actor, whose own yaw is whatever
	// the slot or the placement gave it, so the world rotation is what has to match.
	const float WantedYaw = ToTarget.GetSafeNormal2D().ToOrientationRotator().Yaw;
	const float CurrentYaw = Turret->GetComponentRotation().Yaw;
	const float TurnRate = Data != nullptr ? Data->TurnRate : 0.0f;

	const float NewYaw = TurnRate > 0.0f
		? FMath::FixedTurn(CurrentYaw, WantedYaw, TurnRate * DeltaSeconds)
		: WantedYaw;

	Turret->SetWorldRotation(FRotator(0.0f, NewYaw, 0.0f));

	const float Tolerance = Data != nullptr ? Data->AimTolerance : 0.0f;
	return FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, WantedYaw)) <= Tolerance;
}

void ABDTowerBase::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// DeltaSeconds already carries the global time dilation: 4x means four times the
	// cooldown, the turn and the recognition per real second, with nothing to do here.
	FireCooldown = FMath::Max(0.0f, FireCooldown - DeltaSeconds);

	// A reload runs its course whatever the creeps do; the magazine is full again at the end.
	if (ReloadRemaining > 0.0f)
	{
		ReloadRemaining = FMath::Max(0.0f, ReloadRemaining - DeltaSeconds);
		if (ReloadRemaining <= 0.0f && Data != nullptr)
		{
			ShotsInMagazine = Data->MagazineSize;
		}
	}

	const FBDTowerLevel* LevelStats = GetCurrentLevel();
	if (LevelStats == nullptr)
	{
		return;
	}

	const float Range = GetEffectiveRange();
	const float RangeSquared = Range * Range;

	// 1. Hold the target while it stays in range and alive; otherwise look for one and
	//    start the recognition over.
	ABDEnemyBase* Target = CurrentTarget.Get();
	if (!IsValidTarget(Target, RangeSquared))
	{
		DropTarget();
		Target = AcquireTarget(RangeSquared);
		if (Target != nullptr)
		{
			CurrentTarget = Target;
			AcquisitionRemaining = Data->AcquisitionDelay;
		}
	}

	DrawDebug();

	if (Target == nullptr)
	{
		return;
	}

	// 2. Recognition: seeing is not yet shooting.
	if (AcquisitionRemaining > 0.0f)
	{
		AcquisitionRemaining = FMath::Max(0.0f, AcquisitionRemaining - DeltaSeconds);
		return;
	}

	// 3. Align the weapon. 4. Only then fire, at the rate of the level, while the
	//    magazine lasts. The weapon keeps tracking through a reload.
	bAligned = TurnTowards(Target, DeltaSeconds);
	if (bAligned && FireCooldown <= 0.0f && !IsReloading())
	{
		Fire(Target, *LevelStats);
		FireCooldown = 1.0f / FMath::Max(LevelStats->FireRate, KINDA_SMALL_NUMBER);

		if (Data->MagazineSize > 0 && --ShotsInMagazine <= 0)
		{
			ReloadRemaining = Data->ReloadTime;
			if (ReloadRemaining <= 0.0f)
			{
				// A magazine with no reload time is just a counter: fill it and carry on.
				ShotsInMagazine = Data->MagazineSize;
			}
		}
	}
}

FVector ABDTowerBase::GetMuzzleLocation() const
{
	if (Mesh != nullptr && Mesh->GetStaticMesh() != nullptr)
	{
		const FBoxSphereBounds Bounds = Mesh->Bounds;
		return FVector(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z + Bounds.BoxExtent.Z);
	}

	return Turret->GetComponentLocation();
}

void ABDTowerBase::Fire(ABDEnemyBase* Target, const FBDTowerLevel& LevelStats)
{
	UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
	if (World == nullptr || Grid == nullptr)
	{
		return;
	}

	UClass* ProjectileClass = LevelStats.ProjectileClass.IsNull()
		? ABDProjectileBase::StaticClass()
		: LevelStats.ProjectileClass.LoadSynchronous();
	if (ProjectileClass == nullptr)
	{
		UE_LOG(LogBDTower, Error, TEXT("%s: projectile class %s failed to load."),
			*GetName(), *LevelStats.ProjectileClass.ToString());
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = this;

	ABDProjectileBase* Projectile = World->SpawnActor<ABDProjectileBase>(
		ProjectileClass, GetMuzzleLocation(), Turret->GetComponentRotation(), SpawnParams);
	if (Projectile == nullptr)
	{
		return;
	}

	// Cells per second on the data, centimetres per second in the world.
	Projectile->Launch(this, Target, LevelStats.Damage, LevelStats.ProjectileSpeed * Grid->GetCellSize());
	++ShotsFired;

	UE_LOG(LogBDTower, Verbose, TEXT("%s fired shot %d at %s."), *GetName(), ShotsFired, *Target->GetName());
}

void ABDTowerBase::ApplyHit(ABDEnemyBase* HitTarget, const FVector& HitLocation, const float Damage)
{
	const EBDDamageType DamageType = Data != nullptr ? Data->DamageType : EBDDamageType::Single;

	if (DamageType == EBDDamageType::Area)
	{
		// Everyone within the burst, the target included, whether or not it is still alive.
		const UWorld* World = GetWorld();
		const UBDGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
		const UBDWaveSubsystem* Waves = GetWaves();
		if (Grid == nullptr || Waves == nullptr)
		{
			return;
		}

		const float Radius = Data->AreaRadius * Grid->GetCellSize();
		const float RadiusSquared = Radius * Radius;

		// Copied: a hit can kill, and a kill drops the creep from the live list.
		TArray<ABDEnemyBase*> Enemies;
		Waves->GetLivingEnemies(Enemies);
		for (ABDEnemyBase* Enemy : Enemies)
		{
			if (FVector::DistSquared2D(Enemy->GetActorLocation(), HitLocation) <= RadiusSquared)
			{
				Enemy->ApplyDamage(Damage, this);
			}
		}
		return;
	}

	// Single, and Pierce until it exists.
	if (HitTarget != nullptr)
	{
		HitTarget->ApplyDamage(Damage, this);
	}
}

//~ Debug ------------------------------------------------------------------------

void ABDTowerBase::DrawDebug() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr || (BDTowerPrivate::GShowRange == 0 && BDTowerPrivate::GShowTarget == 0))
	{
		return;
	}

	const UBDTowerSettings& Settings = UBDTowerSettings::Get();

	if (BDTowerPrivate::GShowRange != 0)
	{
		const UBDGridSubsystem* Grid = World->GetSubsystem<UBDGridSubsystem>();
		const float PlaneZ = Grid != nullptr ? Grid->GetOrigin().Z : GetActorLocation().Z;
		const FVector Center(GetActorLocation().X, GetActorLocation().Y, PlaneZ + Settings.RangeDrawHeightOffset);

		DrawDebugCircle(World, Center, GetEffectiveRange(), Settings.RangeSegments, Settings.RangeColor,
			BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority,
			Settings.RangeThickness, FVector::ForwardVector, FVector::RightVector, /*bDrawAxis*/ false);
	}

	const ABDEnemyBase* Target = CurrentTarget.Get();
	if (BDTowerPrivate::GShowTarget != 0 && Target != nullptr)
	{
		DrawDebugLine(World, GetMuzzleLocation(), Target->GetActorLocation(), Settings.TargetLineColor,
			BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority,
			Settings.TargetLineThickness);
	}
}

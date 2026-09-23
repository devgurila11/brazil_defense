// Brazil Defense. The single defender class, wherever it stands.

#include "Tower/BDTowerBase.h"

#include "BDLog.h"
#include "Candidate/BDCandidateSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Enemy/BDCandidate.h"
#include "Enemy/BDEnemyBase.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Grid/BDGridDebug.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Match/BDGameBalanceSettings.h"
#include "Match/BDMatchManager.h"
#include "Platform/BDPlatformComponent.h"
#include "Tower/BDProjectileBase.h"
#include "Tower/BDTowerData.h"
#include "Tower/BDTowerSettings.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDTowerPrivate
{
	static int32 GShowRange = 0;
	// On for now: the line from a defender to its creep is how the player sees who is shooting.
	static int32 GShowTarget = 1;

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
	Level = 1;
	EvolutionSpent = 0;
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
	Mesh->SetRelativeRotation(Data->MeshRotation);

	// Rest the mesh on the root, so the root is the floor contact whatever the pivot of
	// the asset is, with the scale and the rotation the data asked for already applied.
	const FBox Bounds = LoadedMesh->GetBoundingBox().TransformBy(
		FTransform(Data->MeshRotation, FVector::ZeroVector, Data->MeshScale));
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -Bounds.Min.Z));
}

float ABDTowerBase::GetReloadProgress() const
{
	if (!IsReloading() || Data == nullptr || Data->ReloadTime <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(1.0f - ReloadRemaining / Data->ReloadTime, 0.0f, 1.0f);
}

float ABDTowerBase::GetAimError() const
{
	const ABDEnemyBase* Target = CurrentTarget.Get();
	if (Target == nullptr)
	{
		return 0.0f;
	}

	const float WantedYaw = (Target->GetActorLocation() - Turret->GetComponentLocation()).GetSafeNormal2D().ToOrientationRotator().Yaw;
	return FMath::Abs(FMath::FindDeltaAngleDegrees(Turret->GetComponentRotation().Yaw, WantedYaw));
}

const FBDTowerLevel* ABDTowerBase::GetCurrentLevel() const
{
	return Data != nullptr ? Data->GetLevel(Level) : nullptr;
}

float ABDTowerBase::GetEffectiveDamage() const
{
	if (Data == nullptr)
	{
		return 0.0f;
	}

	// An authored level says what it does. Past the authored ones, the formula grows the
	// base damage: a single authored level is a base and nine formula levels.
	if (Data->HasAuthoredLevel(Level) && Level > 1)
	{
		return Data->GetLevel(Level)->Damage;
	}

	const FBDTowerLevel* Base = Data->GetLevel(1);
	return Base != nullptr ? Base->Damage * UBDGameBalanceSettings::Get().GetUpgradeDamageScale(Level) : 0.0f;
}

//~ Upgrades ---------------------------------------------------------------------

bool ABDTowerBase::IsMaxLevel() const
{
	return Level >= UBDTowerData::MaxLevels;
}

int32 ABDTowerBase::GetUpgradeCost() const
{
	if (Data == nullptr || IsMaxLevel())
	{
		return 0;
	}

	// Priced on the wave, like a piece: a level keeps its weight against a boss all match.
	const ABDMatchManager* Match = ABDMatchManager::Get(this);
	return UBDGameBalanceSettings::Get().GetUpgradeCostOnWave(Data->UpgradeCostBase, Level + 1, Match != nullptr ? Match->GetPriceWave() : 1);
}

float ABDTowerBase::GetDamageAtNextLevel() const
{
	if (Data == nullptr || IsMaxLevel())
	{
		return GetEffectiveDamage();
	}

	const int32 Next = Level + 1;
	if (Data->HasAuthoredLevel(Next))
	{
		return Data->GetLevel(Next)->Damage;
	}

	const FBDTowerLevel* Base = Data->GetLevel(1);
	return Base != nullptr ? Base->Damage * UBDGameBalanceSettings::Get().GetUpgradeDamageScale(Next) : 0.0f;
}

bool ABDTowerBase::CanUpgrade(FString& OutReason) const
{
	if (Data == nullptr)
	{
		OutReason = TEXT("no data");
		return false;
	}

	if (IsMaxLevel())
	{
		OutReason = FString::Printf(TEXT("already at the maximum level %d"), UBDTowerData::MaxLevels);
		return false;
	}

	const ABDMatchManager* Match = ABDMatchManager::Get(this);
	if (Match == nullptr)
	{
		OutReason = TEXT("no match to pay");
		return false;
	}

	// On a platform the shooters climb as a block: every slot filled, and the lowest
	// level buys first. The platform is the only thing that can see the others.
	if (const UBDPlatformComponent* Stand = GetPlatform())
	{
		FString Blocked;
		if (!Stand->CanOccupantEvolve(*this, Blocked))
		{
			OutReason = Blocked;
			return false;
		}
	}

	const int32 Cost = GetUpgradeCost();
	if (!Match->CanAffordPublicMoney(Cost))
	{
		OutReason = FString::Printf(TEXT("%d public money needed, %d held"), Cost, Match->GetPublicMoney());
		return false;
	}

	return true;
}

FString ABDTowerBase::DescribeUpgrade() const
{
	if (IsMaxLevel())
	{
		return FString::Printf(TEXT("%s is at the maximum level %d."), *GetName(), Level);
	}

	const ABDMatchManager* Match = ABDMatchManager::Get(this);
	const int32 Cost = GetUpgradeCost();
	const int32 Money = Match != nullptr ? Match->GetPublicMoney() : 0;

	FString Reason;
	const bool bCan = CanUpgrade(Reason);

	// Public money only: the count is not touched by an upgrade, so it is not quoted here.
	return FString::Printf(TEXT("%s level %d -> %d: cost %d public money, %d -> %d held, damage %.1f -> %.1f.%s"),
		*GetName(), Level, Level + 1, Cost, Money, Money - Cost, GetEffectiveDamage(), GetDamageAtNextLevel(),
		bCan ? TEXT("") : *FString::Printf(TEXT(" Refused: %s."), *Reason));
}

bool ABDTowerBase::Upgrade()
{
	FString Reason;
	if (!CanUpgrade(Reason))
	{
		UE_LOG(LogBDTower, Warning, TEXT("%s cannot be upgraded: %s."), *GetName(), *Reason);
		return false;
	}

	ABDMatchManager* Match = ABDMatchManager::Get(this);
	const int32 Cost = GetUpgradeCost();
	const float DamageBefore = GetEffectiveDamage();
	if (Match == nullptr || !Match->SpendPublicMoney(Cost))
	{
		return false;
	}

	++Level;
	EvolutionSpent += Cost;
	UE_LOG(LogBDBribe, Log, TEXT("EVOLVED %s to level %d of %d for %d public money: damage %.1f -> %.1f, %d public money left.%s"),
		*GetName(), Level, UBDTowerData::MaxLevels, Cost, DamageBefore, GetEffectiveDamage(), Match->GetPublicMoney(),
		// The one that finishes the floor is worth saying out loud: it is the only thing
		// that builds the platform a storey higher.
		GetPlatform() != nullptr && GetPlatform()->GetBlockLevel() >= Level
			? *FString::Printf(TEXT(" It completes floor %d of its platform."), Level)
			: TEXT(""));
	return true;
}

void ABDTowerBase::DebugSetLevel(const int32 NewLevel)
{
	Level = FMath::Clamp(NewLevel, 1, UBDTowerData::MaxLevels);
}

void ABDTowerBase::RestoreEvolution(const int32 NewLevel, const int32 Spent)
{
	Level = FMath::Clamp(NewLevel, 1, UBDTowerData::MaxLevels);
	EvolutionSpent = Level > 1 ? FMath::Max(0, Spent) : 0;
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

	// A creep the shots in the air already kill is not a target: it is dead, it just
	// does not know yet. When every creep in range is like that, the tower holds fire.
	if (Enemy->IsDoomed())
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
	// are written, rather than silently doing nothing. The candidate outranks every
	// priority: in range, it is the target, whatever else is there.
	ABDEnemyBase* Best = nullptr;
	int32 BestProgress = -1;

	for (ABDEnemyBase* Enemy : Waves->GetLivingEnemiesRef())
	{
		if (!IsValidTarget(Enemy, RangeSquared))
		{
			continue;
		}

		if (Enemy->IsCandidate())
		{
			return Enemy;
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
	else if (!Target->IsCandidate())
	{
		// A held target is not held against the candidate: the moment it comes into
		// range every defender switches to it, recognition and all.
		const UBDCandidateSubsystem* Candidates = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UBDCandidateSubsystem>() : nullptr;
		ABDEnemyBase* Candidate = Candidates != nullptr ? Candidates->GetCandidate() : nullptr;
		if (Candidate != nullptr && IsValidTarget(Candidate, RangeSquared))
		{
			UE_LOG(LogBDTower, Verbose, TEXT("%s drops %s for the candidate."), *GetName(), *Target->GetName());
			DropTarget();
			Target = Candidate;
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
	const bool bWasAligned = bAligned;
	bAligned = TurnTowards(Target, DeltaSeconds);
	if (bAligned && !bWasAligned)
	{
		UE_LOG(LogBDTower, Verbose, TEXT("%s aligned on %s: weapon yaw %.1f, target yaw %.1f, error %.2f deg."),
			*GetName(), *Target->GetName(), Turret->GetComponentRotation().Yaw,
			(Target->GetActorLocation() - Turret->GetComponentLocation()).GetSafeNormal2D().ToOrientationRotator().Yaw,
			GetAimError());
	}
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
	Projectile->Launch(this, Target, GetEffectiveDamage(), LevelStats.ProjectileSpeed * Grid->GetCellSize());
	++ShotsFired;
	if (UBDWaveSubsystem* Waves = GetWaves())
	{
		Waves->ReportShotFired();
	}

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

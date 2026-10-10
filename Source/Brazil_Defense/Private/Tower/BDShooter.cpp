// Brazil Defense. A platform shooter with a body: the man, the weapon in his hand, the shot.

#include "Tower/BDShooter.h"

#include "Animation/AnimSequenceBase.h"
#include "BDLog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Enemy/BDCreepSoundSubsystem.h"
#include "Enemy/BDEnemyBase.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Palace/BDAnimNotify_Shot.h"
#include "SkeletalMeshComponentBudgeted.h"
#include "Sound/SoundBase.h"
#include "Tower/BDShooterData.h"
#include "Tower/BDShotSound.h"

ABDShooter::ABDShooter()
{
	// In the animation budget with the horde: fifty men on the platforms are a crowd too.
	USkeletalMeshComponentBudgeted* Budgeted = CreateDefaultSubobject<USkeletalMeshComponentBudgeted>(TEXT("Body"));
	Budgeted->SetAutoCalculateSignificance(true);
	Body = Budgeted;
	// On the weapon pivot: the whole man turns to aim, the actor keeps its slot's facing.
	Body->SetupAttachment(GetTurret());
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	// The shot can wait on a notify of the gesture: the pose has to run on or off screen.
	// The budget may tick it less often far away; the notify still fires on the next tick.
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;

	Weapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon"));
	Weapon->SetupAttachment(Body);
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Weapon->SetGenerateOverlapEvents(false);
	Weapon->SetCanEverAffectNavigation(false);
}

const UBDShooterData* ABDShooter::GetShooterData() const
{
	return Cast<UBDShooterData>(GetData());
}

bool ABDShooter::HasBody() const
{
	return Body->GetSkeletalMeshAsset() != nullptr;
}

//~ Look -------------------------------------------------------------------------

void ABDShooter::ApplyBody()
{
	const UBDShooterData* Shooter = GetShooterData();
	USkeletalMesh* BodyMesh = Shooter != nullptr ? Shooter->BodyMesh.LoadSynchronous() : nullptr;
	Body->SetSkeletalMesh(BodyMesh);
	Body->SetVisibility(BodyMesh != nullptr);
	if (BodyMesh == nullptr)
	{
		// The placeholder of the tower data stands in, the weapon in front of it at its top.
		return;
	}

	Body->SetRelativeRotation(FRotator(0.0f, Shooter->BodyYaw, 0.0f));
	Body->SetRelativeScale3D(FVector(Shooter->BodyScale));

	// The body is the look now. The placeholder stays as the thing the cursor picks, unseen.
	if (UStaticMeshComponent* Placeholder = GetMesh())
	{
		Placeholder->SetVisibility(false);
	}
	CurrentAnimation = nullptr;
	PlayIdle();
}

FName ABDShooter::GetWeaponAttachSocket() const
{
	const UBDShooterData* Shooter = GetShooterData();
	return Shooter != nullptr && HasBody() && Body->DoesSocketExist(Shooter->HandSocket) ? Shooter->HandSocket : NAME_None;
}

void ABDShooter::ApplyWeapon()
{
	const UBDShooterData* Shooter = GetShooterData();
	WeaponLevel = GetTowerLevel();
	const FBDShooterWeapon* Entry = Shooter != nullptr ? Shooter->GetWeapon(WeaponLevel) : nullptr;
	if (Entry == nullptr)
	{
		Weapon->SetStaticMesh(nullptr);
		return;
	}

	const TSoftObjectPtr<UStaticMesh> MeshRef = Shooter->ResolveWeaponMesh(WeaponLevel);
	UStaticMesh* WeaponMesh = MeshRef.LoadSynchronous();
	if (!MeshRef.IsNull() && WeaponMesh == nullptr)
	{
		UE_LOG(LogBDTower, Error, TEXT("%s: weapon mesh %s failed to load."), *GetName(), *MeshRef.ToString());
	}
	Weapon->SetStaticMesh(WeaponMesh);

	// In the hand when the body has the socket; at the body's root when it has not; and on
	// the placeholder's top, in front, while there is no body at all.
	const FName Socket = GetWeaponAttachSocket();
	if (HasBody())
	{
		Weapon->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		Weapon->SetRelativeTransform(Entry->Grip);
		if (Socket.IsNone())
		{
			UE_LOG(LogBDTower, Warning, TEXT("%s: the body has no socket '%s' for the weapon; it hangs at the root. Make the socket in the skeleton or change HandSocket on %s."),
				*GetName(), *Shooter->HandSocket.ToString(), *GetNameSafe(Shooter));
		}
	}
	else
	{
		Weapon->AttachToComponent(GetTurret(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		const UStaticMeshComponent* Placeholder = GetMesh();
		const float Top = Placeholder != nullptr && Placeholder->GetStaticMesh() != nullptr
			? Placeholder->Bounds.Origin.Z + Placeholder->Bounds.BoxExtent.Z - GetTurret()->GetComponentLocation().Z
			: 0.0f;
		Weapon->SetRelativeTransform(Entry->Grip * FTransform(FVector(0.0f, 0.0f, Top)));
	}

	UE_LOG(LogBDTower, Log, TEXT("%s holds %s for level %d%s (%s)."), *GetName(), *Entry->Name.ToString(), WeaponLevel,
		Entry->bPlaceholder ? TEXT(", a placeholder") : TEXT(""),
		WeaponMesh != nullptr ? *WeaponMesh->GetName() : TEXT("no model yet"));
}

//~ Animation --------------------------------------------------------------------

void ABDShooter::PlayIdle()
{
	const UBDShooterData* Shooter = GetShooterData();
	UAnimSequenceBase* Idle = Shooter != nullptr && HasBody() ? Shooter->IdleAnimation.LoadSynchronous() : nullptr;
	if (Idle == nullptr || Idle == CurrentAnimation)
	{
		return;
	}
	Body->PlayAnimation(Idle, /*bLooping*/ true);
	Body->SetPlayRate(1.0f);
	CurrentAnimation = Idle;
}

float ABDShooter::PlayGesture()
{
	const UBDShooterData* Shooter = GetShooterData();
	UAnimSequenceBase* Shoot = Shooter != nullptr && HasBody() ? Shooter->ShootAnimation.LoadSynchronous() : nullptr;
	SinceGesture = 0.0f;
	if (Shoot == nullptr)
	{
		GestureSeconds = 0.0f;
		return 0.0f;
	}

	// Never longer than the gap between two shots: sped up to fit, never slowed down.
	const float Length = FMath::Max(Shoot->GetPlayLength(), KINDA_SMALL_NUMBER);
	const float Rate = FMath::Max(1.0f, Length * GetFireRate());
	Body->PlayAnimation(Shoot, /*bLooping*/ false);
	Body->SetPlayRate(Rate);
	CurrentAnimation = Shoot;
	GestureSeconds = Length / Rate;
	++GesturesPlayed;
	return GestureSeconds;
}

bool ABDShooter::UsesShotNotify() const
{
	const UBDShooterData* Shooter = GetShooterData();
	return Shooter != nullptr && HasBody() && UBDAnimNotify_Shot::IsOn(Shooter->ShootAnimation.LoadSynchronous());
}

//~ Shooting ---------------------------------------------------------------------

void ABDShooter::BeginShot(ABDEnemyBase* Target, const FBDTowerLevel& LevelStats)
{
	// A shot still held by the last gesture goes first: never two in one gesture.
	if (bShotPending)
	{
		CommitPendingShot();
	}

	const float Gesture = PlayGesture();
	if (Gesture > 0.0f && UsesShotNotify())
	{
		// Out on the notify; the end of the gesture is the latest it may leave.
		bShotPending = true;
		PendingRemaining = Gesture;
		return;
	}
	CommitShot(Target, LevelStats);
}

void ABDShooter::OnShotFrame()
{
	if (bShotPending)
	{
		CommitPendingShot();
	}
}

void ABDShooter::CommitPendingShot()
{
	bShotPending = false;
	PendingRemaining = 0.0f;

	// The creep the gesture started on may be dead by now: the one right to shoot now.
	ABDEnemyBase* Target = ResolveShotTarget();
	const FBDTowerLevel* LevelStats = GetCurrentLevel();
	if (Target == nullptr || LevelStats == nullptr)
	{
		UE_LOG(LogBDTower, Verbose, TEXT("%s: the gesture ended with no creep left to shoot."), *GetName());
		return;
	}
	CommitShot(Target, *LevelStats);
}

void ABDShooter::NotifyKill()
{
	Super::NotifyKill();
	const UBDShooterData* Shooter = GetShooterData();
	UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(this);
	if (Shooter != nullptr && Sounds != nullptr)
	{
		Sounds->TryCelebrate(*this, Shooter->CelebrationSound);
	}
}

void ABDShooter::Fire(ABDEnemyBase* Target, const FBDTowerLevel& LevelStats)
{
	const int32 Before = GetShotsFired();
	Super::Fire(Target, LevelStats);
	if (GetShotsFired() == Before)
	{
		return;
	}

	const UBDShooterData* Shooter = GetShooterData();
	if (Shooter == nullptr)
	{
		return;
	}
	const int32 ShotLevel = GetTowerLevel();
	const FBDShooterWeapon* Entry = Shooter->GetWeapon(ShotLevel);

	// How far the barrel points off the line to the target, for fitting a weapon's Grip.
	// Only on screen: off it the pose ticks without moving the bones, and the socket reads the reference pose.
	if (Entry != nullptr && Target != nullptr && Body->WasRecentlyRendered() && Weapon->GetStaticMesh() != nullptr && Weapon->DoesSocketExist(Entry->MuzzleSocket)
		&& UE_LOG_ACTIVE(LogBDTower, Verbose))
	{
		const FTransform Muzzle = Weapon->GetSocketTransform(Entry->MuzzleSocket);
		const FRotator Barrel = Muzzle.GetUnitAxis(EAxis::X).Rotation();
		const FRotator Line = (Target->GetActorLocation() - Muzzle.GetLocation()).Rotation();
		UE_LOG(LogBDTower, Verbose, TEXT("%s: barrel off the line of fire by %.1f deg of yaw (+ right), %.1f of pitch, %.0f%% into the gesture."), *GetName(),
			FMath::FindDeltaAngleDegrees(Line.Yaw, Barrel.Yaw), FMath::FindDeltaAngleDegrees(Line.Pitch, Barrel.Pitch),
			Body->IsPlaying() && CurrentAnimation != nullptr && CurrentAnimation->GetPlayLength() > 0.0f ? 100.0f * Body->GetPosition() / CurrentAnimation->GetPlayLength() : 0.0f);
	}

	// The flash on the tip of the barrel, riding with the weapon while it plays.
	++FlashesRequested;
	if (UNiagaraSystem* Flash = Shooter->ResolveMuzzleFlash(ShotLevel).LoadSynchronous())
	{
		const FName Muzzle = Entry != nullptr ? Entry->MuzzleSocket : NAME_None;
		UNiagaraComponent* Spawned = nullptr;
		if (Weapon->GetStaticMesh() != nullptr && Weapon->DoesSocketExist(Muzzle))
		{
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(Flash, Weapon, Muzzle, FVector::ZeroVector, FRotator::ZeroRotator,
				EAttachLocation::SnapToTarget, /*bAutoDestroy*/ true);
		}
		else
		{
			Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Flash, GetMuzzleLocation(), GetTurret()->GetComponentRotation());
		}
		FlashesPlayed += Spawned != nullptr ? 1 : 0;
	}

	// The sound of the weapon, on the same instant, from the same place.
	if (UBDShotSoundSubsystem* Shots = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UBDShotSoundSubsystem>() : nullptr)
	{
		Shots->PlayShot(Shooter->ResolveFireSound(ShotLevel).LoadSynchronous(), GetMuzzleLocation());
	}
}

void ABDShooter::OnShotLanded(ABDEnemyBase* HitTarget, const FVector& HitLocation)
{
	const UBDShooterData* Shooter = GetShooterData();
	if (Shooter == nullptr)
	{
		return;
	}

	++ImpactsRequested;
	if (UNiagaraSystem* Impact = Shooter->ImpactEffect.LoadSynchronous())
	{
		// Facing back the way the shot came, so a spray reads as coming out of the hit.
		const FRotator Back = (GetMuzzleLocation() - HitLocation).GetSafeNormal().ToOrientationRotator();
		ImpactsPlayed += UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Impact, HitLocation, Back) != nullptr ? 1 : 0;
	}
}

float ABDShooter::GetDamageAtLevel(const int32 AtLevel) const
{
	const UBDShooterData* Shooter = GetShooterData();
	const FBDShooterWeapon* Entry = Shooter != nullptr ? Shooter->GetWeapon(AtLevel) : nullptr;
	return Entry != nullptr && Entry->Damage > 0.0f ? Entry->Damage : Super::GetDamageAtLevel(AtLevel);
}

float ABDShooter::GetFireRateAtLevel(const int32 AtLevel) const
{
	const UBDShooterData* Shooter = GetShooterData();
	const FBDShooterWeapon* Entry = Shooter != nullptr ? Shooter->GetWeapon(AtLevel) : nullptr;
	return Entry != nullptr && Entry->FireRate > 0.0f ? Entry->FireRate : Super::GetFireRateAtLevel(AtLevel);
}

FVector ABDShooter::GetMuzzleLocation() const
{
	const UBDShooterData* Shooter = GetShooterData();
	const FBDShooterWeapon* Entry = Shooter != nullptr ? Shooter->GetWeapon(GetTowerLevel()) : nullptr;

	// The tip of the barrel; the hand when the weapon has no muzzle socket yet; the
	// placeholder's top when there is no body.
	if (Entry != nullptr && Weapon->GetStaticMesh() != nullptr && Weapon->DoesSocketExist(Entry->MuzzleSocket))
	{
		return Weapon->GetSocketLocation(Entry->MuzzleSocket);
	}
	const FName Hand = GetWeaponAttachSocket();
	if (!Hand.IsNone())
	{
		return Body->GetSocketLocation(Hand);
	}
	return Super::GetMuzzleLocation();
}

//~ Tick -------------------------------------------------------------------------

void ABDShooter::InitializeTower(const UBDTowerData* InData)
{
	Super::InitializeTower(InData);
	bShotPending = false;
	ApplyBody();
	ApplyWeapon();
}

void ABDShooter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// A star bought, a level restored: the weapon of the new level goes in his hand.
	if (WeaponLevel != INDEX_NONE && WeaponLevel != GetTowerLevel())
	{
		ApplyWeapon();
	}

	// The notify never came (an animation cut short, a pose not ticking): the shot leaves
	// at the end of the gesture rather than not at all.
	if (bShotPending)
	{
		PendingRemaining -= DeltaSeconds;
		if (PendingRemaining <= 0.0f)
		{
			CommitPendingShot();
		}
	}

	const UBDShooterData* Shooter = GetShooterData();
	SinceGesture = SinceGesture < TNumericLimits<float>::Max() ? SinceGesture + DeltaSeconds : SinceGesture;
	if (Shooter != nullptr && SinceGesture > FMath::Max(GestureSeconds, Shooter->IdleAfter))
	{
		PlayIdle();
	}
}

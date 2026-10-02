// Brazil Defense. The Agent: the one man the palace sends out on patrol.

#include "Palace/BDAgent.h"

#include "Algo/Reverse.h"
#include "Animation/AnimSequenceBase.h"
#include "BDLog.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Sound/SoundBase.h"
#include "Enemy/BDEnemyBase.h"
#include "Grid/BDGridDebug.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Match/BDMatchManager.h"
#include "Match/BDMatchTypes.h"
#include "Palace/BDPalace.h"
#include "Palace/BDPalaceData.h"
#include "Tower/BDShotSound.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDAgentPrivate
{
	static int32 GShowRange = 1;
	static FAutoConsoleVariableRef CVarShowRange(
		TEXT("BD.Agent.ShowRange"),
		GShowRange,
		TEXT("1 draws the Agent's detection radius on the board. 0 hides it."));

	static int32 GShowShots = 1;
	static FAutoConsoleVariableRef CVarShowShots(
		TEXT("BD.Agent.ShowShots"),
		GShowShots,
		TEXT("1 flashes a line from the Agent to his target at every shot, gone a moment later. 0 hides them."));

	// Colours of his own, apart from the towers' (cyan reach, red target, yellow next level).
	static const FColor RangeColor(90, 230, 110, 255);
	static const FColor ShotColor(255, 60, 220, 255);

	/**
	 * How long the line of a shot stays on screen, in seconds: a flash, not a beam. A shot
	 * is a discrete thing; a weapon that fires continuously (a laser, a burst) will draw a
	 * steady line instead, so the two read apart at a glance.
	 */
	static constexpr float ShotFlashLife = 0.06f;

	/** Where on a creep the shot lands, over its location. */
	static constexpr float TargetChestHeight = 100.0f;

	static const FIntPoint Steps[4] = { FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1) };
}

ABDAgent::ABDAgent()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Nothing collides with him and he collides with nothing; the cursor does not pick him either.
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPose;
}

UBDGridSubsystem* ABDAgent::GetGrid() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
}

const UBDPalaceData* ABDAgent::GetData() const
{
	const ABDPalace* Home = Palace.Get();
	return Home != nullptr ? Home->GetData() : nullptr;
}

const FBDAgentWeapon* ABDAgent::GetWeapon() const
{
	const UBDPalaceData* Data = GetData();
	const ABDPalace* Home = Palace.Get();
	return Data != nullptr && Home != nullptr ? Data->GetWeapon(Home->GetPalaceLevel()) : nullptr;
}

float ABDAgent::GetDetectionRadius() const
{
	const UBDPalaceData* Data = GetData();
	const UBDGridSubsystem* Grid = GetGrid();
	return Data != nullptr && Grid != nullptr ? Data->DetectionRadiusCells * Grid->GetCellSize() : 0.0f;
}

float ABDAgent::GetPatrolFraction() const
{
	const UBDPalaceData* Data = GetData();
	return Data != nullptr && Data->PatrolTime > 0.0f ? FMath::Clamp(PatrolRemaining / Data->PatrolTime, 0.0f, 1.0f) : 0.0f;
}

//~ Setup ------------------------------------------------------------------------

void ABDAgent::InitializeAgent(ABDPalace* InPalace)
{
	Palace = InPalace;
	const UBDPalaceData* Data = GetData();

	if (Data != nullptr)
	{
		PatrolRemaining = Data->PatrolTime;

		if (USkeletalMesh* LoadedMesh = Data->AgentMesh.LoadSynchronous())
		{
			Body->SetSkeletalMesh(LoadedMesh);
			Body->SetRelativeScale3D(FVector(Data->AgentMeshScale));
			Body->SetRelativeRotation(FRotator(0.0f, Data->AgentMeshYaw, 0.0f));

			// Feet on the floor, whatever the pivot.
			const FBox Bounds = LoadedMesh->GetImportedBounds().GetBox();
			Body->SetRelativeLocation(FVector(0.0f, 0.0f, -Bounds.Min.Z * Data->AgentMeshScale));
		}
		else
		{
			UE_LOG(LogBDTower, Error, TEXT("%s: agent mesh %s of %s failed to load."), *GetName(), *Data->AgentMesh.ToString(), *Data->GetName());
		}
	}

	// Out of the door: he starts on the cell he will sleep on.
	FBDCellCoord Start;
	if (FindSleepCell(Start))
	{
		HeadingCell = Start;
		SetActorLocation(CellPoint(Start));
	}
	else if (const UBDGridSubsystem* Grid = GetGrid())
	{
		Grid->WorldToCell(GetActorLocation(), HeadingCell);
	}
	if (InPalace != nullptr)
	{
		SetActorRotation(FRotator(0.0f, InPalace->GetActorRotation().Yaw, 0.0f));
	}

	UE_LOG(LogBDTower, Log, TEXT("Agent '%s' out of '%s' at %s: %.0fs of patrol, %.1f cells of reach, %s."),
		*GetName(), *GetNameSafe(InPalace), *HeadingCell.ToString(), PatrolRemaining,
		Data != nullptr ? Data->DetectionRadiusCells : 0.0f,
		GetWeapon() != nullptr ? *GetWeapon()->Name.ToString() : TEXT("no weapon"));

	StartWalk();
}

FVector ABDAgent::CellPoint(const FBDCellCoord& Coord) const
{
	const UBDGridSubsystem* Grid = GetGrid();
	return Grid != nullptr ? Grid->CellToWorld(Coord) : GetActorLocation();
}

bool ABDAgent::CanStand(const FBDCellCoord& Coord) const
{
	const UBDGridSubsystem* Grid = GetGrid();
	return Grid != nullptr && Grid->IsValidCoord(Coord) && Grid->GetCellState(Coord) != EBDCellState::Blocked;
}

bool ABDAgent::CanStep(const FBDCellCoord& From, const FBDCellCoord& To) const
{
	const UBDGridSubsystem* Grid = GetGrid();
	return Grid != nullptr && CanStand(To) && !Grid->IsEdgeBlocked(From, To);
}

bool ABDAgent::FindSleepCell(FBDCellCoord& OutCell) const
{
	const ABDPalace* Home = Palace.Get();
	const UBDGridSubsystem* Grid = GetGrid();
	if (Home == nullptr || Grid == nullptr)
	{
		return false;
	}

	// The palace is 2x2 around its location: the cells just outside a side sit 1.5 cells
	// out, and half a cell to either hand of the middle.
	const float Cell = Grid->GetCellSize();
	const FVector Center = Home->GetActorLocation();
	const FVector Forward = Home->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const FVector Sides[4] = { Forward, Right, -Forward, -Right };

	bool bFoundAny = false;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (const FVector& Side : Sides)
		{
			const FVector Across = FVector::CrossProduct(FVector::UpVector, Side);
			for (const float Hand : { 0.5f, -0.5f })
			{
				FBDCellCoord Coord;
				if (!Grid->WorldToCell(Center + Side * (1.5f * Cell) + Across * (Hand * Cell), Coord) || !CanStand(Coord))
				{
					continue;
				}
				// First a cell with nothing on it, so he does not lie inside a tower.
				if (Pass == 1 || Grid->GetCellState(Coord) == EBDCellState::Free)
				{
					OutCell = Coord;
					return true;
				}
				bFoundAny = true;
			}
		}
		if (!bFoundAny)
		{
			break;
		}
	}
	return false;
}

FVector ABDAgent::GetSleepSpot() const
{
	FBDCellCoord Cell;
	return FindSleepCell(Cell) ? CellPoint(Cell) : GetActorLocation();
}

FVector ABDAgent::GetBarAnchor() const
{
	const float Top = Body->GetSkeletalMeshAsset() != nullptr ? Body->Bounds.Origin.Z + Body->Bounds.BoxExtent.Z : GetActorLocation().Z + 400.0f;
	return FVector(GetActorLocation().X, GetActorLocation().Y, Top + 60.0f);
}

TArray<FBDCellCoord> ABDAgent::FindWay(const FBDCellCoord& From, const FBDCellCoord& To) const
{
	TArray<FBDCellCoord> Way;
	if (From == To)
	{
		Way.Add(From);
		return Way;
	}

	TMap<FBDCellCoord, FBDCellCoord> CameFrom;
	TArray<FBDCellCoord> Frontier;
	Frontier.Add(From);
	CameFrom.Add(From, From);

	for (int32 Head = 0; Head < Frontier.Num(); ++Head)
	{
		const FBDCellCoord Current = Frontier[Head];
		for (const FIntPoint& Step : BDAgentPrivate::Steps)
		{
			const FBDCellCoord Next(Current.X + Step.X, Current.Y + Step.Y);
			if (CameFrom.Contains(Next) || !CanStep(Current, Next))
			{
				continue;
			}
			CameFrom.Add(Next, Current);
			if (Next == To)
			{
				for (FBDCellCoord Back = To; Back != From; Back = CameFrom[Back])
				{
					Way.Add(Back);
				}
				Way.Add(From);
				Algo::Reverse(Way);
				return Way;
			}
			Frontier.Add(Next);
		}
	}
	return Way;
}

//~ Patrol -----------------------------------------------------------------------

void ABDAgent::DecideNext()
{
	const UBDPalaceData* Data = GetData();
	if (Data != nullptr && FMath::FRand() < Data->IdleChance)
	{
		StartIdle();
	}
	else
	{
		StartWalk();
	}
}

void ABDAgent::StartWalk()
{
	const UBDPalaceData* Data = GetData();
	const int32 MinCells = Data != nullptr ? FMath::Max(1, Data->WalkCellsMin) : 1;
	const int32 MaxCells = Data != nullptr ? FMath::Max(MinCells, Data->WalkCellsMax) : 1;

	// A random side first, then the others in turn: a boxed in agent still finds the way out.
	const int32 FirstSide = FMath::RandRange(0, 3);
	for (int32 Turn = 0; Turn < 4; ++Turn)
	{
		const FIntPoint& Step = BDAgentPrivate::Steps[(FirstSide + Turn) % 4];
		const int32 Wanted = FMath::RandRange(MinCells, MaxCells);

		TArray<FBDCellCoord> Walk;
		FBDCellCoord Current = HeadingCell;
		for (int32 Count = 0; Count < Wanted; ++Count)
		{
			const FBDCellCoord Next(Current.X + Step.X, Current.Y + Step.Y);
			if (!CanStep(Current, Next))
			{
				break;
			}
			Walk.Add(Next);
			Current = Next;
		}

		if (Walk.Num() > 0)
		{
			Route = MoveTemp(Walk);
			State = EBDAgentState::Walking;
			PlayStateAnimation();
			return;
		}
	}

	StartIdle();
}

void ABDAgent::StartIdle()
{
	const UBDPalaceData* Data = GetData();
	Route.Reset();
	State = EBDAgentState::Idle;
	IdleRemaining = Data != nullptr ? FMath::FRandRange(Data->IdleTimeMin, FMath::Max(Data->IdleTimeMin, Data->IdleTimeMax)) : 1.0f;
	PlayStateAnimation();
}

void ABDAgent::StartReturn()
{
	const ABDMatchManager* Match = ABDMatchManager::Get(this);
	WaveAtSleep = Match != nullptr ? Match->GetCurrentWave() : 0;
	CurrentTarget.Reset();
	PatrolRemaining = 0.0f;

	// Halfway into a cell, that cell is finished first: the edge to it was already crossed.
	const bool bMidStep = Route.Num() > 0;
	const FBDCellCoord From = bMidStep ? Route[0] : HeadingCell;

	FBDCellCoord Home;
	TArray<FBDCellCoord> Way = FindSleepCell(Home) ? FindWay(From, Home) : TArray<FBDCellCoord>();
	if (Way.Num() == 0)
	{
		UE_LOG(LogBDTower, Warning, TEXT("%s: no way home from %s, sleeping where he stands."), *GetName(), *From.ToString());
		Route.Reset();
		StartSleep();
		return;
	}
	if (!bMidStep)
	{
		Way.RemoveAt(0);
	}

	UE_LOG(LogBDTower, Log, TEXT("%s: patrol time over on wave %d, %d cell(s) home."), *GetName(), WaveAtSleep, Way.Num());
	Route = MoveTemp(Way);
	State = EBDAgentState::Returning;
	PlayStateAnimation();
}

void ABDAgent::StartSleep()
{
	Route.Reset();
	CurrentTarget.Reset();
	State = EBDAgentState::Sleeping;
	FBDCellCoord Home;
	if (FindSleepCell(Home) && Home == HeadingCell)
	{
		SetActorLocation(CellPoint(Home));
	}
	if (const ABDPalace* Building = Palace.Get())
	{
		// Lying along the palace's front, not into it.
		SetActorRotation(FRotator(0.0f, Building->GetActorRotation().Yaw, 0.0f));
	}
	PlayStateAnimation();
	UE_LOG(LogBDTower, Log, TEXT("%s asleep at %s until wave %d is over."), *GetName(), *HeadingCell.ToString(),
		WaveAtSleep + (GetData() != nullptr ? GetData()->SleepWaves : 1));
}

void ABDAgent::WakeUp()
{
	const UBDPalaceData* Data = GetData();
	PatrolRemaining = Data != nullptr ? Data->PatrolTime : 0.0f;
	UE_LOG(LogBDTower, Log, TEXT("%s awake: %.0fs of patrol."), *GetName(), PatrolRemaining);
	StartWalk();
}

bool ABDAgent::StepAlong(const float DeltaSeconds)
{
	const UBDPalaceData* Data = GetData();
	const UBDGridSubsystem* Grid = GetGrid();
	if (Route.Num() == 0 || Data == nullptr || Grid == nullptr)
	{
		return true;
	}

	float Budget = Data->WalkSpeed * Grid->GetCellSize() * DeltaSeconds;
	while (Route.Num() > 0)
	{
		const FVector Here = GetActorLocation();
		const FVector There = CellPoint(Route[0]);
		const FVector ToThere = There - Here;
		const float Distance = ToThere.Size2D();
		if (Distance > KINDA_SMALL_NUMBER)
		{
			TurnTowards(ToThere.GetSafeNormal2D().ToOrientationRotator().Yaw, DeltaSeconds, 0.0f);
		}
		if (Distance > Budget)
		{
			SetActorLocation(Here + ToThere.GetSafeNormal2D() * Budget);
			return false;
		}
		SetActorLocation(There);
		Budget -= Distance;
		HeadingCell = Route[0];
		Route.RemoveAt(0);
	}
	return true;
}

bool ABDAgent::TurnTowards(const float WantedYaw, const float DeltaSeconds, const float Tolerance)
{
	const UBDPalaceData* Data = GetData();
	const float Rate = Data != nullptr ? Data->TurnRate : 540.0f;
	const float NewYaw = FMath::FixedTurn(GetActorRotation().Yaw, WantedYaw, Rate * DeltaSeconds);
	SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
	return FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, WantedYaw)) <= Tolerance + KINDA_SMALL_NUMBER;
}

//~ Combat -----------------------------------------------------------------------

bool ABDAgent::IsValidTarget(const ABDEnemyBase* Enemy, const float RadiusSquared) const
{
	return Enemy != nullptr && IsValid(Enemy) && !Enemy->HasArrived() && !Enemy->IsDoomed()
		&& Enemy->GetCurrentHealth() > 0.0f
		&& FVector::DistSquared2D(Enemy->GetActorLocation(), GetActorLocation()) <= RadiusSquared;
}

ABDEnemyBase* ABDAgent::AcquireTarget(const float RadiusSquared) const
{
	const UWorld* World = GetWorld();
	const UBDWaveSubsystem* Waves = World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr;
	const UBDPalaceData* Data = GetData();
	if (Waves == nullptr || Data == nullptr)
	{
		return nullptr;
	}

	ABDEnemyBase* Best = nullptr;
	float BestScore = -MAX_flt;
	for (ABDEnemyBase* Enemy : Waves->GetLivingEnemiesRef())
	{
		if (!IsValidTarget(Enemy, RadiusSquared))
		{
			continue;
		}
		const float Score = Data->bTargetFurthestAlong
			? static_cast<float>(Enemy->GetCurrentPathIndex())
			: -FVector::DistSquared2D(Enemy->GetActorLocation(), GetActorLocation());
		if (Score > BestScore)
		{
			Best = Enemy;
			BestScore = Score;
		}
	}
	return Best;
}

FVector ABDAgent::GetMuzzleLocation() const
{
	// About the hand, held out in front at chest height.
	const float Height = Body->GetSkeletalMeshAsset() != nullptr ? Body->Bounds.BoxExtent.Z * 1.2f : 200.0f;
	return GetActorLocation() + GetActorForwardVector() * 80.0f + FVector(0.0f, 0.0f, Height);
}

void ABDAgent::Fire(ABDEnemyBase* Target, const FBDAgentWeapon& Weapon)
{
	++ShotsFired;
	UWorld* World = GetWorld();
	if (World != nullptr && BDAgentPrivate::GShowShots != 0)
	{
		DrawDebugLine(World, GetMuzzleLocation(), Target->GetActorLocation() + FVector(0.0f, 0.0f, BDAgentPrivate::TargetChestHeight),
			BDAgentPrivate::ShotColor, /*bPersistent*/ false, BDAgentPrivate::ShotFlashLife, /*DepthPriority*/ 0, /*Thickness*/ 4.0f);
	}

	// The sound of the weapon, on the same instant as the flash and the cadence.
	if (UBDShotSoundSubsystem* Shots = World != nullptr ? World->GetSubsystem<UBDShotSoundSubsystem>() : nullptr)
	{
		Shots->PlayShot(Weapon.FireSound.LoadSynchronous(), GetMuzzleLocation());
	}
	UE_LOG(LogBDTower, Verbose, TEXT("%s fires %s at %s: %.0f damage."), *GetName(), *Weapon.Name.ToString(), *Target->GetName(), Weapon.Damage);
	Target->ApplyDamage(Weapon.Damage, this);
}

void ABDAgent::NotifyKill()
{
	++Kills;
	const FBDAgentWeapon* Weapon = GetWeapon();
	if (Weapon == nullptr || State == EBDAgentState::Sleeping || State == EBDAgentState::Returning)
	{
		return;
	}
	PatrolRemaining += Weapon->KillBonusSeconds;
	BonusEarned += Weapon->KillBonusSeconds;
}

bool ABDAgent::Kick()
{
	const UBDPalaceData* Data = GetData();
	UAnimSequenceBase* Animation = Data != nullptr ? Data->KickAnimation.LoadSynchronous() : nullptr;
	if (Animation == nullptr || State == EBDAgentState::Sleeping)
	{
		return false;
	}

	Body->PlayAnimation(Animation, /*bLooping*/ false);
	CurrentLoop = nullptr;
	KickRemaining = Animation->GetPlayLength();
	UE_LOG(LogBDTower, Log, TEXT("%s kicks: %.2fs."), *GetName(), KickRemaining);
	return true;
}

//~ Animation --------------------------------------------------------------------

void ABDAgent::PlayLoop(const TSoftObjectPtr<UAnimSequenceBase>& Animation, const float Rate)
{
	UAnimSequenceBase* Loaded = Animation.LoadSynchronous();
	if (Loaded == nullptr)
	{
		return;
	}
	if (Loaded != CurrentLoop)
	{
		Body->PlayAnimation(Loaded, /*bLooping*/ true);
		CurrentLoop = Loaded;
	}
	Body->SetPlayRate(Rate);
}

void ABDAgent::PlayStateAnimation()
{
	const UBDPalaceData* Data = GetData();
	if (Data == nullptr || KickRemaining > 0.0f)
	{
		return;
	}

	switch (State)
	{
	case EBDAgentState::Walking:
	case EBDAgentState::Returning:
		PlayLoop(Data->WalkAnimation, Data->WalkAnimRate);
		break;
	case EBDAgentState::Idle:
		PlayLoop(Data->IdleAnimation);
		break;
	case EBDAgentState::Shooting:
		PlayLoop(Data->ShootAnimation);
		break;
	case EBDAgentState::Sleeping:
		PlayLoop(Data->SleepAnimation);
		break;
	}
}

//~ Tick -------------------------------------------------------------------------

void ABDAgent::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Palace.IsValid())
	{
		// The palace takes him with it; this is only the guard should it not.
		Destroy();
		return;
	}

	const UBDPalaceData* Data = GetData();
	if (Data == nullptr || GetGrid() == nullptr)
	{
		return;
	}

	FireCooldown = FMath::Max(0.0f, FireCooldown - DeltaSeconds);

	// The clock runs always, waves or not, kick or not; only sleep stops it.
	if (State != EBDAgentState::Sleeping && State != EBDAgentState::Returning)
	{
		PatrolRemaining -= DeltaSeconds;
		if (PatrolRemaining <= 0.0f)
		{
			StartReturn();
		}
	}

	// A kick holds him on the spot until it is over.
	if (KickRemaining > 0.0f)
	{
		KickRemaining = FMath::Max(0.0f, KickRemaining - DeltaSeconds);
		if (KickRemaining <= 0.0f)
		{
			PlayStateAnimation();
		}
		DrawDebug();
		return;
	}

	switch (State)
	{
	case EBDAgentState::Sleeping:
	{
		// Up once the wave after the bar ran out has been fought without him.
		const ABDMatchManager* Match = ABDMatchManager::Get(this);
		if (Match != nullptr && Match->GetPhase() != EBDMatchPhase::WaveActive
			&& Match->GetCurrentWave() >= WaveAtSleep + FMath::Max(1, Data->SleepWaves))
		{
			WakeUp();
		}
		break;
	}

	case EBDAgentState::Returning:
		if (StepAlong(DeltaSeconds))
		{
			StartSleep();
		}
		break;

	default:
	{
		const float Radius = GetDetectionRadius();
		const float RadiusSquared = Radius * Radius;

		// One creep at a time: the one he has, while it lasts; otherwise the best in reach.
		ABDEnemyBase* Target = CurrentTarget.Get();
		if (!IsValidTarget(Target, RadiusSquared))
		{
			Target = AcquireTarget(RadiusSquared);
			CurrentTarget = Target;
		}

		if (Target != nullptr)
		{
			if (State != EBDAgentState::Shooting)
			{
				State = EBDAgentState::Shooting;
				PlayStateAnimation();
			}
			const float WantedYaw = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().ToOrientationRotator().Yaw;
			const FBDAgentWeapon* Weapon = GetWeapon();
			if (TurnTowards(WantedYaw, DeltaSeconds, Data->AimTolerance) && Weapon != nullptr && FireCooldown <= 0.0f)
			{
				Fire(Target, *Weapon);
				FireCooldown = 1.0f / FMath::Max(Weapon->FireRate, KINDA_SMALL_NUMBER);
			}
			break;
		}

		if (State == EBDAgentState::Shooting)
		{
			// Reach clear: back to the walk he was on, or a new decision.
			if (Route.Num() > 0)
			{
				State = EBDAgentState::Walking;
				PlayStateAnimation();
			}
			else
			{
				DecideNext();
			}
		}

		if (State == EBDAgentState::Walking)
		{
			if (StepAlong(DeltaSeconds))
			{
				DecideNext();
			}
		}
		else if (State == EBDAgentState::Idle)
		{
			IdleRemaining -= DeltaSeconds;
			if (IdleRemaining <= 0.0f)
			{
				StartWalk();
			}
		}
		break;
	}
	}

	DrawDebug();
}

void ABDAgent::DrawDebug() const
{
	UWorld* World = GetWorld();
	if (World == nullptr || BDAgentPrivate::GShowRange == 0 || State == EBDAgentState::Sleeping || State == EBDAgentState::Returning)
	{
		return;
	}

	const UBDGridSubsystem* Grid = GetGrid();
	const float PlaneZ = Grid != nullptr ? Grid->GetOrigin().Z : GetActorLocation().Z;
	const FVector Center(GetActorLocation().X, GetActorLocation().Y, PlaneZ + 5.0f);
	const float Radius = GetDetectionRadius();

	DrawDebugSphere(World, Center, Radius, 24, BDAgentPrivate::RangeColor,
		BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, 1.0f);
	DrawDebugCircle(World, Center, Radius, 64, BDAgentPrivate::RangeColor,
		BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority,
		6.0f, FVector::ForwardVector, FVector::RightVector, /*bDrawAxis*/ false);
}

//~ Debug ------------------------------------------------------------------------

void ABDAgent::DebugSetPatrolRemaining(const float Seconds)
{
	if (State == EBDAgentState::Sleeping || State == EBDAgentState::Returning)
	{
		return;
	}
	PatrolRemaining = FMath::Max(0.0f, Seconds);
	if (PatrolRemaining <= 0.0f)
	{
		StartReturn();
	}
}

void ABDAgent::DebugWake()
{
	KickRemaining = 0.0f;
	if (State == EBDAgentState::Returning)
	{
		Route.Reset();
	}
	WakeUp();
}

FString ABDAgent::Describe() const
{
	const FBDAgentWeapon* Weapon = GetWeapon();
	return FString::Printf(TEXT("%s of %s: %s at %s (cell %s), patrol %.1fs (%.0f%%), %s%s, %d shot(s), %d kill(s), +%.2fs earned, target %s"),
		*GetName(), *GetNameSafe(Palace.Get()), *StaticEnum<EBDAgentState>()->GetNameStringByValue(static_cast<int64>(State)),
		*GetActorLocation().ToCompactString(), *HeadingCell.ToString(), PatrolRemaining, GetPatrolFraction() * 100.0f,
		Weapon != nullptr ? *Weapon->Name.ToString() : TEXT("no weapon"), IsKicking() ? TEXT(", kicking") : TEXT(""),
		ShotsFired, Kills, BonusEarned, *GetNameSafe(CurrentTarget.Get()));
}

namespace BDAgentDebug
{
	template <typename FunctionType>
	static int32 ForEachAgent(UWorld* World, FunctionType&& Function)
	{
		int32 Count = 0;
		if (World != nullptr)
		{
			for (TActorIterator<ABDAgent> It(World); It; ++It)
			{
				Function(**It);
				++Count;
			}
		}
		return Count;
	}

	static void ExecStatus(const TArray<FString>& Args, UWorld* World)
	{
		const int32 Count = ForEachAgent(World, [](ABDAgent& Agent) { UE_LOG(LogBDTower, Log, TEXT("  %s"), *Agent.Describe()); });
		const UBDShotSoundSubsystem* Shots = World != nullptr ? World->GetSubsystem<UBDShotSoundSubsystem>() : nullptr;
		UE_LOG(LogBDTower, Log, TEXT("BD.Agent.Status: %d agent(s) on the board. Shot sounds: %d asked, %d played, %d silent (empty slot)."), Count,
			Shots != nullptr ? Shots->GetShotsRequested() : 0, Shots != nullptr ? Shots->GetShotsPlayed() : 0, Shots != nullptr ? Shots->GetShotsSilent() : 0);
	}

	static void ExecKick(const TArray<FString>& Args, UWorld* World)
	{
		int32 Kicked = 0;
		const int32 Count = ForEachAgent(World, [&Kicked](ABDAgent& Agent) { Kicked += Agent.Kick() ? 1 : 0; });
		UE_LOG(LogBDTower, Log, TEXT("BD.Agent.Kick: %d of %d agent(s) kicking."), Kicked, Count);
	}

	static void ExecSetTime(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogBDTower, Error, TEXT("Usage: BD.Agent.SetTime <seconds>. 0 sends him home to sleep."));
			return;
		}
		const float Seconds = FCString::Atof(*Args[0]);
		const int32 Count = ForEachAgent(World, [Seconds](ABDAgent& Agent) { Agent.DebugSetPatrolRemaining(Seconds); });
		UE_LOG(LogBDTower, Log, TEXT("BD.Agent.SetTime: %d agent(s) set to %.1fs."), Count, Seconds);
	}

	static void ExecWake(const TArray<FString>& Args, UWorld* World)
	{
		const int32 Count = ForEachAgent(World, [](ABDAgent& Agent) { Agent.DebugWake(); });
		UE_LOG(LogBDTower, Log, TEXT("BD.Agent.Wake: %d agent(s) up on a full bar."), Count);
	}

	static FAutoConsoleCommandWithWorldAndArgs StatusCommand(TEXT("BD.Agent.Status"),
		TEXT("Lists every agent with his state, patrol time, weapon and kills."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));

	static FAutoConsoleCommandWithWorldAndArgs KickCommand(TEXT("BD.Agent.Kick"),
		TEXT("Plays the kick on every agent awake. Nothing to kick yet: the ministers come later."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecKick));

	static FAutoConsoleCommandWithWorldAndArgs SetTimeCommand(TEXT("BD.Agent.SetTime"),
		TEXT("BD.Agent.SetTime <seconds>: patrol time left on every agent awake. 0 sends them home to sleep."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSetTime));

	static FAutoConsoleCommandWithWorldAndArgs WakeCommand(TEXT("BD.Agent.Wake"),
		TEXT("Wakes every agent on a full bar."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecWake));
}

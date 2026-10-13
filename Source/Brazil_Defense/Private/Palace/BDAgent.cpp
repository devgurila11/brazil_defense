// Brazil Defense. The Agent: the one man the palace sends out on patrol.

#include "Palace/BDAgent.h"

#include "Animation/AnimSequenceBase.h"
#include "BDLog.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Sound/SoundBase.h"
#include "Enemy/BDCreepSoundSubsystem.h"
#include "Enemy/BDEnemyBase.h"
#include "Grid/BDFreeWalk.h"
#include "Grid/BDGridDebug.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Match/BDMatchManager.h"
#include "Match/BDMatchTypes.h"
#include "Palace/BDPalace.h"
#include "Palace/BDAnimNotify_Shot.h"
#include "Palace/BDMinister.h"
#include "Palace/BDMinisterSettings.h"
#include "Palace/BDMinisterSubsystem.h"
#include "Palace/BDPalaceData.h"
#include "Placement/BDInspection.h"
#include "Tower/BDShotSound.h"
#include "Wave/BDWaveSettings.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDAgentPrivate
{
	static int32 GShowRange = 1;
	static FAutoConsoleVariableRef CVarShowRange(
		TEXT("BD.Agent.ShowRange"),
		GShowRange,
		TEXT("1 (default) draws the Agent's detection radius when he or his palace is the piece selected; 2 always; 0 never."));

	static int32 GShowShots = 1;
	static FAutoConsoleVariableRef CVarShowShots(
		TEXT("BD.Agent.ShowShots"),
		GShowShots,
		TEXT("1 flashes a line from the Agent to his target at every shot, gone a moment later. 0 hides them."));

	// Colours of his own, apart from the towers' (cyan reach, red target, yellow next level).
	static const FColor RangeColor(90, 230, 110, 255);

	/** His reach for ministers, outside the green one while any stands on the board. */
	static const FColor MinisterReachColor(40, 40, 40, 255);
	static const FColor ShotColor(255, 60, 220, 255);

	/**
	 * How long the line of a shot stays on screen, in seconds: a flash, not a beam. A shot
	 * is a discrete thing; a weapon that fires continuously (a laser, a burst) will draw a
	 * steady line instead, so the two read apart at a glance.
	 */
	static constexpr float ShotFlashLife = 0.06f;

	/** Where on a creep the shot lands, over its location. */
	static constexpr float TargetChestHeight = 100.0f;

	/** Draws for a patrol point before he gives up and stands still a while. */
	static constexpr int32 WalkDraws = 16;

	/** How quickly an eased turn closes in, per second. The turn rate still caps it. */
	static constexpr float TurnEase = 8.0f;

	/** How quickly his height follows the floor under him, per second. */
	static constexpr float FloorFollow = 15.0f;

	/** Off his heading by this much he walks at full pace; by the second, he barely moves and turns. */
	static constexpr float FullPaceOff = 20.0f;
	static constexpr float TurnInPlaceOff = 100.0f;
	static constexpr float TurnInPlacePace = 0.1f;
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

	HeldWeapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeldWeapon"));
	HeldWeapon->SetupAttachment(Body);
	HeldWeapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeldWeapon->SetGenerateOverlapEvents(false);
	HeldWeapon->SetCanEverAffectNavigation(false);
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

float ABDAgent::GetMinisterReachRadius() const
{
	const UBDGridSubsystem* Grid = GetGrid();
	const UBDMinisterSubsystem* Ministers = UBDMinisterSubsystem::Get(this);
	const int32 Standing = Ministers != nullptr ? Ministers->CountStanding() : 0;
	return GetDetectionRadius() + (Grid != nullptr ? UBDMinisterSettings::Get().AgentReachStepCells * Standing * Grid->GetCellSize() : 0.0f);
}

float ABDAgent::GetPatrolFraction() const
{
	const UBDPalaceData* Data = GetData();
	return Data != nullptr && Data->PatrolTime > 0.0f ? FMath::Clamp(PatrolRemaining / Data->PatrolTime, 0.0f, 1.0f) : 0.0f;
}

float ABDAgent::GetBarFraction() const
{
	if (State == EBDAgentState::Sleeping)
	{
		return RestDuration > 0.0f ? FMath::Clamp(RestElapsed / RestDuration, 0.0f, 1.0f) : 1.0f;
	}
	return GetPatrolFraction();
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
			BodyBaseZ = -Bounds.Min.Z * Data->AgentMeshScale;
			Body->SetRelativeLocation(FVector(0.0f, 0.0f, BodyBaseZ));
		}
		else
		{
			UE_LOG(LogBDTower, Error, TEXT("%s: agent mesh %s of %s failed to load."), *GetName(), *Data->AgentMesh.ToString(), *Data->GetName());
		}
		ApplyWeapon();
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

FBDFreeWalk ABDAgent::Walk() const
{
	// The floor trace looks through him and through his palace.
	return FBDFreeWalk(GetWorld(), GetGrid(), { this, Palace.Get() });
}

FVector ABDAgent::CellPoint(const FBDCellCoord& Coord) const
{
	return GetGrid() != nullptr ? Walk().CellPoint(Coord) : GetActorLocation();
}

bool ABDAgent::TraceGround(const FVector& Point, float& OutGroundZ) const
{
	return Walk().TraceGround(Point, OutGroundZ);
}

bool ABDAgent::CanStand(const FBDCellCoord& Coord) const
{
	return Walk().CanStand(Coord);
}

bool ABDAgent::CanStep(const FBDCellCoord& From, const FBDCellCoord& To) const
{
	return Walk().CanStep(From, To);
}

bool ABDAgent::IsLineClear(const FVector& From, const FVector& To, const float Margin) const
{
	return Walk().IsLineClear(From, To, Margin);
}

TArray<FVector> ABDAgent::PlanWay(const FVector& Goal) const
{
	return Walk().PlanWay(GetActorLocation(), Goal);
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
	return Walk().FindWay(From, To);
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
	const UBDGridSubsystem* Grid = GetGrid();
	const float MinCells = Data != nullptr ? static_cast<float>(FMath::Max(1, Data->WalkCellsMin)) : 1.0f;
	const float MaxCells = Data != nullptr ? static_cast<float>(FMath::Max(Data->WalkCellsMin, Data->WalkCellsMax)) : 1.0f;

	// A point at any angle, any distance in the range, straight there. Each draw that runs
	// into a divider or the scenery is thrown away; first with room for his shoulders, then,
	// tight against something, without it, so he never stays boxed in where he stands.
	if (Grid != nullptr)
	{
		const FVector Here = GetActorLocation();
		const float Margin = Grid->GetCellSize() * FBDFreeWalk::BodyMargin;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (int32 Draw = 0; Draw < BDAgentPrivate::WalkDraws; ++Draw)
			{
				const float Yaw = FMath::FRandRange(0.0f, 360.0f);
				const float Distance = FMath::FRandRange(MinCells, FMath::Max(MinCells, MaxCells)) * Grid->GetCellSize();
				FVector There = Here + FRotator(0.0f, Yaw, 0.0f).Vector() * Distance;
				if (!IsLineClear(Here, There, Pass == 0 ? Margin : 0.0f))
				{
					continue;
				}
				float GroundZ = 0.0f;
				There.Z = TraceGround(There, GroundZ) ? GroundZ : Here.Z;

				Route.Reset();
				Route.Add(There);
				State = EBDAgentState::Walking;
				PlayStateAnimation();
				return;
			}
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
	PatrolRemaining = 0.0f;
	bGoingHome = true;
	bWayHomeLost = false;
	CurrentTarget.Reset();

	if (!PlanWayHome())
	{
		UE_LOG(LogBDTower, Warning, TEXT("%s: no way home from %s, resting where he stands."), *GetName(), *HeadingCell.ToString());
		Route.Reset();
		StartSleep();
		return;
	}

	UE_LOG(LogBDTower, Log, TEXT("%s: patrol time over after %d kill(s), %d straight leg(s) home."), *GetName(), KillsOnPatrol, Route.Num());
	State = EBDAgentState::Returning;
	PlayStateAnimation();
}

bool ABDAgent::PlanWayHome()
{
	// From wherever he stands, not from a cell: the way is planned in the world.
	FBDCellCoord Home;
	TArray<FVector> Way = FindSleepCell(Home) ? PlanWay(CellPoint(Home)) : TArray<FVector>();
	if (Way.Num() == 0)
	{
		return false;
	}
	Route = MoveTemp(Way);
	return true;
}

void ABDAgent::StartSleep()
{
	Route.Reset();
	CurrentTarget.Reset();
	bGoingHome = false;
	bWayHomeLost = false;
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

	// The better the patrol, the shorter the rest.
	const UBDPalaceData* Data = GetData();
	RestDuration = Data != nullptr ? Data->GetRestTime(KillsOnPatrol) : 0.0f;
	RestElapsed = 0.0f;
	PlayStateAnimation();
	UE_LOG(LogBDTower, Log, TEXT("%s lies down at %s: %d kill(s) on the patrol, %.1fs of rest."), *GetName(), *HeadingCell.ToString(), KillsOnPatrol, RestDuration);
}

void ABDAgent::WakeUp()
{
	const UBDPalaceData* Data = GetData();
	PatrolRemaining = Data != nullptr ? Data->PatrolTime : 0.0f;
	KillsOnPatrol = 0;
	bGoingHome = false;
	RestDuration = 0.0f;
	RestElapsed = 0.0f;
	UE_LOG(LogBDTower, Log, TEXT("%s up: %.0fs of patrol."), *GetName(), PatrolRemaining);
	StartWalk();
}

bool ABDAgent::StepAlong(const float DeltaSeconds, const float SpeedScale)
{
	const UBDPalaceData* Data = GetData();
	const UBDGridSubsystem* Grid = GetGrid();
	if (Route.Num() == 0 || Data == nullptr || Grid == nullptr)
	{
		return true;
	}

	const FVector Here = GetActorLocation();
	const FVector ToThere(Route[0].X - Here.X, Route[0].Y - Here.Y, 0.0f);
	const float Distance = ToThere.Size();
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		Route.RemoveAt(0);
		return Route.Num() == 0;
	}

	// The body turns to where he walks, eased; well off his heading he mostly turns before
	// he goes, so he never walks sideways or backwards into a new direction.
	const FVector Direction = ToThere / Distance;
	const float WantedYaw = Direction.ToOrientationRotator().Yaw;
	TurnTowards(WantedYaw, DeltaSeconds, 0.0f, /*bEased*/ true);
	const float Off = FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, WantedYaw));
	const float Pace = FMath::GetMappedRangeValueClamped(FVector2D(BDAgentPrivate::FullPaceOff, BDAgentPrivate::TurnInPlaceOff),
		FVector2D(1.0f, BDAgentPrivate::TurnInPlacePace), Off);
	const float Budget = Data->WalkSpeed * SpeedScale * Pace * Grid->GetCellSize() * DeltaSeconds;

	// On the floor under him wherever he steps, the height eased so a kerb is a step up and
	// not a jump.
	const bool bArrives = Distance <= Budget;
	FVector Next = bArrives ? FVector(Route[0].X, Route[0].Y, Here.Z) : Here + Direction * Budget;
	float GroundZ = 0.0f;
	if (TraceGround(Next, GroundZ))
	{
		Next.Z = FMath::FInterpTo(Here.Z, GroundZ, DeltaSeconds, BDAgentPrivate::FloorFollow);
	}
	SetActorLocation(Next);
	Grid->WorldToCell(Next, HeadingCell);

	if (!bArrives)
	{
		return false;
	}
	Route.RemoveAt(0);
	return Route.Num() == 0;
}

bool ABDAgent::TurnTowards(const float WantedYaw, const float DeltaSeconds, const float Tolerance, const bool bEased)
{
	const UBDPalaceData* Data = GetData();
	const float Rate = Data != nullptr ? Data->TurnRate : 540.0f;
	const float CurrentYaw = GetActorRotation().Yaw;
	float NewYaw = 0.0f;
	if (bEased)
	{
		const float MaxStep = Rate * DeltaSeconds;
		const float Delta = FMath::FindDeltaAngleDegrees(CurrentYaw, WantedYaw);
		NewYaw = CurrentYaw + FMath::Clamp(Delta * (1.0f - FMath::Exp(-BDAgentPrivate::TurnEase * DeltaSeconds)), -MaxStep, MaxStep);
	}
	else
	{
		NewYaw = FMath::FixedTurn(CurrentYaw, WantedYaw, Rate * DeltaSeconds);
	}
	SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
	return FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, WantedYaw)) <= Tolerance + KINDA_SMALL_NUMBER;
}

//~ Combat -----------------------------------------------------------------------

bool ABDAgent::IsValidTarget(const ABDEnemyBase* Enemy, const float RadiusSquared) const
{
	return Enemy != nullptr && IsValid(Enemy) && !Enemy->HasArrived() && !Enemy->IsDoomed()
		&& Enemy->GetCurrentHealth() > 0.0f && !Enemy->IsUntouchable()
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
	bool bBestIsCandidate = false;
	for (ABDEnemyBase* Enemy : Waves->GetLivingEnemiesRef())
	{
		if (!IsValidTarget(Enemy, RadiusSquared))
		{
			continue;
		}
		// A candidate in reach outranks every militant.
		const bool bCandidate = Enemy->IsCandidate();
		if (bBestIsCandidate && !bCandidate)
		{
			continue;
		}
		const float Score = Data->bTargetFurthestAlong
			? static_cast<float>(Enemy->GetCurrentPathIndex())
			: -FVector::DistSquared2D(Enemy->GetActorLocation(), GetActorLocation());
		if (Score > BestScore || (bCandidate && !bBestIsCandidate))
		{
			Best = Enemy;
			BestScore = Score;
			bBestIsCandidate = bCandidate;
		}
	}
	return Best;
}

ABDEnemyBase* ABDAgent::ChooseShotTarget(const float RadiusSquared)
{
	ABDEnemyBase* Held = CurrentTarget.Get();
	if (!IsValidTarget(Held, RadiusSquared))
	{
		Held = nullptr;
	}

	ABDEnemyBase* Best = AcquireTarget(RadiusSquared);
	const FBDAgentWeapon* Weapon = GetWeapon();
	const UBDPalaceData* Data = GetData();
	if (Held == nullptr || Best == nullptr || Best == Held)
	{
		return Held != nullptr ? Held : Best;
	}

	// A candidate coming into reach is taken whatever the weapon.
	if (Best->IsCandidate() && !Held->IsCandidate())
	{
		return Best;
	}
	if (Weapon == nullptr || Weapon->AimMode == EBDAimMode::Locked || Data == nullptr || Data->bTargetFurthestAlong)
	{
		return Held;
	}

	// Shot by shot: the nearest now, but only when clearly nearer than the one held, or he
	// would stutter between two creeps at almost the same distance.
	const float HeldDistance = FVector::Dist2D(Held->GetActorLocation(), GetActorLocation());
	const float BestDistance = FVector::Dist2D(Best->GetActorLocation(), GetActorLocation());
	return BestDistance < HeldDistance * (1.0f - Data->AimSwitchMargin) ? Best : Held;
}

ABDEnemyBase* ABDAgent::FindCandidate() const
{
	const UWorld* World = GetWorld();
	const UBDWaveSubsystem* Waves = World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr;
	if (Waves == nullptr)
	{
		return nullptr;
	}

	ABDEnemyBase* Nearest = nullptr;
	float NearestDistance = MAX_flt;
	for (ABDEnemyBase* Enemy : Waves->GetLivingEnemiesRef())
	{
		// A candidate making his speech is not there for him: he patrols on as if he were not.
		if (Enemy == nullptr || !IsValid(Enemy) || !Enemy->IsCandidate() || Enemy->HasArrived() || Enemy->GetCurrentHealth() <= 0.0f
			|| Enemy->IsRallyImmune())
		{
			continue;
		}
		const float Distance = FVector::DistSquared2D(Enemy->GetActorLocation(), GetActorLocation());
		if (Distance < NearestDistance)
		{
			Nearest = Enemy;
			NearestDistance = Distance;
		}
	}
	return Nearest;
}

void ABDAgent::ChaseTowards(const ABDEnemyBase* Candidate)
{
	const UBDGridSubsystem* Grid = GetGrid();
	FBDCellCoord Goal;
	if (Grid == nullptr || Candidate == nullptr || !Grid->WorldToCell(Candidate->GetActorLocation(), Goal))
	{
		return;
	}

	// In plain sight he runs straight at him, the line taken again every frame. Behind a
	// divider the way around is planned again only when the candidate changes cell.
	const FVector There = Candidate->GetActorLocation();
	if (IsLineClear(GetActorLocation(), There, 0.0f))
	{
		Route.Reset();
		Route.Add(There);
	}
	else if (State != EBDAgentState::Chasing || Goal != ChaseGoal || Route.Num() == 0)
	{
		Route = PlanWay(There);
	}
	ChaseGoal = Goal;
	if (State != EBDAgentState::Chasing)
	{
		UE_LOG(LogBDTower, Log, TEXT("%s runs after %s, %.1f cell(s) away, %d leg(s)."), *GetName(), *Candidate->GetName(),
			FVector::Dist2D(There, GetActorLocation()) / Grid->GetCellSize(), Route.Num());
		State = EBDAgentState::Chasing;
		PlayStateAnimation();
	}
}

bool ABDAgent::UsesShotTimer() const
{
	const UBDPalaceData* Data = GetData();
	return Data == nullptr || !UBDAnimNotify_Shot::IsOn(Data->ShootAnimation.Get());
}

void ABDAgent::ApplyWeapon()
{
	const UBDPalaceData* Data = GetData();
	const ABDPalace* Home = Palace.Get();
	HeldWeaponLevel = Home != nullptr ? Home->GetPalaceLevel() : 0;
	const FBDAgentWeapon* Entry = GetWeapon();
	if (Data == nullptr || Entry == nullptr)
	{
		HeldWeapon->SetStaticMesh(nullptr);
		return;
	}

	const TSoftObjectPtr<UStaticMesh> MeshRef = Data->ResolveWeaponMesh(HeldWeaponLevel);
	UStaticMesh* WeaponMesh = MeshRef.LoadSynchronous();
	if (!MeshRef.IsNull() && WeaponMesh == nullptr)
	{
		UE_LOG(LogBDTower, Error, TEXT("%s: weapon mesh %s failed to load."), *GetName(), *MeshRef.ToString());
	}
	HeldWeapon->SetStaticMesh(WeaponMesh);

	// In the hand when the body has the socket; at the body's root when it has not.
	const bool bHasSocket = Body->GetSkeletalMeshAsset() != nullptr && Body->DoesSocketExist(Data->HandSocket);
	HeldWeapon->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, bHasSocket ? Data->HandSocket : NAME_None);
	HeldWeapon->SetRelativeTransform(Entry->Grip);
	if (WeaponMesh != nullptr && !bHasSocket)
	{
		UE_LOG(LogBDTower, Warning, TEXT("%s: the body has no socket '%s' for the weapon; it hangs at the root. Make the socket in the skeleton or change HandSocket on %s."),
			*GetName(), *Data->HandSocket.ToString(), *Data->GetName());
	}
}

FVector ABDAgent::GetMuzzleLocation() const
{
	// The weapon's muzzle, then the hand, then a guess at the hand held out in front.
	const FBDAgentWeapon* Entry = GetWeapon();
	if (Entry != nullptr && HeldWeapon->GetStaticMesh() != nullptr && HeldWeapon->DoesSocketExist(Entry->MuzzleSocket))
	{
		return HeldWeapon->GetSocketLocation(Entry->MuzzleSocket);
	}
	const UBDPalaceData* Data = GetData();
	if (Data != nullptr && Body->GetSkeletalMeshAsset() != nullptr && Body->DoesSocketExist(Data->HandSocket))
	{
		return Body->GetSocketLocation(Data->HandSocket);
	}

	// About the hand, held out in front at chest height.
	const float Height = Body->GetSkeletalMeshAsset() != nullptr ? Body->Bounds.BoxExtent.Z * 1.2f : 200.0f;
	return GetActorLocation() + GetActorForwardVector() * 80.0f + FVector(0.0f, 0.0f, Height);
}

void ABDAgent::OnShotFrame()
{
	if (State != EBDAgentState::Shooting || KickRemaining > 0.0f)
	{
		return;
	}

	// Looked at again on every shot: the weapon decides whether the target may change.
	const float Radius = GetDetectionRadius();
	ABDEnemyBase* Target = ChooseShotTarget(Radius * Radius);
	const FBDAgentWeapon* Weapon = GetWeapon();
	if (Target == nullptr || Weapon == nullptr)
	{
		return;
	}
	if (Target != CurrentTarget.Get())
	{
		UE_LOG(LogBDTower, Verbose, TEXT("%s switches to %s."), *GetName(), *Target->GetName());
		CurrentTarget = Target;
	}

	// The gesture is the shot: he faces the target at once, so the line leaves the gun.
	SetActorRotation(FRotator(0.0f, (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().ToOrientationRotator().Yaw, 0.0f));
	Fire(Target, *Weapon);
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

	// The sound of the weapon, on the same instant as the flash and the gesture.
	if (UBDShotSoundSubsystem* Shots = World != nullptr ? World->GetSubsystem<UBDShotSoundSubsystem>() : nullptr)
	{
		const UBDPalaceData* Data = GetData();
		Shots->PlayShot(Data != nullptr ? Data->ResolveFireSound(HeldWeaponLevel).LoadSynchronous() : Weapon.FireSound.LoadSynchronous(), GetMuzzleLocation());
	}
	UE_LOG(LogBDTower, Verbose, TEXT("%s fires %s at %s: %.0f damage."), *GetName(), *Weapon.Name.ToString(), *Target->GetName(), Weapon.Damage);

	// How far the barrel points off the line to the target, for fitting a weapon's Grip.
	// Only on screen: off it the pose ticks without moving the bones, and the socket reads the reference pose.
	if (Body->WasRecentlyRendered() && HeldWeapon->GetStaticMesh() != nullptr && HeldWeapon->DoesSocketExist(Weapon.MuzzleSocket) && UE_LOG_ACTIVE(LogBDTower, Verbose))
	{
		const FTransform Muzzle = HeldWeapon->GetSocketTransform(Weapon.MuzzleSocket);
		const FRotator Barrel = Muzzle.GetUnitAxis(EAxis::X).Rotation();
		const FRotator Line = (Target->GetActorLocation() - Muzzle.GetLocation()).Rotation();
		UE_LOG(LogBDTower, Verbose, TEXT("%s: barrel off the line of fire by %.1f deg of yaw (+ right), %.1f of pitch."), *GetName(),
			FMath::FindDeltaAngleDegrees(Line.Yaw, Barrel.Yaw), FMath::FindDeltaAngleDegrees(Line.Pitch, Barrel.Pitch));
	}
	Target->ApplyDamage(Weapon.Damage, this);
}

void ABDAgent::NotifyKill()
{
	++Kills;
	++KillsOnPatrol;
	if (const UBDPalaceData* Voice = GetData())
	{
		if (UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(this))
		{
			Sounds->TryCelebrate(*this, Voice->AgentCelebrationSound);
		}
	}
	const FBDAgentWeapon* Weapon = GetWeapon();
	if (Weapon == nullptr || State == EBDAgentState::Sleeping || bGoingHome)
	{
		// On the way home the kill still shortens the rest; it does not refill the bar.
		return;
	}
	PatrolRemaining += Weapon->KillBonusSeconds;
	BonusEarned += Weapon->KillBonusSeconds;
}

bool ABDAgent::DebugCelebrate()
{
	const UBDPalaceData* Data = GetData();
	UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(this);
	return Data != nullptr && Sounds != nullptr
		&& Sounds->TryCelebrate(*this, Data->AgentCelebrationSound, /*Roll*/ 0.0f) == UBDCreepSoundSubsystem::ECelebration::Said;
}

bool ABDAgent::GoForMinister(const float DeltaSeconds, const float Radius)
{
	const UBDPalaceData* Data = GetData();
	const UBDMinisterSubsystem* Ministers = UBDMinisterSubsystem::Get(this);
	const UBDGridSubsystem* Grid = GetGrid();
	ABDMinister* Minister = Ministers != nullptr && Data != nullptr && Grid != nullptr ? Ministers->FindKickable(GetActorLocation(), Radius) : nullptr;
	if (Minister == nullptr)
	{
		return false;
	}
	CurrentTarget = nullptr;

	// Close enough: face him and kick. Not yet: run at him, the chase's way.
	const FVector ToHim = Minister->GetActorLocation() - GetActorLocation();
	const float Reach = UBDMinisterSettings::Get().KickReachCells * Grid->GetCellSize() + Minister->GetBodyRadius();
	if (ToHim.Size2D() <= Reach)
	{
		if (TurnTowards(ToHim.GetSafeNormal2D().ToOrientationRotator().Yaw, DeltaSeconds, Data->AimTolerance))
		{
			KickMinister(*Minister);
		}
		return true;
	}

	if (State != EBDAgentState::Chasing)
	{
		State = EBDAgentState::Chasing;
		PlayStateAnimation();
	}
	Route = PlanWay(Minister->GetActorLocation());
	if (Route.Num() == 0)
	{
		// No way to him: he gives up on this one for now.
		return false;
	}
	StepAlong(DeltaSeconds, Data->ChaseSpeedScale);
	return true;
}

bool ABDAgent::KickMinister(ABDMinister& Minister)
{
	if (!Minister.CanBeKicked() || !Kick())
	{
		return false;
	}
	SetActorRotation(FRotator(0.0f, (Minister.GetActorLocation() - GetActorLocation()).GetSafeNormal2D().ToOrientationRotator().Yaw, 0.0f));
	KickVictim = &Minister;
	KickImpactRemaining = FMath::Max(KINDA_SMALL_NUMBER, UBDMinisterSettings::Get().KickImpactSeconds);
	Route.Reset();
	UE_LOG(LogBDTower, Log, TEXT("%s kicks %s."), *GetName(), *Minister.GetName());
	return true;
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

	// Every kick has its line, whoever it lands on: the ministers will bring nothing more.
	if (UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(this))
	{
		Sounds->SayAlways(*this, Data->AgentKickSound);
	}
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
	case EBDAgentState::Chasing:
		PlayLoop(Data->WalkAnimation, Data->WalkAnimRate * Data->ChaseSpeedScale);
		break;
	case EBDAgentState::Idle:
		PlayLoop(Data->IdleAnimation);
		break;
	case EBDAgentState::Shooting:
	{
		// One gesture per shot: the loop runs at the weapon's rate, and the notify on it fires.
		UAnimSequenceBase* Shoot = Data->ShootAnimation.LoadSynchronous();
		const FBDAgentWeapon* Weapon = GetWeapon();
		const float Rate = Shoot != nullptr && Weapon != nullptr && !UsesShotTimer()
			? FMath::Max(0.1f, Weapon->FireRate * Shoot->GetPlayLength())
			: 1.0f;
		PlayLoop(Data->ShootAnimation, Rate);
		break;
	}
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

	if (Palace->GetPalaceLevel() != HeldWeaponLevel)
	{
		ApplyWeapon();
	}

	FireCooldown = FMath::Max(0.0f, FireCooldown - DeltaSeconds);

	// Lying down he rises off the floor a little, eased so the change of loop does not pop.
	const float BodyZ = FMath::FInterpTo(Body->GetRelativeLocation().Z,
		BodyBaseZ + (State == EBDAgentState::Sleeping ? Data->SleepLift : 0.0f), DeltaSeconds, 8.0f);
	Body->SetRelativeLocation(FVector(0.0f, 0.0f, BodyZ));

	// Asleep, the rest runs out on its own clock and the bar fills with it.
	if (State == EBDAgentState::Sleeping)
	{
		RestElapsed += DeltaSeconds;
		if (RestElapsed >= RestDuration)
		{
			WakeUp();
		}
		DrawDebug();
		return;
	}

	// The patrol clock runs always while awake, kick or not, until it sends him home.
	if (!bGoingHome)
	{
		PatrolRemaining -= DeltaSeconds;
		if (PatrolRemaining <= 0.0f)
		{
			StartReturn();
			if (State == EBDAgentState::Sleeping)
			{
				return;
			}
		}
	}

	// A kick holds him on the spot until it is over; on a minister, the foot lands partway.
	if (KickRemaining > 0.0f)
	{
		if (KickImpactRemaining > 0.0f)
		{
			KickImpactRemaining -= DeltaSeconds;
			ABDMinister* Victim = KickVictim.Get();
			if (KickImpactRemaining <= 0.0f && Victim != nullptr && Victim->CanBeKicked())
			{
				Victim->ReceiveKick(*this);
				++MinistersKicked;
				KickVictim = nullptr;
			}
		}
		KickRemaining = FMath::Max(0.0f, KickRemaining - DeltaSeconds);
		if (KickRemaining <= 0.0f)
		{
			PlayStateAnimation();
		}
		DrawDebug();
		return;
	}

	const float Radius = GetDetectionRadius();
	const float RadiusSquared = Radius * Radius;

	// A candidate on the board pulls him off the patrol; on the way home only a minister does.
	ABDEnemyBase* Candidate = bGoingHome ? nullptr : FindCandidate();

	// A minister in reach is what he leaves the militants for, short of a candidate, and
	// what he stops for even on an empty bar. That reach grows with every minister on his
	// feet out there.
	if (Candidate == nullptr && GoForMinister(DeltaSeconds, GetMinisterReachRadius()))
	{
		bWayHomeLost |= bGoingHome;
		DrawDebug();
		return;
	}

	// On the way home the horde is not his: he walks past it, the way planned again where
	// a minister took him off it, and lies down at the end of it.
	if (bGoingHome)
	{
		CurrentTarget.Reset();
		if (bWayHomeLost)
		{
			bWayHomeLost = false;
			if (!PlanWayHome())
			{
				UE_LOG(LogBDTower, Warning, TEXT("%s: no way home from %s after the minister, resting where he stands."), *GetName(), *HeadingCell.ToString());
				Route.Reset();
			}
		}
		if (State != EBDAgentState::Returning)
		{
			State = EBDAgentState::Returning;
			PlayStateAnimation();
		}
		if (StepAlong(DeltaSeconds))
		{
			StartSleep();
		}
		DrawDebug();
		return;
	}

	// Who he shoots: the one held while it lasts, the best in reach otherwise. While a
	// candidate is out he only stops for the candidate himself.
	ABDEnemyBase* Target = CurrentTarget.Get();
	if (!IsValidTarget(Target, RadiusSquared) || (Candidate != nullptr && !Target->IsCandidate()))
	{
		Target = AcquireTarget(RadiusSquared);
		if (Candidate != nullptr && Target != nullptr && !Target->IsCandidate())
		{
			Target = nullptr;
		}
		// A chased candidate is shot once well inside the reach, not on its very edge.
		if (Target != nullptr && Target->IsCandidate() && State == EBDAgentState::Chasing
			&& FVector::DistSquared2D(Target->GetActorLocation(), GetActorLocation()) > FMath::Square(Radius * Data->ChaseCloseIn))
		{
			Target = nullptr;
		}
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
		const bool bAligned = TurnTowards(WantedYaw, DeltaSeconds, Data->AimTolerance);

		// Without the shot notify on the animation, a timer at the weapon's rate stands in.
		const FBDAgentWeapon* Weapon = GetWeapon();
		if (UsesShotTimer() && bAligned && Weapon != nullptr && FireCooldown <= 0.0f)
		{
			OnShotFrame();
			FireCooldown = 1.0f / FMath::Max(Weapon->FireRate, KINDA_SMALL_NUMBER);
		}
		DrawDebug();
		return;
	}

	if (Candidate != nullptr)
	{
		ChaseTowards(Candidate);
		StepAlong(DeltaSeconds, Data->ChaseSpeedScale);
		DrawDebug();
		return;
	}

	// Reach clear and nobody to run after: on with the patrol.
	if (State == EBDAgentState::Shooting || State == EBDAgentState::Chasing)
	{
		// Back to the walk he was on, or a new decision. A chase leaves nothing to finish:
		// its points were after the candidate.
		if (State == EBDAgentState::Chasing)
		{
			Route.Reset();
		}
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

	DrawDebug();
}

void ABDAgent::DrawDebug() const
{
	UWorld* World = GetWorld();
	if (World == nullptr || State == EBDAgentState::Sleeping
		|| !UBDInspectionSubsystem::ShouldDrawReach(BDAgentPrivate::GShowRange, this, Palace.Get()))
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

	// His reach for ministers, wider by every one standing out there.
	const float MinisterReach = GetMinisterReachRadius();
	if (MinisterReach > Radius + 1.0f)
	{
		DrawDebugCircle(World, Center, MinisterReach, 64, BDAgentPrivate::MinisterReachColor,
			BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority,
			4.0f, FVector::ForwardVector, FVector::RightVector, /*bDrawAxis*/ false);
	}
}

//~ Debug ------------------------------------------------------------------------

void ABDAgent::DebugSetPatrolRemaining(const float Seconds)
{
	if (State == EBDAgentState::Sleeping || bGoingHome)
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
	Route.Reset();
	WakeUp();
}

FString ABDAgent::Describe() const
{
	const FBDAgentWeapon* Weapon = GetWeapon();
	return FString::Printf(TEXT("%s of %s: %s%s at %s (cell %s), patrol %.1fs, bar %.0f%%, rest %.1f/%.1fs, %s%s, %d shot(s), %d kill(s) (%d this patrol), +%.2fs earned, target %s"),
		*GetName(), *GetNameSafe(Palace.Get()), *StaticEnum<EBDAgentState>()->GetNameStringByValue(static_cast<int64>(State)),
		bGoingHome ? TEXT(" (going home)") : TEXT(""),
		*GetActorLocation().ToCompactString(), *HeadingCell.ToString(), PatrolRemaining, GetBarFraction() * 100.0f, RestElapsed, RestDuration,
		Weapon != nullptr ? *Weapon->Name.ToString() : TEXT("no weapon"), IsKicking() ? TEXT(", kicking") : TEXT(""),
		ShotsFired, Kills, KillsOnPatrol, BonusEarned, *GetNameSafe(CurrentTarget.Get()));
}

bool ABDAgent::MeasureFeet(float& OutLowestBoneOverGround, float& OutRootOverGround) const
{
	OutLowestBoneOverGround = OutRootOverGround = 0.0f;
	const UWorld* World = GetWorld();
	if (World == nullptr || Body->GetSkeletalMeshAsset() == nullptr || Body->GetNumBones() == 0)
	{
		return false;
	}

	float GroundZ = 0.0f;
	if (!TraceGround(GetActorLocation(), GroundZ))
	{
		return false;
	}

	const TArray<FTransform>& Pose = Body->GetComponentSpaceTransforms();
	const FTransform& ToWorld = Body->GetComponentTransform();
	float Lowest = TNumericLimits<float>::Max();
	int32 LowestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Pose.Num(); ++Index)
	{
		const float Z = ToWorld.TransformPosition(Pose[Index].GetLocation()).Z;
		if (Z < Lowest)
		{
			Lowest = Z;
			LowestIndex = Index;
		}
	}
	UE_LOG(LogBDTower, Verbose, TEXT("%s: lowest bone %s."), *GetName(), *Body->GetBoneName(LowestIndex).ToString());
	OutLowestBoneOverGround = Lowest - GroundZ;
	OutRootOverGround = GetActorLocation().Z - GroundZ;
	return true;
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

	static void ExecFeet(const TArray<FString>& Args, UWorld* World)
	{
		const int32 Count = ForEachAgent(World, [](ABDAgent& Agent)
		{
			float Feet = 0.0f, Root = 0.0f;
			const bool bMeasured = Agent.MeasureFeet(Feet, Root);
			UE_LOG(LogBDTower, Log, TEXT("BD.Agent.Feet: %s %s: lowest bone %+.1f cm over the ground, root %+.1f cm%s."), *Agent.GetName(),
				*StaticEnum<EBDAgentState>()->GetNameStringByValue(static_cast<int64>(Agent.GetState())), Feet, Root,
				bMeasured ? TEXT("") : TEXT(" (not measured)"));
		});
		UE_LOG(LogBDTower, Log, TEXT("BD.Agent.Feet: %d agent(s)."), Count);
	}

	static FAutoConsoleCommandWithWorldAndArgs FeetCommand(TEXT("BD.Agent.Feet"),
		TEXT("Logs how high every agent's lowest bone and root are over the ground under him."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecFeet));

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

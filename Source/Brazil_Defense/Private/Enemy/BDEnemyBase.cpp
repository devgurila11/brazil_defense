// Brazil Defense. A creep walking a route over the grid.

#include "Enemy/BDEnemyBase.h"

#include "BDLog.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemy/BDEnemyData.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Grid/BDGridSubsystem.h"
#include "Tower/BDTowerBase.h"
#include "Wave/BDWaveSettings.h"
#include "Wave/BDWaveSubsystem.h"

ABDEnemyBase::ABDEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// The root is the point on the floor; the mesh is rested on it whatever its pivot.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	// Queries only: towers will need to find creeps, but nothing may ever push one off
	// its route, and a creep must not stand in the way of the ground trace of a piece.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionObjectType(ECC_Pawn);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCanEverAffectNavigation(false);

	// A creep is nobody's pawn.
	AutoPossessAI = EAutoPossessAI::Disabled;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	SetCanBeDamaged(true);
}

UBDGridSubsystem* ABDEnemyBase::GetGrid() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
}

UBDWaveSubsystem* ABDEnemyBase::GetWaves() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr;
}

void ABDEnemyBase::InitializeEnemy(const UBDEnemyData* InData, const TArray<FBDCellCoord>& InPath, const float HealthScale)
{
	Data = InData;
	MaxHealth = Data != nullptr ? Data->MaxHealth * FMath::Max(0.0f, HealthScale) : 0.0f;
	CurrentHealth = MaxHealth;
	IncomingDamage = 0.0f;
	CurrentSpeed = 0.0f;
	bFinished = false;

	ApplyMesh();
	SetPath(InPath);

	if (Waypoints.Num() > 0)
	{
		// The creep stands on the spawn cell and heads for the next one. Facing the route
		// from the first frame, rather than turning into it, reads as coming out of the mouth.
		SetActorLocation(Waypoints[0]);
		CurrentPathIndex = FMath::Min(1, Path.Num());
		if (Waypoints.Num() > 1)
		{
			SetActorRotation((Waypoints[1] - Waypoints[0]).GetSafeNormal2D().ToOrientationRotator());
		}
	}
}

void ABDEnemyBase::SetPath(const TArray<FBDCellCoord>& InPath)
{
	Path = InPath;
	CurrentPathIndex = 0;
	RebuildWaypoints();
}

FBDCellCoord ABDEnemyBase::GetHeadingCell() const
{
	if (Path.IsValidIndex(CurrentPathIndex))
	{
		return Path[CurrentPathIndex];
	}

	if (Path.Num() > 0)
	{
		return Path.Last();
	}

	const UBDGridSubsystem* Grid = GetGrid();
	return Grid != nullptr ? Grid->WorldToCellUnclamped(GetActorLocation()) : FBDCellCoord();
}

void ABDEnemyBase::ApplyMesh()
{
	if (Data == nullptr || Data->Mesh.IsNull())
	{
		// A Blueprint child brings its own mesh; leave it where the designer put it.
		return;
	}

	UStaticMesh* LoadedMesh = Data->Mesh.LoadSynchronous();
	if (LoadedMesh == nullptr)
	{
		UE_LOG(LogBDWave, Error, TEXT("%s: mesh %s of %s failed to load."),
			*GetName(), *Data->Mesh.ToString(), *Data->GetName());
		return;
	}

	Mesh->SetStaticMesh(LoadedMesh);
	Mesh->SetRelativeScale3D(Data->MeshScale);

	// Rest the mesh on the root, so the root is the floor contact whatever the pivot of
	// the asset is: the engine cylinder is centered, an imported creep may not be.
	const FBox Bounds = LoadedMesh->GetBoundingBox();
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -Bounds.Min.Z * Data->MeshScale.Z));
}

FVector ABDEnemyBase::ResolveWaypoint(const FBDCellCoord& Coord) const
{
	const UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = GetGrid();
	if (World == nullptr || Grid == nullptr)
	{
		return GetActorLocation();
	}

	const FVector Center = Grid->CellToWorld(Coord);
	const UBDWaveSettings& Settings = UBDWaveSettings::Get();
	const FVector Start(Center.X, Center.Y, Center.Z + Settings.GroundTraceDistance);
	const FVector End(Center.X, Center.Y, Center.Z - Settings.GroundTraceDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BDEnemyGround), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(this);

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, Settings.GroundTraceChannel, Params))
	{
		return Hit.ImpactPoint;
	}

	return Center;
}

void ABDEnemyBase::RebuildWaypoints()
{
	Waypoints.Reset(Path.Num() + 1);
	for (const FBDCellCoord& Coord : Path)
	{
		Waypoints.Add(ResolveWaypoint(Coord));
	}

	// The route ends on a Goal cell; the creep does not. It walks on to the urn itself,
	// which stands half a cell off any cell center. Without an objective to walk to the
	// last cell is the end, and an empty route stays empty.
	UBDWaveSubsystem* Waves = GetWaves();
	FVector ObjectiveLocation;
	if (Path.Num() > 0 && Waves != nullptr && Waves->GetObjectiveLocation(ObjectiveLocation))
	{
		Waypoints.Add(ObjectiveLocation);
	}
}

void ABDEnemyBase::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFinished || Data == nullptr || !Waypoints.IsValidIndex(CurrentPathIndex))
	{
		return;
	}

	const UBDGridSubsystem* Grid = GetGrid();
	const float CellSize = Grid != nullptr ? Grid->GetCellSize() : 0.0f;

	// The data speaks in cells; the world in centimetres. DeltaSeconds already carries
	// the global time dilation, so the game speed needs no handling here.
	const float MaxSpeed = Data->MoveSpeed * CellSize;
	const float Acceleration = Data->Acceleration * CellSize;
	CurrentSpeed = Acceleration > 0.0f ? FMath::Min(MaxSpeed, CurrentSpeed + Acceleration * DeltaSeconds) : MaxSpeed;

	// Distance to cover this frame is spent waypoint by waypoint, so a fast creep on a
	// slow frame turns the corner where the route turns instead of overshooting it.
	float Budget = CurrentSpeed * DeltaSeconds;
	FVector Location = GetActorLocation();
	FVector Direction = GetActorForwardVector();

	const UBDWaveSettings& Settings = UBDWaveSettings::Get();

	while (Budget > 0.0f && Waypoints.IsValidIndex(CurrentPathIndex))
	{
		const FVector ToTarget = Waypoints[CurrentPathIndex] - Location;
		const float Distance = ToTarget.Size();

		// The last waypoint is the urn, and the urn has a body: close enough is there.
		if (CurrentPathIndex == Waypoints.Num() - 1 && Distance <= Settings.ArrivalDistance)
		{
			++CurrentPathIndex;
			break;
		}

		if (Distance <= Budget)
		{
			Location = Waypoints[CurrentPathIndex];
			Budget -= Distance;
			++CurrentPathIndex;
			continue;
		}

		Direction = ToTarget / Distance;
		Location += Direction * Budget;
		Budget = 0.0f;
	}

	SetActorLocation(Location);

	const FRotator WantedRotation = Direction.GetSafeNormal2D().ToOrientationRotator();
	SetActorRotation(Settings.TurnRate > 0.0f
		? FMath::RInterpConstantTo(GetActorRotation(), WantedRotation, DeltaSeconds, Settings.TurnRate)
		: WantedRotation);

	if (HasArrived())
	{
		Arrive();
	}
}

void ABDEnemyBase::Arrive()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	if (UBDWaveSubsystem* Waves = GetWaves())
	{
		Waves->NotifyEnemyArrived(this);
	}

	Destroy();
}

void ABDEnemyBase::ApplyDamage(const float Damage, AActor* Source)
{
	if (bFinished || Damage <= 0.0f)
	{
		return;
	}

	// Whatever the hit takes beyond what was left is overkill: damage the defense paid
	// for and got nothing from. The wave keeps the tally.
	const float Overkill = FMath::Max(0.0f, Damage - CurrentHealth);
	if (UBDWaveSubsystem* Waves = GetWaves())
	{
		Waves->ReportDamageDealt(Damage - Overkill);
		if (Overkill > 0.0f)
		{
			Waves->ReportWastedDamage(Overkill, /*bLostShot*/ false);
		}
	}

	CurrentHealth -= Damage;
	UE_LOG(LogBDWave, Verbose, TEXT("%s hit for %.0f by %s, %.0f health left."),
		*GetName(), Damage, *GetNameSafe(Source), CurrentHealth);

	if (CurrentHealth <= 0.0f)
	{
		CurrentHealth = 0.0f;
		if (ABDTowerBase* Tower = Cast<ABDTowerBase>(Source))
		{
			Tower->NotifyKill();
		}
		Die();
	}
}

void ABDEnemyBase::Kill()
{
	if (bFinished)
	{
		return;
	}

	CurrentHealth = 0.0f;
	Die();
}

void ABDEnemyBase::Die()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	if (UBDWaveSubsystem* Waves = GetWaves())
	{
		Waves->NotifyEnemyDied(this);
	}

	Destroy();
}

void ABDEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Covers a creep removed by anything other than itself: the level being torn down,
	// or a debug delete. Arrivals and kills already reported and are ignored here.
	if (UBDWaveSubsystem* Waves = GetWaves())
	{
		Waves->NotifyEnemyRemoved(this);
	}

	Super::EndPlay(EndPlayReason);
}

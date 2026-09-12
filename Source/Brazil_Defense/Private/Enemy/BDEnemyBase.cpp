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

namespace BDEnemyPrivate
{
	/** Straight segments a cut corner is drawn with. Four already reads as a curve at creep size. */
	static constexpr int32 CornerSegments = 4;

	/** A corner is never cut wider than this fraction of a cell, whatever the settings say. */
	static constexpr float MaxCornerRadiusFrac = 0.49f;

	/** Nor wider than this fraction of the shorter leg, so two turns in a row do not eat each other. */
	static constexpr float MaxCornerRadiusOfLeg = 0.45f;

	/** Above this dot product the route goes straight through a waypoint and there is no corner. */
	static constexpr float StraightDot = 0.99f;

	/**
	 * Cells one full swing of the lane wander takes, drawn per creep inside this range.
	 * Short swings read as jitter rather than as a creep choosing its way, and they bend
	 * the walked line hard enough on a straight run to be seen.
	 */
	static constexpr float MinWanderCells = 6.0f;
	static constexpr float MaxWanderCells = 14.0f;

	/** Seconds one breath of the pace takes, drawn per creep inside this range. */
	static constexpr float MinBreathSeconds = 6.0f;
	static constexpr float MaxBreathSeconds = 14.0f;

	/** The breath is half the spread the creeps already differ by: a pulse, not a second draw. */
	static constexpr float BreathOfVariance = 0.5f;

	/** Left of a direction on the grid plane. */
	static FVector Perpendicular(const FVector& Direction)
	{
		return FVector(-Direction.Y, Direction.X, 0.0f);
	}
}

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

	// Drawn once, before the route is laid out, and kept for the whole trip. Not seeded
	// off the match: where a single creep walks inside its corridor changes nothing that
	// a replay of a scenario has to reproduce.
	//
	// None of it is a single number held to the end. A creep that drew a lane and kept it
	// walks a line parallel to every other one, which is a column of ants, not a crowd.
	// What reads as a crowd is each one drifting across the corridor on a swing of its
	// own, taking its corners its own way and never quite holding its pace.
	const UBDWaveSettings& Settings = UBDWaveSettings::Get();
	Age = 0.0f;
	LateralOffsetFrac = FMath::FRandRange(-Settings.LateralOffsetMax, Settings.LateralOffsetMax);
	LaneWanderFrac = Settings.LateralOffsetMax * Settings.LaneWander;
	LaneWanderCells = FMath::FRandRange(BDEnemyPrivate::MinWanderCells, BDEnemyPrivate::MaxWanderCells);
	LaneWanderPhase = FMath::FRandRange(0.0f, UE_TWO_PI);
	CornerRadiusScale = 1.0f - FMath::FRandRange(0.0f, Settings.CornerRadiusVariance);
	SpeedScale = 1.0f + FMath::FRandRange(-Settings.SpeedVariance, Settings.SpeedVariance);
	SpeedBreathFrac = Settings.SpeedVariance * BDEnemyPrivate::BreathOfVariance;
	SpeedBreathPeriod = FMath::FRandRange(BDEnemyPrivate::MinBreathSeconds, BDEnemyPrivate::MaxBreathSeconds);
	SpeedBreathPhase = FMath::FRandRange(0.0f, UE_TWO_PI);

	ApplyMesh();
	SetPath(InPath);

	if (Waypoints.Num() > 0)
	{
		// The creep stands on the spawn cell and heads for the next one. Facing the route
		// from the first frame, rather than turning into it, reads as coming out of the mouth.
		SetActorLocation(Waypoints[0]);
		CurrentWaypoint = FMath::Min(1, Waypoints.Num());
		if (Waypoints.Num() > 1)
		{
			SetActorRotation((Waypoints[1] - Waypoints[0]).GetSafeNormal2D().ToOrientationRotator());
		}
	}
}

void ABDEnemyBase::SetPath(const TArray<FBDCellCoord>& InPath)
{
	Path = InPath;
	CurrentWaypoint = 0;
	RebuildWaypoints();
}

int32 ABDEnemyBase::GetCurrentPathIndex() const
{
	// The waypoints are denser than the cells: a cut corner is several points inside the
	// same one. Everything outside this class counts in cells.
	return WaypointPathIndex.IsValidIndex(CurrentWaypoint) ? WaypointPathIndex[CurrentWaypoint] : Path.Num();
}

FBDCellCoord ABDEnemyBase::GetHeadingCell() const
{
	const int32 PathIndex = GetCurrentPathIndex();
	if (Path.IsValidIndex(PathIndex))
	{
		return Path[PathIndex];
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

	// What the creep takes up on the floor, for the lateral offset to leave room against
	// a barrier. The wider of the two axes: a creep is never asked which way it faces.
	const FVector Extent = Bounds.GetExtent() * Data->MeshScale;
	BodyRadius = FMath::Max(Extent.X, Extent.Y);
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

bool ABDEnemyBase::IsCornerOpen(const int32 Index) const
{
	const UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr || !Path.IsValidIndex(Index - 1) || !Path.IsValidIndex(Index + 1))
	{
		return false;
	}

	const FBDCellCoord& Previous = Path[Index - 1];
	const FBDCellCoord& Corner = Path[Index];
	const FBDCellCoord& Next = Path[Index + 1];

	// The cell the shortcut runs past: the one touching both legs of the turn. The curve
	// itself stays inside the corner cell, but a turn taken around a fence or a platform
	// is a turn the board forced, and those are walked into and taken square.
	const FBDCellCoord Inner(Previous.X + Next.X - Corner.X, Previous.Y + Next.Y - Corner.Y);
	if (!Grid->IsWalkable(Inner))
	{
		return false;
	}

	return !Grid->IsEdgeBlocked(Previous, Inner)
		&& !Grid->IsEdgeBlocked(Inner, Next)
		&& !Grid->IsEdgeBlocked(Previous, Corner)
		&& !Grid->IsEdgeBlocked(Corner, Next);
}

float ABDEnemyBase::LaneFractionAt(const int32 Index) const
{
	const UBDWaveSettings& Settings = UBDWaveSettings::Get();
	if (LaneWanderFrac <= 0.0f || LaneWanderCells <= 0.0f)
	{
		return LateralOffsetFrac;
	}

	// One slow swing across the corridor, at its own wavelength and starting wherever its
	// phase says. Cell index rather than time, so the drift is a property of the route:
	// two creeps at the same spot of the same corridor are not obliged to be together.
	const float Swing = FMath::Sin(LaneWanderPhase + UE_TWO_PI * Index / LaneWanderCells);
	return FMath::Clamp(
		LateralOffsetFrac + LaneWanderFrac * Swing,
		-Settings.LateralOffsetMax,
		Settings.LateralOffsetMax);
}

FVector ABDEnemyBase::ComputeLateralOffset(const int32 Index, const TArray<FVector>& Centers, const float CellSize) const
{
	const UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr || CellSize <= 0.0f || !Path.IsValidIndex(Index))
	{
		return FVector::ZeroVector;
	}

	const float Wanted = LaneFractionAt(Index) * CellSize;
	if (FMath::IsNearlyZero(Wanted))
	{
		return FVector::ZeroVector;
	}

	// Against a barrier the offset is cut back to whatever room is left between the
	// middle of the cell and the wall, minus the body of the creep: the point of the
	// offset is a crowd filling a corridor, never a creep standing inside a fence.
	const float Tight = FMath::Sign(Wanted)
		* FMath::Min(FMath::Abs(Wanted), FMath::Max(0.0f, 0.5f * CellSize - BodyRadius));

	const FBDCellCoord& Cell = Path[Index];
	const auto OffsetAlong = [Grid, &Cell, Wanted, Tight](const FVector& Direction) -> FVector
	{
		const FVector Side = BDEnemyPrivate::Perpendicular(Direction) * FMath::Sign(Wanted);
		const FBDCellCoord Neighbour(Cell.X + FMath::RoundToInt(Side.X), Cell.Y + FMath::RoundToInt(Side.Y));
		const bool bRoom = Grid->IsWalkable(Neighbour) && !Grid->IsEdgeBlocked(Cell, Neighbour);
		return BDEnemyPrivate::Perpendicular(Direction) * (bRoom ? Wanted : Tight);
	};

	const FVector InDirection = Index > 0
		? (Centers[Index] - Centers[Index - 1]).GetSafeNormal2D()
		: FVector::ZeroVector;
	const FVector OutDirection = Centers.IsValidIndex(Index + 1)
		? (Centers[Index + 1] - Centers[Index]).GetSafeNormal2D()
		: FVector::ZeroVector;

	// Both legs of a turn have to end up the same distance from the middle of the route,
	// so the two offset lines are mitred at the corner: on a straight run the two
	// directions are the same one and the offset is applied once.
	FVector Offset = FVector::ZeroVector;
	if (!InDirection.IsNearlyZero())
	{
		Offset += OffsetAlong(InDirection);
	}
	if (!OutDirection.IsNearlyZero() && !OutDirection.Equals(InDirection))
	{
		Offset += OffsetAlong(OutDirection);
	}

	return Offset;
}

float ABDEnemyBase::ComputeCornerRadius(const int32 Index, const TArray<FVector>& Centers, const TArray<FVector>& Line, const float CellSize) const
{
	const UBDWaveSettings& Settings = UBDWaveSettings::Get();
	if (Settings.CornerSmoothRadius <= 0.0f || CellSize <= 0.0f || Index <= 0 || Index >= Line.Num() - 1)
	{
		return 0.0f;
	}

	// Asked of the route, not of the walked line: the wander bends the line a little at
	// every cell, and rounding those would turn a straight corridor into a chain of arcs.
	const FVector RouteIn = Centers[Index] - Centers[Index - 1];
	const FVector RouteOut = Centers[Index + 1] - Centers[Index];
	if (FVector::DotProduct(RouteIn.GetSafeNormal2D(), RouteOut.GetSafeNormal2D()) > BDEnemyPrivate::StraightDot)
	{
		return 0.0f;
	}

	const FVector In = Line[Index] - Line[Index - 1];
	const FVector Out = Line[Index + 1] - Line[Index];

	if (!IsCornerOpen(Index))
	{
		return 0.0f;
	}

	return FMath::Min3(
		Settings.CornerSmoothRadius * CornerRadiusScale * CellSize,
		BDEnemyPrivate::MaxCornerRadiusFrac * CellSize,
		BDEnemyPrivate::MaxCornerRadiusOfLeg * static_cast<float>(FMath::Min(In.Size2D(), Out.Size2D())));
}

void ABDEnemyBase::RebuildWaypoints()
{
	const UBDGridSubsystem* Grid = GetGrid();
	const float CellSize = Grid != nullptr ? Grid->GetCellSize() : 0.0f;

	// The floor under every cell center, traced once: the offset and the corners are all
	// worked out on this line, and the trace is the expensive part of the whole rebuild.
	TArray<FVector> Centers;
	Centers.Reserve(Path.Num());
	for (const FBDCellCoord& Coord : Path)
	{
		Centers.Add(ResolveWaypoint(Coord));
	}

	// The line this creep walks: the route pushed sideways by its own offset.
	TArray<FVector> Line;
	Line.Reserve(Centers.Num());
	for (int32 Index = 0; Index < Centers.Num(); ++Index)
	{
		Line.Add(Centers[Index] + ComputeLateralOffset(Index, Centers, CellSize));
	}

	Waypoints.Reset(Line.Num() + 1);
	WaypointPathIndex.Reset(Line.Num() + 1);

	for (int32 Index = 0; Index < Line.Num(); ++Index)
	{
		const float Radius = ComputeCornerRadius(Index, Centers, Line, CellSize);
		if (Radius <= 0.0f)
		{
			Waypoints.Add(Line[Index]);
			WaypointPathIndex.Add(Index);
			continue;
		}

		// The corner point itself is dropped and the turn is drawn as a quadratic curve
		// pulled towards it, from a radius short of the corner to a radius past it. It
		// never leaves the corner cell: the radius is under half a cell.
		const FVector In = (Line[Index] - Line[Index - 1]).GetSafeNormal2D();
		const FVector Out = (Line[Index + 1] - Line[Index]).GetSafeNormal2D();
		const FVector Start = Line[Index] - In * Radius;
		const FVector End = Line[Index] + Out * Radius;

		for (int32 Step = 0; Step <= BDEnemyPrivate::CornerSegments; ++Step)
		{
			const float Alpha = static_cast<float>(Step) / BDEnemyPrivate::CornerSegments;
			Waypoints.Add(FMath::Lerp(
				FMath::Lerp(Start, Line[Index], Alpha),
				FMath::Lerp(Line[Index], End, Alpha),
				Alpha));
			WaypointPathIndex.Add(Index);
		}
	}

	// The route ends on a Goal cell; the creep does not. It walks on to the urn itself,
	// which stands half a cell off any cell center. The offset is dropped there: they all
	// converge on the same point. Without an objective to walk to the last cell is the
	// end, and an empty route stays empty.
	UBDWaveSubsystem* Waves = GetWaves();
	FVector ObjectiveLocation;
	if (Path.Num() > 0 && Waves != nullptr && Waves->GetObjectiveLocation(ObjectiveLocation))
	{
		Waypoints.Add(ObjectiveLocation);
		WaypointPathIndex.Add(Path.Num());
	}

	// More waypoints than cells means corners were cut: the count is how the tuning of
	// CornerSmoothRadius is read back, and how a board that leaves no room for any curve
	// tells on itself.
	UE_LOG(LogBDWave, Verbose, TEXT("%s laid out %d waypoint(s) over %d cell(s), lane %+.2f wandering %.2f every %.1f cell(s), corner x%.2f, pace x%.2f."),
		*GetName(), Waypoints.Num(), Path.Num(), LateralOffsetFrac, LaneWanderFrac,
		LaneWanderCells, CornerRadiusScale, SpeedScale);
}

void ABDEnemyBase::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFinished || Data == nullptr || !Waypoints.IsValidIndex(CurrentWaypoint))
	{
		return;
	}

	const UBDGridSubsystem* Grid = GetGrid();
	const float CellSize = Grid != nullptr ? Grid->GetCellSize() : 0.0f;

	// The data speaks in cells; the world in centimetres. DeltaSeconds already carries
	// the global time dilation, so the game speed needs no handling here.
	Age += DeltaSeconds;

	// Every creep walks at its own pace around the one on the data, and that pace breathes
	// slowly around itself, so a wave arrives spread along the corridor instead of as a
	// block and the gaps in the file keep opening and closing.
	const float Breath = SpeedBreathPeriod > 0.0f
		? SpeedBreathFrac * FMath::Sin(SpeedBreathPhase + UE_TWO_PI * Age / SpeedBreathPeriod)
		: 0.0f;
	const float MaxSpeed = Data->MoveSpeed * CellSize * FMath::Max(0.1f, SpeedScale + Breath);
	const float Acceleration = Data->Acceleration * CellSize;
	CurrentSpeed = Acceleration > 0.0f ? FMath::Min(MaxSpeed, CurrentSpeed + Acceleration * DeltaSeconds) : MaxSpeed;

	// Distance to cover this frame is spent waypoint by waypoint, so a fast creep on a
	// slow frame turns the corner where the route turns instead of overshooting it.
	float Budget = CurrentSpeed * DeltaSeconds;
	FVector Location = GetActorLocation();
	FVector Direction = GetActorForwardVector();

	const UBDWaveSettings& Settings = UBDWaveSettings::Get();

	while (Budget > 0.0f && Waypoints.IsValidIndex(CurrentWaypoint))
	{
		const FVector ToTarget = Waypoints[CurrentWaypoint] - Location;
		const float Distance = ToTarget.Size();

		// The last waypoint is the urn, and the urn has a body: close enough is there.
		if (CurrentWaypoint == Waypoints.Num() - 1 && Distance <= Settings.ArrivalDistance)
		{
			++CurrentWaypoint;
			break;
		}

		if (Distance <= Budget)
		{
			Location = Waypoints[CurrentWaypoint];
			Budget -= Distance;
			++CurrentWaypoint;
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

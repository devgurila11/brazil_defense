// Brazil Defense. A minister out of the STF: he strolls the board, the Agent kicks him, he runs back.

#include "Palace/BDMinister.h"

#include "Audio/BDSpeechMarks.h"
#include "BDLog.h"
#include "Components/StaticMeshComponent.h"
#include "Enemy/BDCreepSoundSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Grid/BDFreeWalk.h"
#include "Grid/BDGridSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Palace/BDMinisterSettings.h"

namespace BDMinisterPrivate
{
	/** Draws for a stroll point before he stands still a while instead. */
	static constexpr int32 WalkDraws = 16;

	/** How quickly his feet follow the floor up a kerb, per second. */
	static constexpr float FloorFollow = 15.0f;

	/** Close enough to a point of his route to take the next. */
	static constexpr float ReachedDistance = 15.0f;
}

ABDMinister::ABDMinister()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Nothing collides with him and he collides with nothing; the cursor does not pick him either.
	Block = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Block"));
	Block->SetupAttachment(Root);
	Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Block->SetGenerateOverlapEvents(false);
	Block->SetCanEverAffectNavigation(false);
}

UBDGridSubsystem* ABDMinister::GetGrid() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
}

FBDFreeWalk ABDMinister::Walk() const
{
	// The floor trace looks through him and through the STF he walks out of.
	return FBDFreeWalk(GetWorld(), GetGrid(), { this, StfBuilding.Get() });
}

void ABDMinister::InitializeMinister(const FVector& Outside, const FBDCellCoord& InDoorCell, const AActor* Stf)
{
	const UBDMinisterSettings& Settings = UBDMinisterSettings::Get();
	StfBuilding = Stf;
	DoorOutside = Outside;
	DoorCell = InDoorCell;

	// The black block, resting on the floor whatever the mesh's pivot.
	if (UStaticMesh* Mesh = Settings.BlockMesh.LoadSynchronous())
	{
		Block->SetStaticMesh(Mesh);
		Block->SetRelativeScale3D(Settings.BlockScale);
		const FBox Bounds = Mesh->GetBoundingBox();
		BlockHalfHeight = Bounds.GetExtent().Z * Settings.BlockScale.Z;
		BlockHalfWidth = FMath::Max(Bounds.GetExtent().X * Settings.BlockScale.X, Bounds.GetExtent().Y * Settings.BlockScale.Y);
		// Pivot in the middle of the block, so it turns in the air about its centre.
		Block->SetRelativeLocation(FVector(0.0f, 0.0f, BlockHalfHeight - Bounds.GetCenter().Z * Settings.BlockScale.Z));
	}
	if (UMaterialInterface* Material = Settings.BlockMaterial.LoadSynchronous())
	{
		UMaterialInstanceDynamic* Robe = UMaterialInstanceDynamic::Create(Material, this);
		Robe->SetVectorParameterValue(TEXT("Color"), Settings.BlockColor);
		Block->SetMaterial(0, Robe);
	}

	// Out of the door and onto the board.
	SetActorLocation(DoorOutside);
	const FVector DoorPoint = Walk().CellPoint(DoorCell);
	Route = { DoorPoint };
	State = EBDMinisterState::Entering;
	SetActorRotation(FRotator(0.0f, (DoorPoint - DoorOutside).GetSafeNormal2D().ToOrientationRotator().Yaw, 0.0f));

	UE_LOG(LogBDMinister, Log, TEXT("Minister '%s' out of the STF at %s, onto the board at %s."), *GetName(), *DoorOutside.ToCompactString(), *DoorCell.ToString());
}

void ABDMinister::DebugPlaceOnBoard(const FVector& Where)
{
	FVector Point = Where;
	float GroundZ = 0.0f;
	if (Walk().TraceGround(Point, GroundZ))
	{
		Point.Z = GroundZ;
	}
	SetActorLocation(Point);
	Route.Reset();
	State = EBDMinisterState::Idle;
	IdleRemaining = 0.5f;
}

float ABDMinister::GetBodyRadius() const
{
	return BlockHalfWidth;
}

bool ABDMinister::CanBeKicked() const
{
	return State == EBDMinisterState::Walking || State == EBDMinisterState::Idle || State == EBDMinisterState::Entering;
}

//~ Walking ----------------------------------------------------------------------

bool ABDMinister::StepAlong(const float DeltaSeconds, const float SpeedScale)
{
	const UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr)
	{
		return true;
	}
	const UBDMinisterSettings& Settings = UBDMinisterSettings::Get();
	float Budget = Settings.WalkSpeed * SpeedScale * Grid->GetCellSize() * DeltaSeconds;
	FVector Here = GetActorLocation();

	while (Route.Num() > 0 && Budget > 0.0f)
	{
		const FVector To = Route[0];
		const FVector Flat(To.X - Here.X, To.Y - Here.Y, 0.0f);
		const float Distance = Flat.Size();
		if (Distance <= BDMinisterPrivate::ReachedDistance)
		{
			Route.RemoveAt(0);
			continue;
		}
		const float Move = FMath::Min(Budget, Distance);
		Here += Flat / Distance * Move;
		Budget -= Move;

		// Turned the way he goes, at his rate.
		const float Wanted = Flat.ToOrientationRotator().Yaw;
		const float Current = GetActorRotation().Yaw;
		const float MaxTurn = Settings.TurnRate * DeltaSeconds;
		SetActorRotation(FRotator(0.0f, Current + FMath::Clamp(FMath::FindDeltaAngleDegrees(Current, Wanted), -MaxTurn, MaxTurn), 0.0f));
	}

	// Feet on the floor, eased up a kerb.
	float GroundZ = 0.0f;
	if (Walk().TraceGround(Here, GroundZ))
	{
		Here.Z = FMath::FInterpTo(GetActorLocation().Z, GroundZ, DeltaSeconds, BDMinisterPrivate::FloorFollow);
	}
	SetActorLocation(Here);
	return Route.Num() == 0;
}

void ABDMinister::DecideNext()
{
	const UBDMinisterSettings& Settings = UBDMinisterSettings::Get();
	if (FMath::FRand() < Settings.IdleChance)
	{
		State = EBDMinisterState::Idle;
		IdleRemaining = FMath::FRandRange(Settings.IdleTimeMin, FMath::Max(Settings.IdleTimeMin, Settings.IdleTimeMax));
		return;
	}
	StartWalk();
}

void ABDMinister::StartWalk()
{
	const UBDGridSubsystem* Grid = GetGrid();
	const UBDMinisterSettings& Settings = UBDMinisterSettings::Get();
	if (Grid == nullptr)
	{
		return;
	}

	// A point a few cells off in any direction, in plain sight; the first draws keep a
	// margin from dividers, the last ones do without.
	const FBDFreeWalk Rules = Walk();
	const FVector Here = GetActorLocation();
	const float Margin = Grid->GetCellSize() * FBDFreeWalk::BodyMargin;
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		for (int32 Draw = 0; Draw < BDMinisterPrivate::WalkDraws; ++Draw)
		{
			const float Cells = FMath::FRandRange(static_cast<float>(Settings.WalkCellsMin), static_cast<float>(FMath::Max(Settings.WalkCellsMin, Settings.WalkCellsMax)));
			const FVector Direction = FRotator(0.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f).Vector();
			FVector There = Here + Direction * Cells * Grid->GetCellSize();
			FBDCellCoord Cell;
			if (!Grid->WorldToCell(There, Cell) || !Rules.IsLineClear(Here, There, Pass == 0 ? Margin : 0.0f))
			{
				continue;
			}
			float GroundZ = 0.0f;
			if (Rules.TraceGround(There, GroundZ))
			{
				There.Z = GroundZ;
			}
			Route = { There };
			State = EBDMinisterState::Walking;
			return;
		}
	}
	State = EBDMinisterState::Idle;
	IdleRemaining = Settings.IdleTimeMin;
}

//~ The kick ---------------------------------------------------------------------

void ABDMinister::ReceiveKick(const AActor& Kicker)
{
	if (!CanBeKicked())
	{
		return;
	}
	const UBDGridSubsystem* Grid = GetGrid();
	const UBDMinisterSettings& Settings = UBDMinisterSettings::Get();
	++KicksTaken;

	// Away from the foot that sent him, landing on floor he could stand on: as far as the
	// push goes, or shorter, or straight back down.
	const FBDFreeWalk Rules = Walk();
	FlightFrom = GetActorLocation();
	FlightTo = FlightFrom;
	FVector Away = (FlightFrom - Kicker.GetActorLocation()).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = GetActorForwardVector();
	}
	if (Grid != nullptr)
	{
		for (const float Share : { 1.0f, 0.5f, 0.25f })
		{
			FVector Landing = FlightFrom + Away * Settings.FlightPushCells * Share * Grid->GetCellSize();
			FBDCellCoord Cell;
			if (Grid->WorldToCell(Landing, Cell) && Rules.CanStand(Cell))
			{
				float GroundZ = 0.0f;
				Landing.Z = Rules.TraceGround(Landing, GroundZ) ? GroundZ : FlightFrom.Z;
				FlightTo = Landing;
				break;
			}
		}
	}

	Route.Reset();
	State = EBDMinisterState::Flying;
	StateSeconds = 0.0f;
	LastFlightPeak = 0.0f;
	Cry();
	UE_LOG(LogBDMinister, Log, TEXT("%s kicked by %s: up %.0f cm, landing %.0f cm away."), *GetName(), *Kicker.GetName(),
		Settings.FlightHeight, FVector::Dist2D(FlightFrom, FlightTo));
}

void ABDMinister::Cry()
{
	// His voice when he has one; the black exclamation either way, so who cried reads now.
	const UBDMinisterSettings& Settings = UBDMinisterSettings::Get();
	UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(this);
	const bool bVoiced = !Settings.ScreamSound.IsNull() && Sounds != nullptr && Sounds->SayAlways(*this, Settings.ScreamSound);
	if (!bVoiced)
	{
		if (UBDSpeechMarkSubsystem* Marks = UBDSpeechMarkSubsystem::Get(this))
		{
			Marks->NoteTimedMark(*this, EBDSpeakerSide::Minister, Settings.ScreamMarkSeconds);
		}
	}
}

void ABDMinister::SetLying(const bool bLying)
{
	// On his side: the block turned over, its long side on the floor.
	Block->SetRelativeRotation(bLying ? FRotator(90.0f, 0.0f, 0.0f) : FRotator::ZeroRotator);
	Block->SetRelativeLocation(FVector(0.0f, 0.0f, bLying ? BlockHalfWidth : BlockHalfHeight));
}

void ABDMinister::StartFlee()
{
	// Back to the door cell by the free walk, then out to the STF itself.
	const FBDFreeWalk Rules = Walk();
	const FVector DoorPoint = Rules.CellPoint(DoorCell);
	Route = Rules.PlanWay(GetActorLocation(), DoorPoint);
	if (Route.Num() == 0)
	{
		Route.Add(DoorPoint);
	}
	Route.Add(DoorOutside);
	State = EBDMinisterState::Fleeing;
	UE_LOG(LogBDMinister, Log, TEXT("%s gets up and runs for the STF: %d point(s)."), *GetName(), Route.Num());
}

//~ Tick -------------------------------------------------------------------------

void ABDMinister::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const UBDMinisterSettings& Settings = UBDMinisterSettings::Get();
	StateSeconds += DeltaSeconds;

	switch (State)
	{
	case EBDMinisterState::Entering:
		if (StepAlong(DeltaSeconds, 1.0f))
		{
			DecideNext();
		}
		break;

	case EBDMinisterState::Walking:
		if (StepAlong(DeltaSeconds, 1.0f))
		{
			DecideNext();
		}
		break;

	case EBDMinisterState::Idle:
		IdleRemaining -= DeltaSeconds;
		if (IdleRemaining <= 0.0f)
		{
			StartWalk();
		}
		break;

	case EBDMinisterState::Flying:
	{
		// An arc up and over, turning end over end.
		const float Alpha = FMath::Clamp(StateSeconds / Settings.FlightSeconds, 0.0f, 1.0f);
		const float Lift = 4.0f * Settings.FlightHeight * Alpha * (1.0f - Alpha);
		LastFlightPeak = FMath::Max(LastFlightPeak, Lift);
		SetActorLocation(FMath::Lerp(FlightFrom, FlightTo, Alpha) + FVector(0.0f, 0.0f, Lift));
		Block->SetRelativeRotation(FRotator(Settings.FlightSpinDegrees * Alpha, 0.0f, 0.0f));
		if (Alpha >= 1.0f)
		{
			SetActorLocation(FlightTo);
			SetLying(true);
			State = EBDMinisterState::Down;
			StateSeconds = 0.0f;
		}
		break;
	}

	case EBDMinisterState::Down:
		if (StateSeconds >= Settings.DownSeconds)
		{
			SetLying(false);
			StartFlee();
		}
		break;

	case EBDMinisterState::Fleeing:
		if (StepAlong(DeltaSeconds, Settings.FleeSpeedScale))
		{
			UE_LOG(LogBDMinister, Log, TEXT("%s is back in the STF."), *GetName());
			Destroy();
		}
		break;
	}
}

FString ABDMinister::Describe() const
{
	FBDCellCoord Cell;
	const UBDGridSubsystem* Grid = GetGrid();
	const bool bOnBoard = Grid != nullptr && Grid->WorldToCell(GetActorLocation(), Cell);
	return FString::Printf(TEXT("%s: %s at %s (%s), %d point(s) to go, %d kick(s) taken"), *GetName(),
		*StaticEnum<EBDMinisterState>()->GetNameStringByValue(static_cast<int64>(State)), *GetActorLocation().ToCompactString(),
		bOnBoard ? *Cell.ToString() : TEXT("off the board"), Route.Num(), KicksTaken);
}

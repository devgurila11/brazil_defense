// Brazil Defense. Putting the urn on the board: the Goal cell follows it.

#include "Objective/BDObjectiveSubsystem.h"

#include "BDLog.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Grid/BDGridDebug.h"
#include "Grid/BDGridSubsystem.h"
#include "Objective/BDObjective.h"
#include "Objective/BDObjectiveSettings.h"
#include "Path/BDPathfinder.h"
#include "Placement/BDPlacementSettings.h"

void UBDObjectiveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UBDGridSubsystem>();
	Collection.InitializeDependency<UBDPathfinder>();
}

UBDObjectiveSubsystem* UBDObjectiveSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	return World != nullptr ? World->GetSubsystem<UBDObjectiveSubsystem>() : nullptr;
}

UBDGridSubsystem* UBDObjectiveSubsystem::GetGrid() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
}

const UBDPathfinder* UBDObjectiveSubsystem::GetPathfinder() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDPathfinder>() : nullptr;
}

ABDObjective* UBDObjectiveSubsystem::GetObjective() const
{
	if (!Objective.IsValid())
	{
		Objective = ABDObjective::Get(GetWorld());
	}

	return Objective.Get();
}

FString UBDObjectiveSubsystem::DescribeRefusal(const EBDObjectiveRefusal Refusal)
{
	return StaticEnum<EBDObjectiveRefusal>()->GetNameStringByValue(static_cast<int64>(Refusal));
}

EBDObjectiveRefusal UBDObjectiveSubsystem::EvaluateCell(const FBDCellCoord& Coord) const
{
	const UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr || !Grid->IsValidCoord(Coord))
	{
		return EBDObjectiveRefusal::OffGrid;
	}

	if (!UBDObjectiveSettings::Get().IsInZone(Coord))
	{
		return EBDObjectiveRefusal::OutOfZone;
	}

	// Its own cell is fine again: moving the urn onto where it already stands is a no-op,
	// not a collision.
	const EBDCellState State = Grid->GetCellState(Coord);
	const bool bOwnCell = bPlaced && Coord == GoalCell && State == EBDCellState::Goal;
	if (!bOwnCell && !Grid->IsBuildable(Coord))
	{
		return EBDObjectiveRefusal::CellTaken;
	}

	const UBDPathfinder* Pathfinder = GetPathfinder();
	if (Pathfinder == nullptr || !Pathfinder->CanEverySpawnReach(Grid, Coord))
	{
		return EBDObjectiveRefusal::Unreachable;
	}

	return EBDObjectiveRefusal::None;
}

float UBDObjectiveSubsystem::ResolveGroundZ(const FVector& Point) const
{
	const UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = GetGrid();
	const float PlaneZ = Grid != nullptr ? Grid->GetOrigin().Z : Point.Z;
	if (World == nullptr)
	{
		return PlaneZ;
	}

	const UBDPlacementSettings& Settings = UBDPlacementSettings::Get();
	const FVector Start(Point.X, Point.Y, PlaneZ + Settings.GroundTraceDistance);
	const FVector End(Point.X, Point.Y, PlaneZ - Settings.GroundTraceDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BDObjectiveGround), /*bTraceComplex*/ false);
	if (const ABDObjective* Urn = GetObjective())
	{
		// The urn must not be measured against its own roof when it is only moving.
		Params.AddIgnoredActor(Urn);
	}

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, Settings.GroundTraceChannel, Params))
	{
		return Hit.ImpactPoint.Z;
	}

	return PlaneZ;
}

bool UBDObjectiveSubsystem::PlaceObjective(const FBDCellCoord& Coord, UClass* ActorClass, UStaticMesh* Mesh, EBDObjectiveRefusal& OutRefusal)
{
	OutRefusal = EvaluateCell(Coord);
	if (OutRefusal != EBDObjectiveRefusal::None)
	{
		UE_LOG(LogBDGrid, Warning, TEXT("Objective refused at %s: %s."), *Coord.ToString(), *DescribeRefusal(OutRefusal));
		return false;
	}

	UWorld* World = GetWorld();
	UBDGridSubsystem* Grid = GetGrid();
	check(World != nullptr && Grid != nullptr);

	// The actor first: a failed spawn must not leave a Goal with no urn on it.
	ABDObjective* Urn = GetObjective();
	if (Urn == nullptr)
	{
		UClass* SpawnClass = ActorClass != nullptr && ActorClass->IsChildOf<ABDObjective>()
			? ActorClass
			: ABDObjective::StaticClass();
		if (ActorClass != nullptr && SpawnClass != ActorClass)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Objective actor class %s is not a BD Objective; spawning the base class instead."),
				*ActorClass->GetName());
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Urn = World->SpawnActor<ABDObjective>(SpawnClass, FTransform::Identity, SpawnParams);
		if (Urn == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Failed to spawn the objective actor %s."), *SpawnClass->GetName());
			OutRefusal = EBDObjectiveRefusal::OffGrid;
			return false;
		}

		Objective = Urn;

		if (Mesh != nullptr && Urn->GetMesh() != nullptr && Urn->GetMesh()->GetStaticMesh() == nullptr)
		{
			Urn->GetMesh()->SetStaticMesh(Mesh);
		}
	}

	FVector Location = Grid->CellToWorld(Coord);
	Location.Z = ResolveGroundZ(Location);
	Urn->SetActorLocation(Location);

	// Then the grid. The previous Goal cells go back to Free whatever wrote them, the
	// authored layout included: the Goal is wherever the urn is, nowhere else.
	TArray<FBDCellCoord> PreviousGoals;
	UBDPathfinder::GatherCellsWithState(*Grid, EBDCellState::Goal, PreviousGoals);
	for (const FBDCellCoord& Previous : PreviousGoals)
	{
		if (Previous != Coord)
		{
			Grid->SetCellState(Previous, EBDCellState::Free);
		}
	}
	Grid->SetCellState(Coord, EBDCellState::Goal);

	GoalCell = Coord;
	bPlaced = true;

	UE_LOG(LogBDGrid, Log, TEXT("Objective placed at %s (%s), %d previous goal cell(s) freed."),
		*Coord.ToString(), *Location.ToCompactString(), PreviousGoals.Num());
	return true;
}

void UBDObjectiveSubsystem::DrawZone() const
{
	const UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = GetGrid();
	if (World == nullptr || Grid == nullptr || Grid->GetCellCount() <= 0)
	{
		return;
	}

	const UBDObjectiveSettings& Settings = UBDObjectiveSettings::Get();

	// Clipped to the grid, so a zone authored past the edge does not paint thin air.
	const FBDCellCoord Min(FMath::Max(0, Settings.MinX), FMath::Max(0, Settings.MinY));
	const FBDCellCoord Max(FMath::Min(Grid->GetSizeX() - 1, Settings.MaxX), FMath::Min(Grid->GetSizeY() - 1, Settings.MaxY));
	if (Min.X > Max.X || Min.Y > Max.Y)
	{
		return;
	}

	for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
	{
		for (int32 X = Min.X; X <= Max.X; ++X)
		{
			BDGridDebug::DrawCellFill(*World, *Grid, FBDCellCoord(X, Y), Settings.ZoneFillColor);
		}
	}

	BDGridDebug::DrawCellRectOutline(*World, *Grid, Min, Max, Settings.ZoneBorderColor, Settings.ZoneBorderThickness);
}

// Brazil Defense. The authored cell layout of a map, saved with the level.

#include "Grid/BDGridLayoutActor.h"

#include "BDLog.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Obstacle/BDObstacleGenerator.h"

ABDGridLayoutActor::ABDGridLayoutActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// The actor renders nothing: the root only gives it a transform and a selection
	// handle in the editor. Its position is irrelevant, the coordinates are the data.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SetHidden(true);
	SetCanBeDamaged(false);

#if WITH_EDITOR
	// See the class comment: streaming this actor would mean streaming the layout.
	bIsSpatiallyLoaded = false;
#endif
}

void ABDGridLayoutActor::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	if (IsTemplate())
	{
		return;
	}

	if (UBDGridSubsystem* Grid = GetGrid())
	{
		// Resizing the grid in Project Settings wipes every cell, so the layout has to
		// go back on afterwards or the level would silently lose its Spawn and Goal.
		GridRebuiltHandle = Grid->OnGridRebuilt.AddUObject(this, &ABDGridLayoutActor::HandleGridRebuilt);
	}

	ApplyToGrid();
}

void ABDGridLayoutActor::PostUnregisterAllComponents()
{
	if (GridRebuiltHandle.IsValid())
	{
		if (UBDGridSubsystem* Grid = GetGrid())
		{
			Grid->OnGridRebuilt.Remove(GridRebuiltHandle);
		}

		GridRebuiltHandle.Reset();
	}

	Super::PostUnregisterAllComponents();
}

UBDGridSubsystem* ABDGridLayoutActor::GetGrid() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
}

void ABDGridLayoutActor::HandleGridRebuilt()
{
	ApplyToGrid();
}

int32 ABDGridLayoutActor::ApplyToGrid()
{
	UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr)
	{
		return 0;
	}

	const int32 ChangedCells = Grid->ApplyAuthoredLayout(AuthoredCells);
	const int32 ChangedEdges = Grid->ApplyAuthoredEdges(AuthoredEdges);

	UE_LOG(LogBDGrid, Log, TEXT("%s applied %d authored cell(s) and %d edge(s) to the %s world grid (%d and %d changed)."),
		*GetName(), AuthoredCells.Num(), AuthoredEdges.Num(),
		GetWorld()->IsGameWorld() ? TEXT("game") : TEXT("editor"), ChangedCells, ChangedEdges);

	return ChangedCells + ChangedEdges;
}

#if WITH_EDITOR
int32 ABDGridLayoutActor::CaptureFromGrid(const UBDGridSubsystem& Grid)
{
	AuthoredCells.Reset();
	AuthoredEdges.Reset();

	// Generated obstacles are not part of the map. A statue, a lamp post or a planter is
	// authored once and is there every match; what the generator scatters belongs to a
	// seed and is handed back the moment the next one runs. The two share the Blocked
	// state because the A* has no reason to tell them apart, so what separates them is
	// ownership, and ownership is exactly what the generator already tracks.
	TSet<FBDCellCoord> GeneratedCells;
	if (const UWorld* World = GetWorld())
	{
		if (const UBDObstacleGenerator* Generator = World->GetSubsystem<UBDObstacleGenerator>())
		{
			GeneratedCells.Append(Generator->GetGeneratedCells());
		}
	}

	int32 SkippedCount = 0;

	for (int32 Y = 0; Y < Grid.GetSizeY(); ++Y)
	{
		for (int32 X = 0; X < Grid.GetSizeX(); ++X)
		{
			const FBDCellCoord Coord(X, Y);
			const EBDCellState State = Grid.GetCellState(Coord);
			if (State == EBDCellState::Free)
			{
				continue;
			}

			if (GeneratedCells.Contains(Coord))
			{
				++SkippedCount;
				continue;
			}

			AuthoredCells.Add(Coord, State);
		}
	}

	if (SkippedCount > 0)
	{
		UE_LOG(LogBDGrid, Log,
			TEXT("%d generated obstacle(s) left out of the capture: they belong to a seed, not to the map."),
			SkippedCount);
	}

	// Edges have no generator yet, so everything blocked is authored.
	Grid.GetBlockedEdges(AuthoredEdges);

	return AuthoredCells.Num() + AuthoredEdges.Num();
}
#endif

#if WITH_EDITOR
namespace BDGridLayoutCommands
{
	/**
	 * Captures whatever the editor grid currently holds into the layout actor, so a board
	 * painted with BD.Grid.SetCells becomes the layout of the map.
	 *
	 * Editor worlds only, and deliberately so. While PIE is running the editor console
	 * routes commands to the play world, and capturing that would save a snapshot of a
	 * match in progress into the level.
	 */
	static void ExecSaveLayout(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("BD.Grid.SaveLayout needs a world."));
			return;
		}

		if (World->IsGameWorld())
		{
			UE_LOG(LogBDGrid, Error,
				TEXT("BD.Grid.SaveLayout only runs on an editor world. Stop PIE first: while it runs "
					 "the console talks to the play world, and that state is not the map layout."));
			return;
		}

		const UBDGridSubsystem* Grid = World->GetSubsystem<UBDGridSubsystem>();
		if (Grid == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("BD.Grid.SaveLayout: no grid subsystem in this world."));
			return;
		}

		ABDGridLayoutActor* LayoutActor = nullptr;
		for (TActorIterator<ABDGridLayoutActor> It(World); It; ++It)
		{
			LayoutActor = *It;
			break;
		}

		const bool bSpawned = LayoutActor == nullptr;
		if (bSpawned)
		{
			LayoutActor = World->SpawnActor<ABDGridLayoutActor>();
			if (LayoutActor == nullptr)
			{
				UE_LOG(LogBDGrid, Error, TEXT("BD.Grid.SaveLayout: could not spawn a layout actor."));
				return;
			}

			LayoutActor->SetActorLabel(TEXT("BD Grid Layout"));
		}

		LayoutActor->Modify();
		const int32 CapturedCount = LayoutActor->CaptureFromGrid(*Grid);
		LayoutActor->MarkPackageDirty();

		UE_LOG(LogBDGrid, Log,
			TEXT("BD.Grid.SaveLayout: captured %d cell(s) and edge(s) into %s%s. Save the level to keep it."),
			CapturedCount, *LayoutActor->GetName(), bSpawned ? TEXT(" (newly created)") : TEXT(""));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSaveLayout(
		TEXT("BD.Grid.SaveLayout"),
		TEXT("BD.Grid.SaveLayout: stores every non Free cell and every blocked edge of the editor grid into the level layout actor."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSaveLayout));
}
#endif

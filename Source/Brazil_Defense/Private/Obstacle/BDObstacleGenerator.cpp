// Brazil Defense. Seeded, validated generation of the permanent obstacles of a board.

#include "Obstacle/BDObstacleGenerator.h"

#include "BDLog.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "Placement/BDPlacementSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Math/RandomStream.h"
#include "Objective/BDObjectiveSettings.h"
#include "Obstacle/BDObstacleSettings.h"
#include "Path/BDPathfinder.h"

UBDObstacleGenerator* UBDObstacleGenerator::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	return World != nullptr ? World->GetSubsystem<UBDObstacleGenerator>() : nullptr;
}

const UBDPathfinder* UBDObstacleGenerator::GetPathfinder() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDPathfinder>() : nullptr;
}

void UBDObstacleGenerator::BuildCandidates(const UBDGridSubsystem& Grid, const TArray<FBDCellCoord>& Spawns,
	const TArray<FBDCellCoord>& Goals, const int32 Clearance, TArray<FBDCellCoord>& OutCandidates)
{
	OutCandidates.Reset();

	auto IsTooClose = [Clearance](const FBDCellCoord& Cell, const TArray<FBDCellCoord>& Protected)
	{
		for (const FBDCellCoord& Other : Protected)
		{
			// Chebyshev distance: a square of free room around the cell, diagonals included.
			if (FMath::Max(FMath::Abs(Cell.X - Other.X), FMath::Abs(Cell.Y - Other.Y)) <= Clearance)
			{
				return true;
			}
		}

		return false;
	};

	for (int32 Y = 0; Y < Grid.GetSizeY(); ++Y)
	{
		for (int32 X = 0; X < Grid.GetSizeX(); ++X)
		{
			const FBDCellCoord Coord(X, Y);
			if (Grid.GetCellState(Coord) != EBDCellState::Free)
			{
				continue;
			}

			if (IsTooClose(Coord, Spawns) || IsTooClose(Coord, Goals))
			{
				continue;
			}

			OutCandidates.Add(Coord);
		}
	}
}

void UBDObstacleGenerator::PickCells(FRandomStream& Stream, const TArray<FBDCellCoord>& Candidates,
	const int32 Count, TArray<FBDCellCoord>& OutPicked)
{
	OutPicked.Reset();

	TArray<FBDCellCoord> Pool = Candidates;
	const int32 Take = FMath::Clamp(Count, 0, Pool.Num());

	// Partial Fisher-Yates: every draw is distinct and every attempt consumes the stream
	// in the same order, which is what keeps a seed reproducible across runs.
	for (int32 Index = 0; Index < Take; ++Index)
	{
		const int32 Swap = Stream.RandRange(Index, Pool.Num() - 1);
		Pool.Swap(Index, Swap);
		OutPicked.Add(Pool[Index]);
	}
}

void UBDObstacleGenerator::WriteCells(UBDGridSubsystem& Grid, const TArray<FBDCellCoord>& Cells, const EBDCellState State)
{
	for (const FBDCellCoord& Coord : Cells)
	{
		Grid.SetCellState(Coord, State);
	}
}

float UBDObstacleGenerator::ComputeFreeRatio(const UBDGridSubsystem& Grid)
{
	const int32 CellCount = Grid.GetCellCount();
	if (CellCount <= 0)
	{
		return 0.0f;
	}

	int32 FreeCount = 0;
	for (int32 Y = 0; Y < Grid.GetSizeY(); ++Y)
	{
		for (int32 X = 0; X < Grid.GetSizeX(); ++X)
		{
			FreeCount += Grid.GetCellState(FBDCellCoord(X, Y)) == EBDCellState::Free ? 1 : 0;
		}
	}

	return static_cast<float>(FreeCount) / static_cast<float>(CellCount);
}

bool UBDObstacleGenerator::IsLayoutPlayable(const UBDGridSubsystem& Grid, const TArray<FBDCellCoord>& Spawns,
	const TArray<FBDCellCoord>& Goals, FString& OutReason)
{
	const UBDPathfinder* Pathfinder = GetPathfinder();
	if (Pathfinder == nullptr)
	{
		OutReason = TEXT("no pathfinder in this world");
		return false;
	}

	const UBDObstacleSettings& Settings = UBDObstacleSettings::Get();

	const float FreeRatio = ComputeFreeRatio(Grid);
	if (FreeRatio < Settings.MinFreeRatio)
	{
		OutReason = FString::Printf(TEXT("only %.0f%% of the board is free, minimum is %.0f%%"),
			FreeRatio * 100.0f, Settings.MinFreeRatio * 100.0f);
		return false;
	}

	int32 ShortestRoute = MAX_int32;
	TArray<FBDCellCoord> Path;

	for (const FBDCellCoord& Spawn : Spawns)
	{
		// The route that matters is the one the creeps would actually take, so a spawn is
		// measured by its shortest way out, not by whichever goal happens to be first.
		int32 BestForSpawn = MAX_int32;
		for (const FBDCellCoord& Goal : Goals)
		{
			if (Pathfinder->FindPath(&Grid, Spawn, Goal, Path))
			{
				BestForSpawn = FMath::Min(BestForSpawn, Path.Num());
			}
		}

		if (BestForSpawn == MAX_int32)
		{
			OutReason = FString::Printf(TEXT("spawn %s has no route to any goal"), *Spawn.ToString());
			return false;
		}

		if (BestForSpawn < Settings.MinPathLength)
		{
			OutReason = FString::Printf(TEXT("spawn %s reaches a goal in %d cells, minimum is %d"),
				*Spawn.ToString(), BestForSpawn, Settings.MinPathLength);
			return false;
		}

		if (BestForSpawn > Settings.MaxPathLength)
		{
			OutReason = FString::Printf(TEXT("spawn %s needs %d cells to reach a goal, maximum is %d"),
				*Spawn.ToString(), BestForSpawn, Settings.MaxPathLength);
			return false;
		}

		ShortestRoute = FMath::Min(ShortestRoute, BestForSpawn);
	}

	LastPathLength = ShortestRoute == MAX_int32 ? 0 : ShortestRoute;
	return true;
}

int32 UBDObstacleGenerator::ClearGeneratedObstacles(UBDGridSubsystem* Grid)
{
	HideObstacles();

	if (Grid == nullptr || GeneratedCells.Num() == 0)
	{
		GeneratedCells.Reset();
		return 0;
	}

	int32 ClearedCount = 0;
	for (const FBDCellCoord& Coord : GeneratedCells)
	{
		// Only cells still holding our obstacle go back to Free. Anything that took the
		// cell in the meantime is not ours to hand back.
		if (Grid->GetCellState(Coord) == EBDCellState::Blocked)
		{
			ClearedCount += Grid->SetCellState(Coord, EBDCellState::Free) ? 1 : 0;
		}
	}

	GeneratedCells.Reset();
	return ClearedCount;
}

bool UBDObstacleGenerator::GenerateObstacles(UBDGridSubsystem* Grid, const int32 Seed, const int32 ObstacleCountOverride)
{
	LastSeed = Seed;
	LastAttemptCount = 0;
	LastPathLength = 0;
	bLastGenerationValidated = false;

	if (Grid == nullptr)
	{
		UE_LOG(LogBDObstacle, Error, TEXT("GenerateObstacles was given no grid."));
		return false;
	}

	ClearGeneratedObstacles(Grid);

	TArray<FBDCellCoord> Spawns;
	TArray<FBDCellCoord> Goals;
	UBDPathfinder::GatherCellsWithState(*Grid, EBDCellState::Spawn, Spawns);
	UBDPathfinder::GatherCellsWithState(*Grid, EBDCellState::Goal, Goals);

	if (Spawns.Num() == 0)
	{
		UE_LOG(LogBDObstacle, Error,
			TEXT("The board has no spawn, so no layout could be validated and nothing was placed. ")
			TEXT("Author them first: BD.Grid.SetCells then BD.Grid.SaveLayout."));
		return false;
	}

	// The urn is placed by the player after the obstacles exist, so on a fresh board there
	// is no Goal yet. The zone it may go in stands in: every cell of it is kept clear, and
	// the routes are measured to its middle. A cell of the zone that still ends up walled
	// off is refused when the player tries to put the urn there, not here.
	TArray<FBDCellCoord> Protected = Goals;
	if (Goals.Num() == 0)
	{
		const UBDObjectiveSettings& ObjectiveSettings = UBDObjectiveSettings::Get();
		ObjectiveSettings.GetZoneCells(Protected);
		Goals.Add(ObjectiveSettings.GetZoneCenter());

		UE_LOG(LogBDObstacle, Log, TEXT("No goal on the board yet: validating against the objective zone, middle %s."),
			*Goals[0].ToString());
	}

	const UBDObstacleSettings& Settings = UBDObstacleSettings::Get();

	TArray<FBDCellCoord> Candidates;
	BuildCandidates(*Grid, Spawns, Protected, FMath::Max(0, Settings.ClearanceFromSpawnGoal), Candidates);

	const int32 RequestedCount = ObstacleCountOverride >= 0 ? ObstacleCountOverride : Settings.ObstacleCount;
	const int32 Count = FMath::Min(RequestedCount, Candidates.Num());
	if (Count < RequestedCount)
	{
		UE_LOG(LogBDObstacle, Warning,
			TEXT("Asked for %d obstacles but only %d cell(s) are free and clear of the spawns and goals."),
			RequestedCount, Candidates.Num());
	}

	FRandomStream Stream(Seed);
	TArray<FBDCellCoord> Picked;
	FString Reason;

	const int32 MaxAttempts = FMath::Max(1, Settings.MaxAttempts);
	for (int32 Attempt = 1; Attempt <= MaxAttempts; ++Attempt)
	{
		LastAttemptCount = Attempt;

		PickCells(Stream, Candidates, Count, Picked);
		WriteCells(*Grid, Picked, EBDCellState::Blocked);

		if (IsLayoutPlayable(*Grid, Spawns, Goals, Reason))
		{
			GeneratedCells = Picked;
			bLastGenerationValidated = true;
			ShowObstacles(*Grid);

			UE_LOG(LogBDObstacle, Log,
				TEXT("Seed %d: %d obstacle(s) accepted on attempt %d of %d. Shortest route %d cells."),
				Seed, Picked.Num(), Attempt, MaxAttempts, LastPathLength);
			return true;
		}

		// Candidates were Free by construction, so handing them back is exact.
		WriteCells(*Grid, Picked, EBDCellState::Free);

		UE_LOG(LogBDObstacle, Verbose, TEXT("Seed %d attempt %d rejected: %s."), Seed, Attempt, *Reason);
	}

	UE_LOG(LogBDObstacle, Warning,
		TEXT("Seed %d: %d attempts all rejected (last reason: %s). Falling back to the authored layout."),
		Seed, MaxAttempts, *Reason);

	TArray<FBDCellCoord> Fallback;
	for (const FBDCellCoord& Coord : Settings.FallbackObstacles)
	{
		if (Grid->GetCellState(Coord) == EBDCellState::Free)
		{
			Fallback.Add(Coord);
		}
	}

	WriteCells(*Grid, Fallback, EBDCellState::Blocked);
	GeneratedCells = Fallback;
	ShowObstacles(*Grid);

	if (Fallback.Num() == 0)
	{
		// A board with no obstacle is not a dull board, it is a failed generation: the
		// validation rules or the authored fallback no longer fit this layout.
		UE_LOG(LogBDObstacle, Error,
			TEXT("Seed %d: the authored fallback placed no obstacle either. The board is playing empty; ")
			TEXT("check MinPathLength/MaxPathLength against the layout and author FallbackObstacles."),
			Seed);
		return false;
	}

	UE_LOG(LogBDObstacle, Warning, TEXT("Authored fallback placed %d obstacle(s)."), Fallback.Num());
	return false;
}

bool UBDObstacleGenerator::IsObstacleShown(const FBDCellCoord& Coord) const
{
	const AActor* Actor = VisualActor.Get();
	const UInstancedStaticMeshComponent* Mesh = VisualMesh.Get();
	return Actor != nullptr && !Actor->IsHidden() && Mesh != nullptr && Mesh->GetStaticMesh() != nullptr
		&& Mesh->IsVisible() && ShownCells.Contains(Coord);
}

float UBDObstacleGenerator::ResolveGroundZ(const UBDGridSubsystem& Grid, const FVector& Point) const
{
	const UWorld* World = GetWorld();
	const float PlaneZ = Grid.GetOrigin().Z;
	if (World == nullptr)
	{
		return PlaneZ;
	}

	const UBDPlacementSettings& Settings = UBDPlacementSettings::Get();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BDObstacleGround), /*bTraceComplex*/ false);
	if (const AActor* Actor = VisualActor.Get())
	{
		Params.AddIgnoredActor(Actor);
	}

	FHitResult Hit;
	const FVector Start(Point.X, Point.Y, PlaneZ + Settings.GroundTraceDistance);
	const FVector End(Point.X, Point.Y, PlaneZ - Settings.GroundTraceDistance);
	return World->LineTraceSingleByChannel(Hit, Start, End, Settings.GroundTraceChannel, Params) ? Hit.ImpactPoint.Z : PlaneZ;
}

void UBDObstacleGenerator::ShowObstacles(const UBDGridSubsystem& Grid)
{
	HideObstacles();

	UWorld* World = GetWorld();
	if (World == nullptr || !World->IsGameWorld() || GeneratedCells.Num() == 0)
	{
		return;
	}

	const UBDObstacleSettings& Settings = UBDObstacleSettings::Get();
	UStaticMesh* StaticMesh = Settings.ObstacleMesh.LoadSynchronous();
	if (StaticMesh == nullptr)
	{
		UE_LOG(LogBDObstacle, Error, TEXT("No obstacle mesh (%s): the %d obstacle(s) stand invisible and the ghost refuses cells that look free."),
			*Settings.ObstacleMesh.ToString(), GeneratedCells.Num());
		return;
	}

	UInstancedStaticMeshComponent* Mesh = VisualMesh.Get();
	if (Mesh == nullptr)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(World->PersistentLevel, AActor::StaticClass(), TEXT("BDObstacles"));
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
		if (Actor == nullptr)
		{
			return;
		}

		Mesh = NewObject<UInstancedStaticMeshComponent>(Actor, TEXT("ObstacleMesh"));
		Mesh->SetMobility(EComponentMobility::Movable);
		Actor->SetRootComponent(Mesh);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->RegisterComponent();
		Actor->AddInstanceComponent(Mesh);

		VisualActor = Actor;
		VisualMesh = Mesh;
	}

	Mesh->SetStaticMesh(StaticMesh);
	if (UMaterialInterface* Material = Settings.ObstacleMaterial.LoadSynchronous())
	{
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			Mesh->SetMaterial(Slot, Material);
		}
	}

	// Fitted from the bounds, so a cube pivoted at its centre and an asset pivoted at its
	// base both end up standing on the floor and filling the same share of the cell.
	const FBox Bounds = StaticMesh->GetBoundingBox();
	const FVector Size = Bounds.GetSize();
	const float Across = Grid.GetCellSize() * Settings.ObstacleFootprintRatio;
	const FVector Scale(
		Across / FMath::Max(Size.X, 1.0f),
		Across / FMath::Max(Size.Y, 1.0f),
		Settings.ObstacleHeight / FMath::Max(Size.Z, 1.0f));

	TArray<FTransform> Instances;
	Instances.Reserve(GeneratedCells.Num());
	for (const FBDCellCoord& Coord : GeneratedCells)
	{
		FVector Location = Grid.CellToWorld(Coord);
		Location.Z = ResolveGroundZ(Grid, Location) - Bounds.Min.Z * Scale.Z;
		// Centred on the cell whatever the mesh's own pivot is across.
		Location.X -= Bounds.GetCenter().X * Scale.X;
		Location.Y -= Bounds.GetCenter().Y * Scale.Y;
		Instances.Emplace(FQuat::Identity, Location, Scale);
		ShownCells.Add(Coord);
	}
	Mesh->AddInstances(Instances, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);

	UE_LOG(LogBDObstacle, Log, TEXT("%d obstacle(s) shown as %s, %.0f cm across and %.0f cm tall."),
		ShownCells.Num(), *StaticMesh->GetName(), Across, Settings.ObstacleHeight);
}

void UBDObstacleGenerator::HideObstacles()
{
	if (UInstancedStaticMeshComponent* Mesh = VisualMesh.Get())
	{
		Mesh->ClearInstances();
	}
	ShownCells.Reset();
}

namespace BDObstacleCommands
{
	static constexpr int32 ArgCountGenerate = 1;

	/**
	 * A cheap fingerprint of a layout, so two seeds can be told apart from the log alone
	 * and the same seed can be shown to rebuild the same board.
	 */
	static uint32 HashLayout(const TArray<FBDCellCoord>& Cells)
	{
		uint32 Hash = 0;
		for (const FBDCellCoord& Coord : Cells)
		{
			Hash = HashCombine(Hash, GetTypeHash(Coord));
		}

		return Hash;
	}

	static void ExecGenerate(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogBDObstacle, Error, TEXT("BD.Obstacles.Generate needs a world."));
			return;
		}

		if (Args.Num() != ArgCountGenerate)
		{
			UE_LOG(LogBDObstacle, Error, TEXT("Usage: BD.Obstacles.Generate <seed>"));
			return;
		}

		UBDGridSubsystem* Grid = World->GetSubsystem<UBDGridSubsystem>();
		UBDObstacleGenerator* Generator = World->GetSubsystem<UBDObstacleGenerator>();
		if (Grid == nullptr || Generator == nullptr)
		{
			UE_LOG(LogBDObstacle, Error, TEXT("BD.Obstacles.Generate: this world has no grid or no generator."));
			return;
		}

		const int32 Seed = FCString::Atoi(*Args[0]);
		const bool bValidated = Generator->GenerateObstacles(Grid, Seed);

		UE_LOG(LogBDObstacle, Log,
			TEXT("BD.Obstacles.Generate %d: %s, %d cell(s), %d attempt(s), shortest route %d, layout %08x."),
			Seed,
			bValidated ? TEXT("validated") : TEXT("FELL BACK to the authored layout"),
			Generator->GetGeneratedCells().Num(),
			Generator->GetLastAttemptCount(),
			Generator->GetLastPathLength(),
			HashLayout(Generator->GetGeneratedCells()));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdGenerate(
		TEXT("BD.Obstacles.Generate"),
		TEXT("BD.Obstacles.Generate <seed>: lays down a seeded, validated obstacle layout."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecGenerate));
}

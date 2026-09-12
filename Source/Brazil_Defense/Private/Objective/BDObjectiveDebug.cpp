// Brazil Defense. Console access to the urn, for placing it without a mouse.

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "BDDebugAssetLookup.h"
#include "BDLog.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Objective/BDObjective.h"
#include "Objective/BDObjectiveSettings.h"
#include "Objective/BDObjectiveSubsystem.h"
#include "Placement/BDPlaceableData.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDObjectiveDebug
{
	static constexpr int32 ArgCountPlaceAtMin = 2;
	static constexpr int32 ArgCountPlaceAtMax = 3;

	static UBDObjectiveSubsystem* FindObjectives(const UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("This command needs a world."));
			return nullptr;
		}

		UBDObjectiveSubsystem* Objectives = World->GetSubsystem<UBDObjectiveSubsystem>();
		if (Objectives == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("No objective subsystem in this world."));
		}

		return Objectives;
	}

	/**
	 * The placeable that describes the urn: the one named, or the first UBDPlaceableData
	 * in the project whose kind is Objective. Null when there is none, which still places
	 * a bare BD Objective actor.
	 */
	static const UBDPlaceableData* ResolveObjectivePlaceable(const TArray<FString>& Args, const int32 NameArgIndex)
	{
		if (Args.IsValidIndex(NameArgIndex))
		{
			const UBDPlaceableData* Placeable = BDDebugAssetLookup::FindByPathOrName<UBDPlaceableData>(Args[NameArgIndex]);
			if (Placeable == nullptr)
			{
				UE_LOG(LogBDGrid, Error, TEXT("BD.Objective.PlaceAt: no UBDPlaceableData found for '%s'."), *Args[NameArgIndex]);
			}
			return Placeable;
		}

		const IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(UBDPlaceableData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses*/ true);

		for (const FAssetData& Asset : Assets)
		{
			const UBDPlaceableData* Placeable = Cast<UBDPlaceableData>(Asset.GetAsset());
			if (Placeable != nullptr && Placeable->GetPieceKind() == EBDPieceKind::Objective)
			{
				return Placeable;
			}
		}

		UE_LOG(LogBDGrid, Warning, TEXT("BD.Objective.PlaceAt: no objective placeable in the project; placing a bare BD Objective."));
		return nullptr;
	}

	static void ExecPlaceAt(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < ArgCountPlaceAtMin || Args.Num() > ArgCountPlaceAtMax)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Objective.PlaceAt <x> <y> [placeable asset path or name]"));
			return;
		}

		UBDObjectiveSubsystem* Objectives = FindObjectives(World);
		if (Objectives == nullptr)
		{
			return;
		}

		const UBDPlaceableData* Placeable = ResolveObjectivePlaceable(Args, 2);
		UClass* ActorClass = Placeable != nullptr ? Placeable->ActorClass.LoadSynchronous() : nullptr;
		UStaticMesh* Mesh = Placeable != nullptr ? Placeable->PreviewMesh.LoadSynchronous() : nullptr;

		// Straight to the subsystem, past the gesture and the budget: this is for moving
		// the urn around while tuning, as many times as it takes.
		const FBDCellCoord Coord(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		EBDObjectiveRefusal Refusal;
		const bool bPlaced = Objectives->PlaceObjective(Coord, ActorClass, Mesh, Refusal);

		UE_LOG(LogBDGrid, Log, TEXT("BD.Objective.PlaceAt %s: %s%s."), *Coord.ToString(),
			bPlaced ? TEXT("placed") : TEXT("refused: "),
			bPlaced ? TEXT("") : *UBDObjectiveSubsystem::DescribeRefusal(Refusal));
	}

	static void ExecStatus(const TArray<FString>& Args, UWorld* World)
	{
		const UBDObjectiveSubsystem* Objectives = FindObjectives(World);
		if (Objectives == nullptr)
		{
			return;
		}

		const UBDObjectiveSettings& Settings = UBDObjectiveSettings::Get();
		const ABDObjective* Urn = Objectives->GetObjective();

		UE_LOG(LogBDGrid, Log, TEXT("BD.Objective.Status: %s, cell %s, actor %s, zone %d..%d x %d..%d."),
			Objectives->IsPlaced() ? TEXT("placed") : TEXT("NOT placed"),
			Objectives->IsPlaced() ? *Objectives->GetGoalCell().ToString() : TEXT("-"),
			Urn != nullptr ? *Urn->GetActorLocation().ToCompactString() : TEXT("none"),
			Settings.MinX, Settings.MaxX, Settings.MinY, Settings.MaxY);

		UBDWaveSubsystem* Waves = World->GetSubsystem<UBDWaveSubsystem>();
		if (Waves == nullptr)
		{
			return;
		}

		const TArray<FBDSpawnPoint>& Points = Waves->GetSpawnPoints();
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const FBDSpawnPoint& Point = Points[Index];
			const FString Route = Point.Route.Num() > 0
				? FString::Printf(TEXT("%d cells to %s"), Point.Route.Num(), *Point.Route.Last().ToString())
				: TEXT("NO ROUTE");

			UE_LOG(LogBDGrid, Log, TEXT("  spawn %d at %s: %s."), Index, *Point.ExitCell.ToString(), *Route);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdPlaceAt(
		TEXT("BD.Objective.PlaceAt"),
		TEXT("BD.Objective.PlaceAt <x> <y> [placeable]: puts the urn on a cell, skipping the gesture and the budget."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecPlaceAt));

	static FAutoConsoleCommandWithWorldAndArgs CmdStatus(
		TEXT("BD.Objective.Status"),
		TEXT("BD.Objective.Status: logs the urn cell, the actor position and the route of every spawn."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));
}

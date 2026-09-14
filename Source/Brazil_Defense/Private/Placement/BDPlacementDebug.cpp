// Brazil Defense. Console access to the placement gesture, for testing it without a mouse.

#include "BDDebugAssetLookup.h"
#include "BDLog.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementComponent.h"
#include "Platform/BDPlatformComponent.h"

namespace BDPlacementDebug
{
	static constexpr int32 ArgCountSelectTransient = 3;
	static constexpr int32 ArgCountSelect = 1;
	static constexpr int32 ArgCountAt = 2;
	static constexpr int32 ArgCountAtEdge = 3;

	static UBDPlacementComponent* FindPlacementComponent(const UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (const APlayerController* Controller = It->Get())
			{
				if (UBDPlacementComponent* Placement = Controller->FindComponentByClass<UBDPlacementComponent>())
				{
					return Placement;
				}
			}
		}

		UE_LOG(LogBDGrid, Error, TEXT("No player controller in this world carries a UBDPlacementComponent."));
		return nullptr;
	}

	/**
	 * Builds a throwaway placeable so the gesture can be exercised before any data asset
	 * exists in Content. It has no ActorClass, so placing one marks the cells or edges
	 * without spawning anything. "Edge" as the state makes a fence of the given length.
	 */
	static void ExecSelectTransient(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != ArgCountSelectTransient)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Place.SelectTransient <footprintX> <footprintY> <State|Edge>"));
			return;
		}

		UBDPlacementComponent* Placement = FindPlacementComponent(World);
		if (Placement == nullptr)
		{
			return;
		}

		UBDPlaceableData* Placeable = NewObject<UBDPlaceableData>(World, NAME_None, RF_Transient);
		Placeable->DisplayName = FText::FromString(TEXT("Transient test piece"));

		if (Args[2].Equals(TEXT("Edge"), ESearchCase::IgnoreCase))
		{
			Placeable->bOccupiesEdge = true;
			Placeable->SegmentLength = FMath::Max(1, FCString::Atoi(*Args[0]));
			Placement->SelectPlaceable(Placeable);

			UE_LOG(LogBDGrid, Log, TEXT("BD.Place.SelectTransient: fence of %d edge(s)."), Placeable->SegmentLength);
			return;
		}

		const UEnum* StateEnum = StaticEnum<EBDCellState>();
		const int64 StateValue = StateEnum != nullptr ? StateEnum->GetValueByNameString(Args[2]) : INDEX_NONE;
		if (StateValue == INDEX_NONE || StateValue >= static_cast<int64>(EBDCellState::Count))
		{
			UE_LOG(LogBDGrid, Error, TEXT("BD.Place.SelectTransient: '%s' is not a cell state or Edge."), *Args[2]);
			return;
		}

		Placeable->Footprint = FIntPoint(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		Placeable->OccupiesAs = static_cast<EBDCellState>(StateValue);

		Placement->SelectPlaceable(Placeable);

		UE_LOG(LogBDGrid, Log, TEXT("BD.Place.SelectTransient: %dx%d as %s."),
			Placeable->Footprint.X, Placeable->Footprint.Y, *StateEnum->GetNameStringByValue(StateValue));
	}

	static void ExecSelect(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != ArgCountSelect)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Place.Select <asset path or name>"));
			return;
		}

		UBDPlacementComponent* Placement = FindPlacementComponent(World);
		if (Placement == nullptr)
		{
			return;
		}

		UBDPlaceableData* Placeable = BDDebugAssetLookup::FindByPathOrName<UBDPlaceableData>(Args[0]);
		if (Placeable == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("BD.Place.Select: no UBDPlaceableData found for '%s'."), *Args[0]);
			return;
		}

		Placement->SelectPlaceable(Placeable);

		const FString ActorName = Placeable->ActorClass.IsNull() ? TEXT("none") : Placeable->ActorClass.GetAssetName();
		const FString PreviewName = Placeable->PreviewMesh.IsNull() ? TEXT("none") : Placeable->PreviewMesh.GetAssetName();

		if (Placeable->bOccupiesEdge)
		{
			UE_LOG(LogBDGrid, Log, TEXT("BD.Place.Select: '%s' (%s) fence of %d edge(s), %d per edge, actor %s, preview %s."),
				*Placeable->GetName(), *Placeable->DisplayName.ToString(),
				FMath::Max(1, Placeable->SegmentLength), FMath::Max(1, Placeable->InstancesPerEdge),
				*ActorName, *PreviewName);
			return;
		}

		const UEnum* StateEnum = StaticEnum<EBDCellState>();
		UE_LOG(LogBDGrid, Log, TEXT("BD.Place.Select: '%s' (%s) %dx%d as %s, actor %s, preview %s."),
			*Placeable->GetName(), *Placeable->DisplayName.ToString(),
			Placeable->Footprint.X, Placeable->Footprint.Y,
			*StateEnum->GetNameStringByValue(static_cast<int64>(Placeable->OccupiesAs)),
			*ActorName, *PreviewName);
	}

	/** Reads "<x> <y> <dir>" into an edge. @return false and logs when the direction is not 0 or 1. */
	static bool ParseEdge(const TArray<FString>& Args, const TCHAR* Command, FBDEdgeCoord& OutEdge)
	{
		const int32 Direction = FCString::Atoi(*Args[2]);
		if (Direction < 0 || Direction >= FBDEdgeCoord::DirectionCount)
		{
			UE_LOG(LogBDGrid, Error, TEXT("%s: direction must be 0 (+X) or 1 (+Y), got '%s'."), Command, *Args[2]);
			return false;
		}

		OutEdge = FBDEdgeCoord(FBDCellCoord(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1])), static_cast<uint8>(Direction));
		return true;
	}

	static void ExecPlaceAtEdge(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != ArgCountAtEdge)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Place.AtEdge <x> <y> <dir: 0=+X 1=+Y>"));
			return;
		}

		UBDPlacementComponent* Placement = FindPlacementComponent(World);
		FBDEdgeCoord Edge;
		if (Placement == nullptr || !ParseEdge(Args, TEXT("BD.Place.AtEdge"), Edge))
		{
			return;
		}

		Placement->SetHoveredEdgeDirect(Edge);

		const bool bValidBefore = Placement->IsCurrentPlacementValid();
		const FString Reason = Placement->DescribeCurrentRefusal();
		const bool bPlaced = Placement->TryPlaceAtHovered();

		UE_LOG(LogBDGrid, Log, TEXT("BD.Place.AtEdge %s: preview said %s (%s), placement %s."),
			*Edge.ToString(),
			bValidBefore ? TEXT("valid") : TEXT("refused"), *Reason,
			bPlaced ? TEXT("succeeded") : TEXT("rejected"));
	}

	/** Removing is selling: BD.Place.SellAtEdge and BD.Place.RemoveAtEdge are the same gesture. */
	static void ExecRemoveAtEdge(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != ArgCountAtEdge)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Place.SellAtEdge <x> <y> <dir: 0=+X 1=+Y>"));
			return;
		}

		UBDPlacementComponent* Placement = FindPlacementComponent(World);
		FBDEdgeCoord Edge;
		if (Placement == nullptr || !ParseEdge(Args, TEXT("BD.Place.SellAtEdge"), Edge))
		{
			return;
		}

		Placement->SetHoveredEdgeDirect(Edge);

		UE_LOG(LogBDGrid, Log, TEXT("BD.Place.SellAtEdge %s: %s."), *Edge.ToString(),
			Placement->TryRemoveAtHovered() ? TEXT("sold") : TEXT("nothing of the player there, or not removable now"));
	}

	static void ExecPlaceAt(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != ArgCountAt)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Place.At <x> <y>"));
			return;
		}

		UBDPlacementComponent* Placement = FindPlacementComponent(World);
		if (Placement == nullptr)
		{
			return;
		}

		const FBDCellCoord Coord(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		Placement->SetHoveredCellDirect(Coord);

		const bool bValidBefore = Placement->IsCurrentPlacementValid();
		const FString Reason = Placement->DescribeCurrentRefusal();
		const bool bPlaced = Placement->TryPlaceAtHovered();

		UE_LOG(LogBDGrid, Log, TEXT("BD.Place.At %s: preview said %s (%s), placement %s."),
			*Coord.ToString(),
			bValidBefore ? TEXT("valid") : TEXT("refused"), *Reason,
			bPlaced ? TEXT("succeeded") : TEXT("rejected"));
	}

	static void ExecPlaceAtSlot(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != 3)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Place.AtSlot <x> <y> <slot>"));
			return;
		}

		UBDPlacementComponent* Placement = FindPlacementComponent(World);
		if (Placement == nullptr)
		{
			return;
		}

		const FBDCellCoord Coord(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		UBDPlatformComponent* Platform = Placement->FindPlatformAt(Coord);
		if (Platform == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("BD.Place.AtSlot: no platform at %s."), *Coord.ToString());
			return;
		}

		const int32 SlotIndex = FCString::Atoi(*Args[2]);
		Placement->SetHoveredSlotDirect(Platform, SlotIndex);

		const bool bValidBefore = Placement->IsCurrentPlacementValid();
		const FString Reason = Placement->DescribeCurrentRefusal();
		const bool bPlaced = Placement->TryPlaceAtHovered();

		UE_LOG(LogBDGrid, Log, TEXT("BD.Place.AtSlot %s slot %d: preview said %s (%s), placement %s."),
			*Coord.ToString(), SlotIndex,
			bValidBefore ? TEXT("valid") : TEXT("refused"), *Reason,
			bPlaced ? TEXT("succeeded") : TEXT("rejected"));
	}

	/** Removing is selling: BD.Place.Sell and BD.Place.RemoveAt are the same gesture. */
	static void ExecRemoveAt(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != ArgCountAt)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Place.Sell <x> <y>"));
			return;
		}

		UBDPlacementComponent* Placement = FindPlacementComponent(World);
		if (Placement == nullptr)
		{
			return;
		}

		const FBDCellCoord Coord(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		Placement->SetHoveredCellDirect(Coord);

		UE_LOG(LogBDGrid, Log, TEXT("BD.Place.Sell %s: %s."), *Coord.ToString(),
			Placement->TryRemoveAtHovered() ? TEXT("sold") : TEXT("nothing of the player there, or not removable now"));
	}

	static void ExecPickUp(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != ArgCountAt)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Place.PickUp <x> <y>"));
			return;
		}

		UBDPlacementComponent* Placement = FindPlacementComponent(World);
		if (Placement == nullptr)
		{
			return;
		}

		const FBDCellCoord Coord(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		Placement->SetHoveredCellDirect(Coord);

		UE_LOG(LogBDGrid, Log, TEXT("BD.Place.PickUp %s: %s. Drop with BD.Place.At / AtEdge, or BD.Place.CancelMove."),
			*Coord.ToString(), Placement->TryBeginMoveAtHovered() ? TEXT("lifted") : TEXT("nothing to lift, or moving not allowed now"));
	}

	static void ExecPickUpEdge(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != ArgCountAtEdge)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Place.PickUpEdge <x> <y> <dir: 0=+X 1=+Y>"));
			return;
		}

		UBDPlacementComponent* Placement = FindPlacementComponent(World);
		FBDEdgeCoord Edge;
		if (Placement == nullptr || !ParseEdge(Args, TEXT("BD.Place.PickUpEdge"), Edge))
		{
			return;
		}

		Placement->SetHoveredEdgeDirect(Edge);

		UE_LOG(LogBDGrid, Log, TEXT("BD.Place.PickUpEdge %s: %s."),
			*Edge.ToString(), Placement->TryBeginMoveAtHovered() ? TEXT("lifted") : TEXT("nothing to lift, or moving not allowed now"));
	}

	static void ExecCancelMove(const TArray<FString>& Args, UWorld* World)
	{
		if (UBDPlacementComponent* Placement = FindPlacementComponent(World))
		{
			const bool bWasMoving = Placement->IsMoving();
			Placement->CancelMove();
			UE_LOG(LogBDGrid, Log, TEXT("BD.Place.CancelMove: %s."), bWasMoving ? TEXT("put back") : TEXT("nothing lifted"));
		}
	}

	static void ExecCellState(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != ArgCountAt || World == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Grid.CellState <x> <y>"));
			return;
		}

		const UBDGridSubsystem* Grid = World->GetSubsystem<UBDGridSubsystem>();
		if (Grid == nullptr)
		{
			return;
		}

		const FBDCellCoord Coord(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		const UEnum* StateEnum = StaticEnum<EBDCellState>();

		UE_LOG(LogBDGrid, Log, TEXT("BD.Grid.CellState %s: %s."), *Coord.ToString(),
			*StateEnum->GetNameStringByValue(static_cast<int64>(Grid->GetCellState(Coord))));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSelectTransient(
		TEXT("BD.Place.SelectTransient"),
		TEXT("BD.Place.SelectTransient <footprintX> <footprintY> <State>: selects a throwaway piece with no actor."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSelectTransient));

	static FAutoConsoleCommandWithWorldAndArgs CmdSelect(
		TEXT("BD.Place.Select"),
		TEXT("BD.Place.Select <asset path or name>: selects a real placeable data asset, e.g. DA_Divider."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSelect));

	static FAutoConsoleCommandWithWorldAndArgs CmdPlaceAt(
		TEXT("BD.Place.At"),
		TEXT("BD.Place.At <x> <y>: points the hover at a cell and tries to place the selected piece."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecPlaceAt));

	static FAutoConsoleCommandWithWorldAndArgs CmdPlaceAtSlot(
		TEXT("BD.Place.AtSlot"),
		TEXT("BD.Place.AtSlot <x> <y> <slot>: points the hover at a slot of the platform on a cell and tries to place the selected piece."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecPlaceAtSlot));

	static FAutoConsoleCommandWithWorldAndArgs CmdPlaceAtEdge(
		TEXT("BD.Place.AtEdge"),
		TEXT("BD.Place.AtEdge <x> <y> <dir: 0=+X 1=+Y>: points the hover at an edge and tries to place the selected fence."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecPlaceAtEdge));

	static FAutoConsoleCommandWithWorldAndArgs CmdSellAtEdge(
		TEXT("BD.Place.SellAtEdge"),
		TEXT("BD.Place.SellAtEdge <x> <y> <dir: 0=+X 1=+Y>: sells the fence on an edge, paying part of its build cost back in blue votes."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecRemoveAtEdge));

	static FAutoConsoleCommandWithWorldAndArgs CmdSell(
		TEXT("BD.Place.Sell"),
		TEXT("BD.Place.Sell <x> <y>: sells the piece on a cell, paying part of its build cost back in blue votes. A platform returns its passengers to the hand."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecRemoveAt));

	static FAutoConsoleCommandWithWorldAndArgs CmdRemoveAtEdge(
		TEXT("BD.Place.RemoveAtEdge"),
		TEXT("BD.Place.RemoveAtEdge <x> <y> <dir: 0=+X 1=+Y>: same as BD.Place.SellAtEdge."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecRemoveAtEdge));

	static FAutoConsoleCommandWithWorldAndArgs CmdRemoveAt(
		TEXT("BD.Place.RemoveAt"),
		TEXT("BD.Place.RemoveAt <x> <y>: same as BD.Place.Sell."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecRemoveAt));

	static FAutoConsoleCommandWithWorldAndArgs CmdPickUp(
		TEXT("BD.Place.PickUp"),
		TEXT("BD.Place.PickUp <x> <y>: lifts the piece on a cell (or the nearest slot of a platform there) to move it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecPickUp));

	static FAutoConsoleCommandWithWorldAndArgs CmdPickUpEdge(
		TEXT("BD.Place.PickUpEdge"),
		TEXT("BD.Place.PickUpEdge <x> <y> <dir: 0=+X 1=+Y>: lifts the fence on an edge to move it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecPickUpEdge));

	static FAutoConsoleCommandWithWorldAndArgs CmdCancelMove(
		TEXT("BD.Place.CancelMove"),
		TEXT("BD.Place.CancelMove: puts the lifted piece back where it was, for nothing."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecCancelMove));

	static FAutoConsoleCommandWithWorldAndArgs CmdCellState(
		TEXT("BD.Grid.CellState"),
		TEXT("BD.Grid.CellState <x> <y>: logs the state of one cell."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecCellState));
}

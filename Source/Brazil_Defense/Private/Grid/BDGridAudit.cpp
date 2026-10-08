// Brazil Defense. Holds every cell state of the grid against what really stands on it.

#include "Grid/BDGridAudit.h"

#include "BDLog.h"
#include "Day/BDStreetLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Grid/BDGridLayoutActor.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Objective/BDObjectiveSubsystem.h"
#include "Obstacle/BDObstacleGenerator.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementComponent.h"
#include "Platform/BDPlatformComponent.h"
#include "UObject/UObjectIterator.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDGridAuditPrivate
{
	/** Something that says it holds a cell, the state it holds it as, and whether the player can see it. */
	struct FClaim
	{
		FString Who;
		EBDCellState Expected = EBDCellState::Free;
		bool bVisible = true;
	};

	const TCHAR* StateName(const EBDCellState State)
	{
		switch (State)
		{
		case EBDCellState::Free: return TEXT("Free");
		case EBDCellState::Tower: return TEXT("Tower");
		case EBDCellState::Platform: return TEXT("Platform");
		case EBDCellState::Blocked: return TEXT("Blocked");
		case EBDCellState::Spawn: return TEXT("Spawn");
		case EBDCellState::Goal: return TEXT("Goal");
		default: return TEXT("?");
		}
	}

	bool AnyActorShown(const TArray<TObjectPtr<AActor>>& Actors)
	{
		for (const AActor* Actor : Actors)
		{
			if (IsValid(Actor) && !Actor->IsHidden())
			{
				return true;
			}
		}
		return false;
	}

	FString JoinCells(const TArray<FBDCellCoord>& Cells, const int32 Max)
	{
		FString Text;
		for (int32 Index = 0; Index < Cells.Num() && Index < Max; ++Index)
		{
			Text += (Index > 0 ? TEXT(" ") : TEXT("")) + Cells[Index].ToString();
		}
		if (Cells.Num() > Max)
		{
			Text += FString::Printf(TEXT(" +%d"), Cells.Num() - Max);
		}
		return Text;
	}
}

FString FBDGridAuditReport::Summary(const int32 MaxCellsPerList) const
{
	using namespace BDGridAuditPrivate;
	return FString::Printf(TEXT("%d cells, %d taken: %d ghost(s) [%s], %d invisible [%s], %d mismatch(es) [%s]"),
		CellsChecked, CellsTaken,
		GhostCells.Num(), *JoinCells(GhostCells, MaxCellsPerList),
		InvisibleCells.Num(), *JoinCells(InvisibleCells, MaxCellsPerList),
		MismatchCells.Num(), *JoinCells(MismatchCells, MaxCellsPerList));
}

FBDGridAuditReport BDGridAudit::Run(UWorld& World)
{
	using namespace BDGridAuditPrivate;

	FBDGridAuditReport Report;
	const UBDGridSubsystem* Grid = World.GetSubsystem<UBDGridSubsystem>();
	if (Grid == nullptr || Grid->GetCellCount() <= 0)
	{
		return Report;
	}

	TMap<FBDCellCoord, TArray<FClaim>> Claims;
	const auto Claim = [&Claims, Grid](const FBDCellCoord& Coord, const FString& Who, const EBDCellState Expected, const bool bVisible)
	{
		if (Grid->IsValidCoord(Coord))
		{
			Claims.FindOrAdd(Coord).Add({ Who, Expected, bVisible });
		}
	};

	// Pieces the player placed: every cell of the footprint points at the piece.
	const APlayerController* Controller = World.GetFirstPlayerController();
	if (const UBDPlacementComponent* Placement = Controller != nullptr ? Controller->FindComponentByClass<UBDPlacementComponent>() : nullptr)
	{
		for (const TPair<FBDCellCoord, FBDPlacedPiece>& Entry : Placement->GetPlacedByCell())
		{
			const FBDPlacedPiece& Piece = Entry.Value;
			const EBDCellState Expected = Piece.Data != nullptr ? Piece.Data->OccupiesAs : EBDCellState::Tower;
			Claim(Entry.Key, FString::Printf(TEXT("piece '%s' from %s"), *GetNameSafe(Piece.Data), *Piece.Origin.ToString()),
				Expected, AnyActorShown(Piece.Actors));
		}
	}

	// Platforms stamp their own cells, the authored ones of the level and the placed ones alike.
	for (TObjectIterator<UBDPlatformComponent> It; It; ++It)
	{
		const UBDPlatformComponent* Platform = *It;
		if (Platform->GetWorld() != &World || Platform->IsLifted())
		{
			continue;
		}
		const AActor* Owner = Platform->GetOwner();
		for (const TPair<FBDCellCoord, EBDCellState>& Stamped : Platform->GetStampedCells())
		{
			Claim(Stamped.Key, FString::Printf(TEXT("stamp of %s"), *GetNameSafe(Owner)), EBDCellState::Platform,
				IsValid(Owner) && !Owner->IsHidden());
		}
	}

	for (TObjectIterator<UBDStreetLightComponent> It; It; ++It)
	{
		const UBDStreetLightComponent* Light = *It;
		FBDCellCoord Coord;
		if (Light->GetWorld() == &World && Light->GetOccupiedCell(Coord))
		{
			Claim(Coord, FString::Printf(TEXT("street light %s"), *GetNameSafe(Light->GetOwner())), EBDCellState::Blocked, true);
		}
	}

	// Generation is off and the board starts clean. Should it be switched back on, its cells
	// are Blocked and nothing else, with no body: owned, but reported as nothing to see.
	if (const UBDObstacleGenerator* Generator = World.GetSubsystem<UBDObstacleGenerator>())
	{
		for (const FBDCellCoord& Coord : Generator->GetGeneratedCells())
		{
			Claim(Coord, FString::Printf(TEXT("generated obstacle (seed %d)"), Generator->GetLastSeed()), EBDCellState::Blocked, false);
		}
	}

	if (UBDWaveSubsystem* Waves = World.GetSubsystem<UBDWaveSubsystem>())
	{
		const TArray<FBDSpawnPoint>& Points = Waves->GetSpawnPoints();
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			for (const FBDCellCoord& Coord : Points[Index].Cells)
			{
				Claim(Coord, FString::Printf(TEXT("mouth %d"), Index), EBDCellState::Spawn, true);
			}
		}
	}

	if (const UBDObjectiveSubsystem* Objectives = World.GetSubsystem<UBDObjectiveSubsystem>())
	{
		if (Objectives->IsPlaced())
		{
			Claim(Objectives->GetGoalCell(), TEXT("urn"), EBDCellState::Goal, true);
		}
	}

	// The authored layout only owns a cell while the cell still holds what it authored: a
	// mouth that slid away leaves its authored Spawn cells Free, and that is right.
	for (TActorIterator<ABDGridLayoutActor> It(&World); It; ++It)
	{
		for (const TPair<FBDCellCoord, EBDCellState>& Authored : It->GetAuthoredCells())
		{
			if (Authored.Value != EBDCellState::Free && Grid->GetCellState(Authored.Key) == Authored.Value)
			{
				Claim(Authored.Key, FString::Printf(TEXT("authored by %s"), *It->GetName()), Authored.Value, true);
			}
		}
	}

	for (int32 Y = 0; Y < Grid->GetSizeY(); ++Y)
	{
		for (int32 X = 0; X < Grid->GetSizeX(); ++X)
		{
			const FBDCellCoord Coord(X, Y);
			const EBDCellState State = Grid->GetCellState(Coord);
			const TArray<FClaim>* CellClaims = Claims.Find(Coord);
			++Report.CellsChecked;

			if (CellClaims != nullptr)
			{
				FString& Text = Report.Holders.Add(Coord);
				for (const FClaim& Each : *CellClaims)
				{
					Text += FString::Printf(TEXT("%s%s as %s%s"), Text.IsEmpty() ? TEXT("") : TEXT("; "), *Each.Who,
						StateName(Each.Expected), Each.bVisible ? TEXT("") : TEXT(" (nothing to see)"));
				}
			}

			if (State == EBDCellState::Free)
			{
				// A Free cell some owner still counts as its own is the same bug from the other side.
				if (CellClaims != nullptr)
				{
					Report.MismatchCells.Add(Coord);
					Report.Mismatches.Add(FString::Printf(TEXT("%s is Free but %s still holds it as %s"),
						*Coord.ToString(), *(*CellClaims)[0].Who, StateName((*CellClaims)[0].Expected)));
				}
				continue;
			}

			++Report.CellsTaken;
			if (CellClaims == nullptr)
			{
				Report.GhostCells.Add(Coord);
				Report.Ghosts.Add(FString::Printf(TEXT("%s is %s and nothing on the board holds it"), *Coord.ToString(), StateName(State)));
				continue;
			}

			const FClaim* Agreeing = CellClaims->FindByPredicate([State](const FClaim& Each) { return Each.Expected == State; });
			if (Agreeing == nullptr)
			{
				Report.MismatchCells.Add(Coord);
				Report.Mismatches.Add(FString::Printf(TEXT("%s is %s but %s holds it as %s"),
					*Coord.ToString(), StateName(State), *(*CellClaims)[0].Who, StateName((*CellClaims)[0].Expected)));
				continue;
			}

			const bool bAnyVisible = CellClaims->ContainsByPredicate([](const FClaim& Each) { return Each.bVisible; });
			if (!bAnyVisible)
			{
				Report.InvisibleCells.Add(Coord);
				Report.Invisible.Add(FString::Printf(TEXT("%s is %s for %s, with nothing on the board to see"),
					*Coord.ToString(), StateName(State), *Agreeing->Who));
			}
		}
	}

	return Report;
}

void BDGridAudit::LogReport(const FBDGridAuditReport& Report)
{
	UE_LOG(LogBDGrid, Display, TEXT("GRID AUDIT %s: %s"), Report.IsClean() ? TEXT("CLEAN") : TEXT("FOUND"), *Report.Summary());
	for (const FString& Line : Report.Ghosts) { UE_LOG(LogBDGrid, Warning, TEXT("GRID AUDIT ghost: %s."), *Line); }
	for (const FString& Line : Report.Invisible) { UE_LOG(LogBDGrid, Warning, TEXT("GRID AUDIT invisible: %s."), *Line); }
	for (const FString& Line : Report.Mismatches) { UE_LOG(LogBDGrid, Warning, TEXT("GRID AUDIT mismatch: %s."), *Line); }
}

namespace BDGridAuditCommands
{
	static void ExecAudit(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("BD.Grid.Audit needs a world."));
			return;
		}

		const FBDGridAuditReport Report = BDGridAudit::Run(*World);
		BDGridAudit::LogReport(Report);

		// Pairs of coordinates after it: what each of those cells is and who holds it.
		const UBDGridSubsystem* Grid = World->GetSubsystem<UBDGridSubsystem>();
		for (int32 Index = 0; Grid != nullptr && Index + 1 < Args.Num(); Index += 2)
		{
			const FBDCellCoord Coord(FCString::Atoi(*Args[Index]), FCString::Atoi(*Args[Index + 1]));
			const FString* Holder = Report.Holders.Find(Coord);
			UE_LOG(LogBDGrid, Display, TEXT("GRID AUDIT cell %s: %s, %s."), *Coord.ToString(),
				BDGridAuditPrivate::StateName(Grid->GetCellState(Coord)), Holder != nullptr ? **Holder : TEXT("held by nothing"));
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdAudit(
		TEXT("BD.Grid.Audit"),
		TEXT("BD.Grid.Audit [X Y ...]: lists every taken cell nothing holds (ghost), held by something the player cannot see (invisible), or held as another state (mismatch); each X Y given also says what holds that cell."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecAudit));
}

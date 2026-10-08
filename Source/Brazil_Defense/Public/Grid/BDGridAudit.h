// Brazil Defense. Holds every cell state of the grid against what really stands on it:
// a cell may only be taken when something in the world takes it.

#pragma once

#include "CoreMinimal.h"
#include "Grid/BDGridTypes.h"

class UWorld;

/** What BD.Grid.Audit found. Each line names the cell, its state and why it is wrong. */
struct BRAZIL_DEFENSE_API FBDGridAuditReport
{
	int32 CellsChecked = 0;
	int32 CellsTaken = 0;

	/** Taken cells nothing claims: a removal that did not give its cells back, a stale stamp. */
	TArray<FBDCellCoord> GhostCells;
	TArray<FString> Ghosts;

	/** Taken cells whose owner has nothing on the board to show for it: the player sees a free cell. */
	TArray<FBDCellCoord> InvisibleCells;
	TArray<FString> Invisible;

	/** An owner that says the cell holds one thing while the grid says another, Free included. */
	TArray<FBDCellCoord> MismatchCells;
	TArray<FString> Mismatches;

	/** Every cell something holds, with who holds it and as what. */
	TMap<FBDCellCoord, FString> Holders;

	bool IsClean() const { return Ghosts.Num() == 0 && Invisible.Num() == 0 && Mismatches.Num() == 0; }

	/** One line: the counts, then the first few cells of each list. */
	FString Summary(int32 MaxCellsPerList = 6) const;
};

namespace BDGridAudit
{
	/** Walks every cell of the world's grid. Read only. */
	BRAZIL_DEFENSE_API FBDGridAuditReport Run(UWorld& World);

	/** Every line of the report to LogBDGrid, the clean case included. */
	BRAZIL_DEFENSE_API void LogReport(const FBDGridAuditReport& Report);
}

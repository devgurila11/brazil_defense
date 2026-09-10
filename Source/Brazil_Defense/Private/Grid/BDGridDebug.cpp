// Brazil Defense. Runtime switch for the grid debug drawing.

#include "Grid/BDGridDebug.h"

#include "BDLog.h"
#include "Engine/World.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"

namespace BDGridDebug
{
	// Defaults to on so the grid stays visible while the layout is being authored.
	static int32 GGridDebugEnabled = 1;

	static FAutoConsoleVariableRef CVarGridDebug(
		TEXT("BD.Grid.Debug"),
		GGridDebugEnabled,
		TEXT("Brazil Defense grid debug drawing: grid lines, cell states, platform footprints and slots. 0 to disable."),
		ECVF_Cheat);

	bool IsEnabled()
	{
		return GGridDebugEnabled != 0;
	}
}

namespace BDGridDebugCommands
{
	static constexpr int32 ArgCountSetCells = 5;

	/**
	 * Paints a rectangle of cells with one state, so the grid can be reshaped from the
	 * console while there is no placement UI yet. Debug tooling, not the game rule: the
	 * real placement path will go through the build validation instead.
	 */
	static void ExecSetCells(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("BD.Grid.SetCells needs a world."));
			return;
		}

		if (Args.Num() != ArgCountSetCells)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Grid.SetCells <x1> <y1> <x2> <y2> <State>"));
			return;
		}

		UBDGridSubsystem* Grid = World->GetSubsystem<UBDGridSubsystem>();
		if (Grid == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("BD.Grid.SetCells: no grid subsystem in this world."));
			return;
		}

		const UEnum* StateEnum = StaticEnum<EBDCellState>();
		const int64 StateValue = StateEnum != nullptr ? StateEnum->GetValueByNameString(Args[4]) : INDEX_NONE;
		if (StateValue == INDEX_NONE || StateValue >= static_cast<int64>(EBDCellState::Count))
		{
			UE_LOG(LogBDGrid, Error,
				TEXT("BD.Grid.SetCells: '%s' is not a cell state. Valid: Free, Tower, Divider, Platform, Blocked, Spawn, Goal."),
				*Args[4]);
			return;
		}

		const EBDCellState NewState = static_cast<EBDCellState>(StateValue);
		const int32 MinX = FMath::Min(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[2]));
		const int32 MaxX = FMath::Max(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[2]));
		const int32 MinY = FMath::Min(FCString::Atoi(*Args[1]), FCString::Atoi(*Args[3]));
		const int32 MaxY = FMath::Max(FCString::Atoi(*Args[1]), FCString::Atoi(*Args[3]));

		int32 ChangedCount = 0;
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				ChangedCount += Grid->SetCellState(FBDCellCoord(X, Y), NewState) ? 1 : 0;
			}
		}

		UE_LOG(LogBDGrid, Log, TEXT("BD.Grid.SetCells (%d,%d)-(%d,%d) to %s: %d cell(s) changed."),
			MinX, MinY, MaxX, MaxY, *StateEnum->GetNameStringByValue(StateValue), ChangedCount);
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSetCells(
		TEXT("BD.Grid.SetCells"),
		TEXT("BD.Grid.SetCells <x1> <y1> <x2> <y2> <State>: paints a rectangle of cells with a state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSetCells));
}

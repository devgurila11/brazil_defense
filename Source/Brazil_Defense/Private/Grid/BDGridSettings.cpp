// Brazil Defense. Project wide configuration of the gameplay grid.

#include "Grid/BDGridSettings.h"

UBDGridSettings::UBDGridSettings()
{
	// Shows up under Project Settings > Game.
	CategoryName = TEXT("Game");

	// Free cells are intentionally left out of the map: they only show the grid lines.
	CellStateColors.Add(EBDCellState::Tower, FColor(255, 200, 0, 120));
	CellStateColors.Add(EBDCellState::Divider, FColor(0, 160, 255, 120));
	CellStateColors.Add(EBDCellState::Platform, FColor(160, 80, 255, 120));
	CellStateColors.Add(EBDCellState::Blocked, FColor(120, 120, 120, 120));
	CellStateColors.Add(EBDCellState::Spawn, FColor(255, 60, 60, 120));
	CellStateColors.Add(EBDCellState::Goal, FColor(60, 255, 120, 120));
}

const UBDGridSettings& UBDGridSettings::Get()
{
	const UBDGridSettings* Settings = GetDefault<UBDGridSettings>();
	check(Settings);
	return *Settings;
}

bool UBDGridSettings::TryGetCellStateColor(const EBDCellState State, FColor& OutColor) const
{
	if (const FColor* Found = CellStateColors.Find(State))
	{
		OutColor = *Found;
		return true;
	}

	return false;
}

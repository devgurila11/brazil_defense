// Brazil Defense. Where the player is allowed to put the urn, and how that is shown.

#include "Objective/BDObjectiveSettings.h"

UBDObjectiveSettings::UBDObjectiveSettings()
{
	CategoryName = TEXT("Game");
}

const UBDObjectiveSettings& UBDObjectiveSettings::Get()
{
	const UBDObjectiveSettings* Settings = GetDefault<UBDObjectiveSettings>();
	check(Settings);
	return *Settings;
}

bool UBDObjectiveSettings::IsInZone(const FBDCellCoord& Coord) const
{
	return !bRestrictToZone || (Coord.X >= MinX && Coord.X <= MaxX && Coord.Y >= MinY && Coord.Y <= MaxY);
}

FBDCellCoord UBDObjectiveSettings::GetZoneCenter() const
{
	return FBDCellCoord((MinX + MaxX) / 2, (MinY + MaxY) / 2);
}

void UBDObjectiveSettings::GetZoneCells(TArray<FBDCellCoord>& OutCells) const
{
	OutCells.Reset();

	// With no zone to keep clear, only its middle is protected, as the likely goal.
	if (!bRestrictToZone)
	{
		OutCells.Add(GetZoneCenter());
		return;
	}
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			OutCells.Emplace(X, Y);
		}
	}
}

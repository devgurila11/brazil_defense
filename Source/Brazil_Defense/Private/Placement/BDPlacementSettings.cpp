// Brazil Defense. Configuration of the placement gesture and its preview.

#include "Placement/BDPlacementSettings.h"

#include "Placement/BDPlaceableData.h"

UBDPlacementSettings::UBDPlacementSettings()
{
	CategoryName = TEXT("Game");
}

const UBDPlacementSettings& UBDPlacementSettings::Get()
{
	const UBDPlacementSettings* Settings = GetDefault<UBDPlacementSettings>();
	check(Settings);
	return *Settings;
}

int32 UBDPlacementSettings::GetUnlockWave(const UBDPlaceableData* Piece) const
{
	if (Piece == nullptr)
	{
		return 0;
	}

	// Compared by path, so the check never loads anything and a transient test piece
	// simply has no entry.
	const FSoftObjectPath Path(Piece);
	for (const FBDPieceUnlock& Entry : Unlocks)
	{
		if (Entry.Piece.ToSoftObjectPath() == Path)
		{
			return FMath::Max(0, Entry.Wave);
		}
	}
	return 0;
}

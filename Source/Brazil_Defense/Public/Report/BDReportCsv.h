// Brazil Defense. What the reports share: the board counted, and a row appended to a CSV.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/** What stands on the board, counted the way the reports count it. */
struct FBDBoardTally
{
	int32 Towers = 0;
	int32 Characters = 0;
	int32 Platforms = 0;
	int32 Dividers = 0;
	/** Per palette entry, so every kind of platform has its column. */
	TMap<FSoftObjectPath, int32> PerPiece;
	/** Every defender standing, on the ground or on a platform, and their levels. */
	int32 Defenders = 0;
	int32 LevelSum = 0;
	int32 TopLevel = 0;

	double GetAverageLevel() const { return Defenders > 0 ? static_cast<double>(LevelSum) / Defenders : 0.0; }
};

namespace BDReportCsv
{
	/** One column: its header and the value of this row. */
	struct FColumn
	{
		FString Name;
		FString Value;
	};

	/** A text value, quoted, with its own quotes doubled: reasons have commas in them. */
	BRAZIL_DEFENSE_API FString Quote(const FString& Text);

	/** Two decimals with a point, whatever the machine's locale: the sheet has to read them. */
	BRAZIL_DEFENSE_API FString Decimal(double Value);

	/** Played on the screen, played headless (a console run with no window), or played by BD.Sim.Run. */
	BRAZIL_DEFENSE_API const TCHAR* Mode(const UWorld& World);

	/**
	 * Counts the board off its saved form, which already counts a wide piece once and
	 * leaves the urn apart, and the defenders off the world.
	 */
	BRAZIL_DEFENSE_API FBDBoardTally TallyBoard(UWorld& World);

	/**
	 * Appends one row to the CSV at Path, header first when the file is new. A file whose
	 * header is not this one describes other columns: it is kept aside as
	 * <Stem>-<date>.csv and a new one started. Columns added at the end are the one change
	 * that needs no new file: the old rows are padded with empty cells.
	 * @return Whether the row was written.
	 */
	BRAZIL_DEFENSE_API bool AppendRow(const FString& Path, const TArray<FColumn>& Columns);
}

// Brazil Defense. The post-match report: one row per match, to calibrate with numbers.

#pragma once

#include "CoreMinimal.h"

class ABDMatchManager;

/**
 * Writes what a match was when it ended - the count, where the public money came from
 * and went, the board standing, what the creeps did - as one row of
 * Saved/Logs/PostMatch.csv, and a readable summary on LogBDMatch.
 *
 * Every end writes one: a victory, a defeat, a match left half played (to the menu, out
 * of the game, or thrown away for a save) and the wave cap of BD.Sim.Run. A match that
 * never sent a wave out writes nothing. The columns say whether it was played on the
 * screen, headless or by the simulation, so the rows can be told apart in a sheet.
 *
 * The file is appended to. When the columns change, the old file is kept under a dated
 * name and a new one is started, so a header never sits over rows it does not describe.
 */
namespace BDPostMatch
{
	/**
	 * Writes the row and the summary for the match as it stands, and marks its ledger as
	 * reported. The ledger's end fields (reason, money left) are read as they are: whoever
	 * ends the match fills them first.
	 * @param Outcome Victory, Defeat, Abandoned or WaveCap.
	 */
	BRAZIL_DEFENSE_API void Write(ABDMatchManager& Match, const TCHAR* Outcome);

	/** Where the rows go. */
	BRAZIL_DEFENSE_API FString GetCsvPath();
}

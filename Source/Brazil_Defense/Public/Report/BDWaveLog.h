// Brazil Defense. The wave log: one row per wave, to see a match unfold.

#pragma once

#include "CoreMinimal.h"

class ABDMatchManager;

/**
 * The running totals of a match as the last wave row left them. A row is the difference
 * between now and this, so every row covers one stretch: the building phase before its
 * wave and the wave itself. The first stretch starts where the ledger opened.
 */
struct FBDWaveLogMark
{
	/** The wave of the last row, 0 before the first. A wave is written once. */
	int32 Wave = 0;
	int32 VotesBlue = 0;
	int32 VotesRed = 0;
	int32 VotesNull = 0;
	/** Public money plus bribe held: all the money there is. */
	int32 Funds = 0;
	int32 FundsEarned = 0;
	int32 FundsRefunded = 0;
	int32 FundsGranted = 0;
	int32 FundsSpent = 0;
	int32 CreepsSpawned = 0;
	int32 CreepsKilled = 0;
	int32 CreepsArrived = 0;
	float DamageWasted = 0.0f;
	int32 CandidatesSent = 0;
	int32 CandidatesFallen = 0;
	int32 CandidateArrived = 0;
	/** What the last row found missing from the money, for the regression: 0 when it closes. */
	int64 LastFundsGap = 0;
};

/**
 * Writes one row of Saved/Logs/WaveLog.csv per wave that ends - cleared, or lost while
 * out - and one short line on LogBDMatch. The rows of one match share its MatchStart, and
 * the Mode column tells a match on the screen from a headless one or a simulation, as in
 * the post-match report. BD.WaveLog.Enabled 0 keeps the file untouched; the log line and
 * the running totals go on either way.
 */
namespace BDWaveLog
{
	/** The running totals of the match as they stand. Wave is the current wave. */
	BRAZIL_DEFENSE_API FBDWaveLogMark Mark(const ABDMatchManager& Match);

	/**
	 * Writes the row of the current wave, unless it has one already or no wave is out yet,
	 * and moves the ledger's mark up to now.
	 * @param Ending Cleared or Defeat.
	 */
	BRAZIL_DEFENSE_API void Write(ABDMatchManager& Match, const TCHAR* Ending);

	/** Where the rows go. */
	BRAZIL_DEFENSE_API FString GetCsvPath();
}

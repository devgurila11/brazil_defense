// Brazil Defense. The wave log: one row per wave, to see a match unfold.

#include "Report/BDWaveLog.h"

#include "BDBuildInfo.h"
#include "BDLog.h"
#include "Candidate/BDCandidateSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Match/BDMatchManager.h"
#include "Misc/Paths.h"
#include "Report/BDReportCsv.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDWaveLogPrivate
{
	/** On by default while the game is being balanced: the curves are the point. */
	static int32 GEnabled = 1;

	static FAutoConsoleVariableRef CVarEnabled(
		TEXT("BD.WaveLog.Enabled"),
		GEnabled,
		TEXT("1 (default) appends a row to Saved/Logs/WaveLog.csv at the end of every wave; 0 writes nothing (the one-line summary stays in the log)."),
		ECVF_Default);
}

FString BDWaveLog::GetCsvPath()
{
	return FPaths::Combine(FPaths::ProjectLogDir(), TEXT("WaveLog.csv"));
}

FBDWaveLogMark BDWaveLog::Mark(const ABDMatchManager& Match)
{
	FBDWaveLogMark Now;
	Now.Wave = Match.GetCurrentWave();
	Now.VotesBlue = Match.GetVotesBlue();
	Now.VotesRed = Match.GetVotesRed();
	Now.VotesNull = Match.GetVotesNull();
	Now.Funds = Match.GetPublicMoney() + Match.GetBribeHeld();

	const FBDMatchLedger& Ledger = Match.GetLedger();
	Now.FundsEarned = Ledger.BribeEarned;
	Now.FundsRefunded = Ledger.Refunded;
	Now.FundsGranted = Ledger.Granted;
	Now.FundsSpent = Ledger.SpentBuild + Ledger.SpentMove + Ledger.SpentEvolve;
	Now.LastFundsGap = Ledger.WaveMark.LastFundsGap;

	const UWorld* World = Match.GetWorld();
	if (const UBDWaveSubsystem* Waves = World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr)
	{
		const FBDMatchCombatTotals& Combat = Waves->GetMatchTotals();
		Now.CreepsSpawned = Combat.CreepsSpawned;
		Now.CreepsKilled = Combat.CreepsKilled;
		Now.CreepsArrived = Combat.CreepsArrived;
		Now.DamageWasted = Combat.DamageWasted;
	}
	if (const UBDCandidateSubsystem* Candidates = World != nullptr ? World->GetSubsystem<UBDCandidateSubsystem>() : nullptr)
	{
		Now.CandidatesSent = Candidates->GetCandidatesSent();
		Now.CandidatesFallen = Candidates->GetFallenCount();
		Now.CandidateArrived = Candidates->GetArrivedOrdinal();
	}
	return Now;
}

void BDWaveLog::Write(ABDMatchManager& Match, const TCHAR* Ending)
{
	using namespace BDWaveLogPrivate;
	using namespace BDReportCsv;

	UWorld* World = Match.GetWorld();
	FBDMatchLedger& Ledger = Match.GetLedgerMutable();
	const int32 Wave = Match.GetCurrentWave();
	if (World == nullptr || Wave < 1 || Wave <= Ledger.WaveMark.Wave)
	{
		return;
	}

	const FBDWaveLogMark Before = Ledger.WaveMark;
	FBDWaveLogMark Now = Mark(Match);

	// What came in and went out since the last row, and what should be left for it: a
	// gap is money that came from nowhere, or went nowhere, during this stretch.
	const int32 Earned = Now.FundsEarned - Before.FundsEarned;
	const int32 Refunded = Now.FundsRefunded - Before.FundsRefunded;
	const int32 Granted = Now.FundsGranted - Before.FundsGranted;
	const int32 Spent = Now.FundsSpent - Before.FundsSpent;
	const int64 Gap = static_cast<int64>(Before.Funds) + Earned + Refunded + Granted - Spent - Now.Funds;
	Now.LastFundsGap = Gap;

	const bool bCandidateOut = Now.CandidatesSent > Before.CandidatesSent;
	const int32 CandidatesKilled = Now.CandidatesFallen - Before.CandidatesFallen;
	const bool bCandidateArrived = Now.CandidateArrived > 0 && Before.CandidateArrived == 0;

	const FBDBoardTally Board = TallyBoard(*World);

	// The routes as they stand at the end of the wave: how far the maze makes them walk.
	UBDWaveSubsystem* Waves = World->GetSubsystem<UBDWaveSubsystem>();
	int32 RouteShortest = 0;
	int32 RouteLongest = 0;
	int32 PeakAlive = 0;
	if (Waves != nullptr)
	{
		PeakAlive = Waves->GetWavePeakAlive();
		for (const FBDSpawnPoint& Point : Waves->GetSpawnPoints())
		{
			const int32 Steps = Point.Route.Num() - 1;
			if (Steps > 0)
			{
				RouteShortest = RouteShortest > 0 ? FMath::Min(RouteShortest, Steps) : Steps;
				RouteLongest = FMath::Max(RouteLongest, Steps);
			}
		}
	}

	const FString DifficultyName = StaticEnum<EBDDifficulty>()->GetNameStringByValue(static_cast<int64>(Match.Difficulty));
	TArray<FColumn> Columns;
	const auto Add = [&Columns](const TCHAR* Name, const FString& Value) { Columns.Add({ Name, Value }); };
	const auto AddInt = [&Add](const TCHAR* Name, const int64 Value) { Add(Name, FString::Printf(TEXT("%lld"), Value)); };

	// Which match, which wave.
	Add(TEXT("MatchStart"), Ledger.StartedAt);
	Add(TEXT("Build"), BDBuildInfo::GetLabel());
	Add(TEXT("Mode"), Mode(*World));
	AddInt(TEXT("Seed"), Match.ObstacleSeed);
	Add(TEXT("Difficulty"), DifficultyName);
	AddInt(TEXT("Wave"), Wave);
	AddInt(TEXT("BossWave"), bCandidateOut ? 1 : 0);
	Add(TEXT("Ending"), Ending);
	Add(TEXT("HealthScale"), Decimal(Match.GetHealthScale()));

	// The count at the end of the wave, and how much of it this wave made.
	AddInt(TEXT("VotesBlue"), Now.VotesBlue);
	AddInt(TEXT("VotesRed"), Now.VotesRed);
	AddInt(TEXT("VotesNull"), Now.VotesNull);
	AddInt(TEXT("BlueDelta"), Now.VotesBlue - Before.VotesBlue);
	AddInt(TEXT("RedDelta"), Now.VotesRed - Before.VotesRed);
	AddInt(TEXT("NullDelta"), Now.VotesNull - Before.VotesNull);

	// The money of the stretch: the building phase before the wave and the wave.
	AddInt(TEXT("FundsEnd"), Now.Funds);
	AddInt(TEXT("FundsEarned"), Earned);
	AddInt(TEXT("FundsSpent"), Spent);
	AddInt(TEXT("FundsRefunded"), Refunded);
	AddInt(TEXT("FundsGranted"), Granted);
	AddInt(TEXT("FundsGap"), Gap);
	AddInt(TEXT("DividersInHand"), Match.GetDividersRemaining());

	// The board at the end of the wave.
	AddInt(TEXT("Towers"), Board.Towers);
	AddInt(TEXT("Characters"), Board.Characters);
	AddInt(TEXT("Platforms"), Board.Platforms);
	AddInt(TEXT("Dividers"), Board.Dividers);
	AddInt(TEXT("Defenders"), Board.Defenders);
	Add(TEXT("AvgLevel"), Decimal(Board.GetAverageLevel()));

	// The fight of this wave.
	AddInt(TEXT("CreepsSpawned"), Now.CreepsSpawned - Before.CreepsSpawned);
	AddInt(TEXT("CreepsKilled"), Now.CreepsKilled - Before.CreepsKilled);
	AddInt(TEXT("CreepsArrived"), Now.CreepsArrived - Before.CreepsArrived);
	AddInt(TEXT("CandidateOut"), bCandidateOut ? 1 : 0);
	AddInt(TEXT("CandidateKilled"), CandidatesKilled);
	AddInt(TEXT("CandidateArrived"), bCandidateArrived ? 1 : 0);
	AddInt(TEXT("DamageWasted"), FMath::RoundToInt64(Now.DamageWasted - Before.DamageWasted));
	AddInt(TEXT("PeakAlive"), PeakAlive);
	AddInt(TEXT("RouteShortest"), RouteShortest);
	AddInt(TEXT("RouteLongest"), RouteLongest);

	Ledger.WaveMark = Now;

	// One line, kept short: the detail is in the file. It still opens the way the old
	// scoreboard line did, so what reads "Wave N cleared. Votes:" keeps working.
	FString Candidate;
	if (bCandidateOut || CandidatesKilled > 0 || bCandidateArrived)
	{
		Candidate = FString::Printf(TEXT(" | candidate%s%s%s"),
			bCandidateOut ? TEXT(" out") : TEXT(""), CandidatesKilled > 0 ? TEXT(" killed") : TEXT(""), bCandidateArrived ? TEXT(" AT THE URN") : TEXT(""));
	}
	UE_LOG(LogBDMatch, Log, TEXT("Wave %d %s. Votes: blue %d (%+d), red %d (%+d), null %d (%+d) | funds %d (+%d, -%d) | %d defender(s) at %.2f | creeps %d killed, %d at the urn, peak %d%s | route %d-%d"),
		Wave, FCString::Stricmp(Ending, TEXT("Cleared")) == 0 ? TEXT("cleared") : TEXT("lost"),
		Now.VotesBlue, Now.VotesBlue - Before.VotesBlue, Now.VotesRed, Now.VotesRed - Before.VotesRed, Now.VotesNull, Now.VotesNull - Before.VotesNull,
		Now.Funds, Earned + Refunded + Granted, Spent, Board.Defenders, Board.GetAverageLevel(),
		Now.CreepsKilled - Before.CreepsKilled, Now.CreepsArrived - Before.CreepsArrived, PeakAlive, *Candidate, RouteShortest, RouteLongest);
	if (Gap != 0)
	{
		UE_LOG(LogBDMatch, Warning, TEXT("Wave %d: the money does not close, %lld unaccounted for since the last wave."), Wave, Gap);
	}

	if (GEnabled != 0)
	{
		AppendRow(GetCsvPath(), Columns);
	}
}

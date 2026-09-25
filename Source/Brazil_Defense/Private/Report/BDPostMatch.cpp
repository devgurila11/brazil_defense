// Brazil Defense. The post-match report: one row per match, to calibrate with numbers.

#include "Report/BDPostMatch.h"

#include "BDBuildInfo.h"
#include "BDLog.h"
#include "Candidate/BDCandidateSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Match/BDMatchManager.h"
#include "Misc/Paths.h"
#include "Placement/BDPlacementSettings.h"
#include "Report/BDReportCsv.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDPostMatchPrivate
{
	/** On by default: the rows are the point. Off for a session that should leave no trace. */
	static int32 GEnabled = 1;

	static FAutoConsoleVariableRef CVarEnabled(
		TEXT("BD.PostMatch.Enabled"),
		GEnabled,
		TEXT("1 (default) appends a row to Saved/Logs/PostMatch.csv at the end of every match; 0 writes nothing."),
		ECVF_Default);
}

FString BDPostMatch::GetCsvPath()
{
	return FPaths::Combine(FPaths::ProjectLogDir(), TEXT("PostMatch.csv"));
}

void BDPostMatch::Write(ABDMatchManager& Match, const TCHAR* Outcome)
{
	using namespace BDPostMatchPrivate;
	using namespace BDReportCsv;

	FBDMatchLedger& Ledger = Match.GetLedgerMutable();
	UWorld* World = Match.GetWorld();
	if (Ledger.bReportWritten || World == nullptr)
	{
		return;
	}
	Ledger.bReportWritten = true;

	//~ What stands on the board ----------------------------------------------------
	const FBDBoardTally Board = TallyBoard(*World);
	const int32 Towers = Board.Towers;
	const int32 Characters = Board.Characters;
	const int32 Platforms = Board.Platforms;
	const int32 Dividers = Board.Dividers;
	const int32 TopLevel = Board.TopLevel;

	//~ The rest of the match ---------------------------------------------------------
	const UBDCandidateSubsystem* Candidates = World->GetSubsystem<UBDCandidateSubsystem>();
	const UBDWaveSubsystem* Waves = World->GetSubsystem<UBDWaveSubsystem>();
	const FBDMatchCombatTotals Combat = Waves != nullptr ? Waves->GetMatchTotals() : FBDMatchCombatTotals();

	const int32 Blue = Match.GetVotesBlue();
	const int32 Red = Match.GetVotesRed();
	const float Damage = Combat.DamageDealt + Combat.DamageWasted;
	const float WastePercent = Damage > 0.0f ? 100.0f * Combat.DamageWasted / Damage : 0.0f;
	const int32 Spent = Ledger.SpentBuild + Ledger.SpentMove + Ledger.SpentEvolve;
	const int32 Left = Ledger.PublicMoneyAtEnd + Ledger.BribeHeldAtEnd;
	const double RealSeconds = FPlatformTime::Seconds() - Ledger.RealStartSeconds;
	const double GameSeconds = World->GetTimeSeconds() - Ledger.GameStartSeconds;

	TArray<FColumn> Columns;
	const auto Add = [&Columns](const TCHAR* Name, const FString& Value) { Columns.Add({ Name, Value }); };
	const auto AddInt = [&Add](const TCHAR* Name, const int64 Value) { Add(Name, FString::Printf(TEXT("%lld"), Value)); };

	// Who and how it ended.
	Add(TEXT("DateTime"), FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")));
	Add(TEXT("Mode"), Mode(*World));
	AddInt(TEXT("Seed"), Match.ObstacleSeed);
	Add(TEXT("Difficulty"), StaticEnum<EBDDifficulty>()->GetNameStringByValue(static_cast<int64>(Match.Difficulty)));
	Add(TEXT("Outcome"), Outcome);
	Add(TEXT("EndReason"), Quote(Ledger.EndReason));
	AddInt(TEXT("Wave"), Match.GetCurrentWave());
	AddInt(TEXT("WavesToWin"), Match.GetWavesToWin());
	AddInt(TEXT("Endless"), Match.IsEndless() ? 1 : 0);
	AddInt(TEXT("LoadedAtWave"), Ledger.LoadedAtWave);

	// The count.
	AddInt(TEXT("VotesBlue"), Blue);
	AddInt(TEXT("VotesRed"), Red);
	Add(TEXT("BlueToRed"), Red > 0 ? Decimal(static_cast<double>(Blue) / Red) : FString());
	Add(TEXT("BlueShare"), Blue + Red > 0 ? Decimal(100.0 * Blue / (Blue + Red)) : FString());
	AddInt(TEXT("VotesNull"), Match.GetVotesNull());

	// The money.
	AddInt(TEXT("StartingFunds"), Ledger.StartingFunds);
	AddInt(TEXT("FundsEarned"), Ledger.BribeEarned);
	AddInt(TEXT("FundsRefunded"), Ledger.Refunded);
	AddInt(TEXT("FundsGranted"), Ledger.Granted);
	AddInt(TEXT("SpentBuild"), Ledger.SpentBuild);
	AddInt(TEXT("SpentEvolve"), Ledger.SpentEvolve);
	AddInt(TEXT("SpentMove"), Ledger.SpentMove);
	AddInt(TEXT("FundsLeft"), Left);
	// Starting funds and everything that came in, less everything that went out, is what
	// should be left: a row where it is not has money that came from nowhere.
	AddInt(TEXT("LedgerGap"), static_cast<int64>(Ledger.StartingFunds) + Ledger.BribeEarned + Ledger.Refunded + Ledger.Granted - Spent - Left);

	// The board.
	AddInt(TEXT("Towers"), Towers);
	AddInt(TEXT("Characters"), Characters);
	AddInt(TEXT("Platforms"), Platforms);
	AddInt(TEXT("Dividers"), Dividers);
	AddInt(TEXT("DividersGranted"), Ledger.DividersGranted);
	AddInt(TEXT("DividersInHand"), Match.GetDividersRemaining());
	for (const TSoftObjectPtr<UBDPlaceableData>& Entry : UBDPlacementSettings::Get().Palette)
	{
		const FSoftObjectPath Path = Entry.ToSoftObjectPath();
		Add(*FString::Printf(TEXT("Count_%s"), *Path.GetAssetName()), FString::FromInt(Board.PerPiece.FindRef(Path)));
	}
	AddInt(TEXT("PiecesBuilt"), Ledger.PiecesBuilt);
	Add(TEXT("AvgLevel"), Decimal(Board.GetAverageLevel()));
	AddInt(TEXT("MaxLevel"), TopLevel);
	AddInt(TEXT("LevelsBought"), Ledger.LevelsBought);

	// The fight.
	AddInt(TEXT("CreepsSpawned"), Combat.CreepsSpawned);
	AddInt(TEXT("CreepsKilled"), Combat.CreepsKilled);
	AddInt(TEXT("CreepsArrived"), Combat.CreepsArrived);
	AddInt(TEXT("CandidatesSent"), Candidates != nullptr ? Candidates->GetCandidatesSent() : 0);
	AddInt(TEXT("CandidatesKilled"), Candidates != nullptr ? Candidates->GetFallenCount() : 0);
	AddInt(TEXT("CandidateAtUrn"), Candidates != nullptr ? Candidates->GetArrivedOrdinal() : 0);
	AddInt(TEXT("DamageDealt"), FMath::RoundToInt64(Combat.DamageDealt));
	AddInt(TEXT("DamageWasted"), FMath::RoundToInt64(Combat.DamageWasted));
	Add(TEXT("WastePercent"), Decimal(WastePercent));
	AddInt(TEXT("ShotsFired"), Combat.ShotsFired);
	AddInt(TEXT("LostShots"), Combat.LostShots);
	AddInt(TEXT("PeakAlive"), Combat.PeakAlive);

	// The pace.
	Add(TEXT("RealSeconds"), Decimal(RealSeconds));
	Add(TEXT("GameSeconds"), Decimal(GameSeconds));
	AddInt(TEXT("LastGrowthWave"), Ledger.LastGrowthWave);

	// Which binary played it: the answer to "was this on the adjusted build?".
	Add(TEXT("Build"), BDBuildInfo::GetLabel());

	//~ The summary, readable -----------------------------------------------------------
	UE_LOG(LogBDMatch, Log, TEXT("POST-MATCH %s (%s) on %s, seed %d, wave %d of %d: %s."),
		Outcome, Mode(*World), *StaticEnum<EBDDifficulty>()->GetNameStringByValue(static_cast<int64>(Match.Difficulty)),
		Match.ObstacleSeed, Match.GetCurrentWave(), Match.GetWavesToWin(), *Ledger.EndReason);
	UE_LOG(LogBDMatch, Log, TEXT("  count: %d blue, %d red, %d null."), Blue, Red, Match.GetVotesNull());
	UE_LOG(LogBDMatch, Log, TEXT("  money: %d to start + %d from bosses + %d refunded + %d granted; spent %d building, %d evolving, %d moving; %d left."),
		Ledger.StartingFunds, Ledger.BribeEarned, Ledger.Refunded, Ledger.Granted, Ledger.SpentBuild, Ledger.SpentEvolve, Ledger.SpentMove, Left);
	UE_LOG(LogBDMatch, Log, TEXT("  board: %d tower(s), %d character(s), %d platform(s), %d divider(s) (+%d granted, %d in hand); average level %.2f, top %d, %d level(s) bought; last grew on wave %d."),
		Towers, Characters, Platforms, Dividers, Ledger.DividersGranted, Match.GetDividersRemaining(), Board.GetAverageLevel(), TopLevel, Ledger.LevelsBought, Ledger.LastGrowthWave);
	UE_LOG(LogBDMatch, Log, TEXT("  fight: %d creep(s) killed, %d at the urn, peak %d alive; candidates %d sent, %d killed%s; %.0f damage, %.1f%% wasted; %.0fs real, %.0fs of game."),
		Combat.CreepsKilled, Combat.CreepsArrived, Combat.PeakAlive,
		Candidates != nullptr ? Candidates->GetCandidatesSent() : 0, Candidates != nullptr ? Candidates->GetFallenCount() : 0,
		Candidates != nullptr && Candidates->GetArrivedOrdinal() > 0 ? *FString::Printf(TEXT(", number %d at the urn"), Candidates->GetArrivedOrdinal()) : TEXT(""),
		Combat.DamageDealt, WastePercent, RealSeconds, GameSeconds);

	if (GEnabled == 0)
	{
		return;
	}

	//~ The row -------------------------------------------------------------------------
	const FString Path = GetCsvPath();
	if (AppendRow(Path, Columns))
	{
		UE_LOG(LogBDMatch, Log, TEXT("  row appended to %s."), *FPaths::ConvertRelativePathToFull(Path));
	}
}

// Brazil Defense. The post-match report: one row per match, to calibrate with numbers.

#include "Report/BDPostMatch.h"

#include "BDLog.h"
#include "Candidate/BDCandidateSubsystem.h"
#include "Debug/BDSimSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Match/BDMatchManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementComponent.h"
#include "Placement/BDPlacementSettings.h"
#include "Save/BDMatchSave.h"
#include "Tower/BDTowerBase.h"
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

	/** One column: its header and the value of this match. */
	struct FColumn
	{
		FString Name;
		FString Value;
	};

	/** A text value, quoted, with its own quotes doubled: reasons have commas in them. */
	static FString Quote(const FString& Text)
	{
		return TEXT("\"") + Text.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
	}

	/** Two decimals with a point, whatever the machine's locale: the sheet has to read them. */
	static FString Decimal(const double Value)
	{
		return FString::Printf(TEXT("%.2f"), Value);
	}

	/** Played on the screen, played headless (a console run with no window), or played by BD.Sim.Run. */
	static const TCHAR* Mode(const UWorld& World)
	{
		const UBDSimSubsystem* Sim = World.GetSubsystem<UBDSimSubsystem>();
		if (Sim != nullptr && Sim->IsRunning())
		{
			return TEXT("Sim");
		}
		return FApp::CanEverRender() ? TEXT("Screen") : TEXT("Headless");
	}
}

FString BDPostMatch::GetCsvPath()
{
	return FPaths::Combine(FPaths::ProjectLogDir(), TEXT("PostMatch.csv"));
}

void BDPostMatch::Write(ABDMatchManager& Match, const TCHAR* Outcome)
{
	using namespace BDPostMatchPrivate;

	FBDMatchLedger& Ledger = Match.GetLedgerMutable();
	UWorld* World = Match.GetWorld();
	if (Ledger.bReportWritten || World == nullptr)
	{
		return;
	}
	Ledger.bReportWritten = true;

	//~ What stands on the board ----------------------------------------------------
	// Read off the saved form of the board, which already counts a wide piece once and
	// leaves the urn apart; per palette entry, so every kind of platform has its column.
	TMap<FSoftObjectPath, int32> PerPiece;
	int32 Towers = 0;
	int32 Characters = 0;
	int32 Platforms = 0;
	int32 Dividers = 0;
	const APlayerController* Controller = World->GetFirstPlayerController();
	const UBDPlacementComponent* Placement = Controller != nullptr ? Controller->FindComponentByClass<UBDPlacementComponent>() : nullptr;
	if (Placement != nullptr)
	{
		TArray<FBDSavedPiece> Pieces;
		Placement->CaptureBoard(Pieces);
		for (const FBDSavedPiece& Piece : Pieces)
		{
			const UBDPlaceableData* Data = Cast<UBDPlaceableData>(Piece.Data.ResolveObject());
			const EBDPieceKind Kind = Data != nullptr ? Data->GetPieceKind() : EBDPieceKind::Objective;
			if (Kind == EBDPieceKind::Objective)
			{
				continue;
			}
			++PerPiece.FindOrAdd(Piece.Data);
			Towers += Kind == EBDPieceKind::Tower ? 1 : 0;
			Characters += Kind == EBDPieceKind::Character ? 1 : 0;
			Platforms += Kind == EBDPieceKind::Platform ? 1 : 0;
			Dividers += Kind == EBDPieceKind::Divider ? 1 : 0;
		}
	}

	int32 Defenders = 0;
	int32 LevelSum = 0;
	int32 TopLevel = 0;
	for (TActorIterator<ABDTowerBase> It(World); It; ++It)
	{
		++Defenders;
		LevelSum += It->GetTowerLevel();
		TopLevel = FMath::Max(TopLevel, It->GetTowerLevel());
	}

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
	for (const TSoftObjectPtr<UBDPlaceableData>& Entry : UBDPlacementSettings::Get().Palette)
	{
		const FSoftObjectPath Path = Entry.ToSoftObjectPath();
		Add(*FString::Printf(TEXT("Count_%s"), *Path.GetAssetName()), FString::FromInt(PerPiece.FindRef(Path)));
	}
	AddInt(TEXT("PiecesBuilt"), Ledger.PiecesBuilt);
	Add(TEXT("AvgLevel"), Decimal(Defenders > 0 ? static_cast<double>(LevelSum) / Defenders : 0.0));
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

	//~ The summary, readable -----------------------------------------------------------
	UE_LOG(LogBDMatch, Log, TEXT("POST-MATCH %s (%s) on %s, seed %d, wave %d of %d: %s."),
		Outcome, Mode(*World), *StaticEnum<EBDDifficulty>()->GetNameStringByValue(static_cast<int64>(Match.Difficulty)),
		Match.ObstacleSeed, Match.GetCurrentWave(), Match.GetWavesToWin(), *Ledger.EndReason);
	UE_LOG(LogBDMatch, Log, TEXT("  count: %d blue, %d red, %d null."), Blue, Red, Match.GetVotesNull());
	UE_LOG(LogBDMatch, Log, TEXT("  money: %d to start + %d from bosses + %d refunded + %d granted; spent %d building, %d evolving, %d moving; %d left."),
		Ledger.StartingFunds, Ledger.BribeEarned, Ledger.Refunded, Ledger.Granted, Ledger.SpentBuild, Ledger.SpentEvolve, Ledger.SpentMove, Left);
	UE_LOG(LogBDMatch, Log, TEXT("  board: %d tower(s), %d character(s), %d platform(s), %d divider(s); average level %.2f, top %d, %d level(s) bought; last grew on wave %d."),
		Towers, Characters, Platforms, Dividers, Defenders > 0 ? static_cast<float>(LevelSum) / Defenders : 0.0f, TopLevel, Ledger.LevelsBought, Ledger.LastGrowthWave);
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
	FString Header;
	FString Row;
	for (int32 Index = 0; Index < Columns.Num(); ++Index)
	{
		const TCHAR* Separator = Index > 0 ? TEXT(",") : TEXT("");
		Header += Separator + Columns[Index].Name;
		Row += Separator + Columns[Index].Value;
	}

	// A file whose header is not this one describes other columns: it is kept aside
	// under a dated name rather than written under.
	const FString Path = GetCsvPath();
	IFileManager& Files = IFileManager::Get();
	bool bNeedsHeader = !Files.FileExists(*Path);
	if (!bNeedsHeader)
	{
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *Path);
		if (Lines.Num() == 0 || Lines[0] != Header)
		{
			const FString Aside = FPaths::Combine(FPaths::ProjectLogDir(), FString::Printf(TEXT("PostMatch-%s.csv"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))));
			Files.Move(*Aside, *Path);
			UE_LOG(LogBDMatch, Log, TEXT("  the columns changed: the old report was kept as %s."), *Aside);
			bNeedsHeader = true;
		}
	}

	const FString Text = (bNeedsHeader ? Header + LINE_TERMINATOR : FString()) + Row + LINE_TERMINATOR;
	if (FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &Files, FILEWRITE_Append))
	{
		UE_LOG(LogBDMatch, Log, TEXT("  row appended to %s."), *FPaths::ConvertRelativePathToFull(Path));
	}
	else
	{
		UE_LOG(LogBDMatch, Error, TEXT("  the post-match row could not be written to %s."), *Path);
	}
}

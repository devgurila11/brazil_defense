// Brazil Defense. Console access to the bribe: the ledger, and whether its curve holds.

#include "BDLog.h"
#include "Bribe/BDBribeSubsystem.h"
#include "Enemy/BDEnemyData.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Match/BDDifficultyData.h"
#include "Match/BDGameBalanceSettings.h"
#include "Match/BDMatchManager.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementSettings.h"
#include "Tower/BDTowerBase.h"
#include "Tower/BDTowerData.h"
#include "Wave/BDWaveSettings.h"

namespace BDBribeDebug
{
	static ABDMatchManager* FindMatch(const UWorld* World)
	{
		ABDMatchManager* Match = ABDMatchManager::Get(World);
		if (Match == nullptr)
		{
			UE_LOG(LogBDBribe, Error, TEXT("This command needs a running match."));
		}
		return Match;
	}

	static void ExecStatus(const TArray<FString>& Args, UWorld* World)
	{
		const ABDMatchManager* Match = FindMatch(World);
		const UBDBribeSubsystem* Bribes = UBDBribeSubsystem::Get(World);
		if (Match == nullptr)
		{
			return;
		}

		UE_LOG(LogBDBribe, Log, TEXT("Bribe: %d held by the thief, %d public money to spend. %s"),
			Match->GetBribeHeld(), Match->GetPublicMoney(),
			Bribes != nullptr && Bribes->IsConverting()
				? *FString::Printf(TEXT("A conversion is running, %.0f%% across."), Bribes->GetConversionProgress() * 100.0f)
				: TEXT("Nothing converting."));
	}

	static void ExecGrant(const TArray<FString>& Args, UWorld* World)
	{
		ABDMatchManager* Match = FindMatch(World);
		if (Match == nullptr)
		{
			return;
		}

		const int32 Amount = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1000;
		Match->AddPublicMoney(Amount, TEXT("BD.Bribe.Grant"));
	}

	static void ExecFlush(const TArray<FString>& Args, UWorld* World)
	{
		if (UBDBribeSubsystem* Bribes = UBDBribeSubsystem::Get(World))
		{
			Bribes->FlushNow(TEXT("BD.Bribe.Flush"));
		}
	}

	/**
	 * The whole economy of a match on one screen: what every scheduled boss drops, what
	 * has been dropped by then, and where that meets the price of a defense with every
	 * piece at the top level.
	 *
	 * The three things the curve has to do, which the last lines check:
	 *   every boss buys at least one level of something;
	 *   the middle of the match cannot afford everything;
	 *   the whole defense at level 5 becomes payable in the last stretch, not before.
	 *
	 * With an argument, that argument is the wave the full defense should become payable
	 * on, and the last line says which HealthPerBribe would put it there.
	 */
	static void ExecReport(const TArray<FString>& Args, UWorld* World)
	{
		const ABDMatchManager* Match = FindMatch(World);
		const UBDEnemyData* Enemy = UBDWaveSettings::Get().ResolveWaveEnemy();
		if (Match == nullptr || Enemy == nullptr)
		{
			UE_LOG(LogBDBribe, Error, TEXT("BD.Bribe.Report needs a match and a wave enemy."));
			return;
		}

		const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
		const int32 Interval = FMath::Max(1, Balance.CandidateInterval);
		const int32 WavesToWin = Match->GetWavesToWin();

		// The price of the defense: a full board of defenders, each taken from level 1 to
		// the top, at the average upgrade cost base of the palette. Nothing caps how many
		// get built, so "full" is the reference count of the balance settings.
		int32 CostBaseTotal = 0;
		int32 CostBaseKinds = 0;
		for (const TSoftObjectPtr<UBDPlaceableData>& Entry : UBDPlacementSettings::Get().Palette)
		{
			const UBDPlaceableData* Data = Entry.LoadSynchronous();
			const UBDTowerData* TowerData = Data != nullptr ? Data->TowerData.LoadSynchronous() : nullptr;
			if (TowerData != nullptr)
			{
				CostBaseTotal += TowerData->UpgradeCostBase;
				++CostBaseKinds;
			}
		}
		if (CostBaseKinds == 0)
		{
			UE_LOG(LogBDBribe, Error, TEXT("BD.Bribe.Report needs at least one defender in the palette."));
			return;
		}
		const int32 AverageCostBase = CostBaseTotal / CostBaseKinds;
		const int32 PerDefender = Balance.GetEvolutionSpent(AverageCostBase, UBDTowerData::MaxLevels);
		const int32 FirstStep = Balance.GetUpgradeCost(AverageCostBase, 2);

		const int32 Bosses = WavesToWin / Interval;
		const int32 ReferenceDefenders = FMath::Max(0, Balance.ReferenceTowerCount) + FMath::Max(0, Balance.ReferenceCharacterCount);

		// Two denominators, because they answer different questions and one on its own
		// lies: the defense standing on the board right now is what the player is actually
		// paying to evolve, and the reference is what a full board is taken to be - there
		// is no ceiling any more, so that number is told to the report rather than read
		// off a budget. A board that never grows is measured against the first; the report
		// prints both rather than pretending one is the truth.
		int32 OnBoard = 0;
		if (World != nullptr)
		{
			for (TActorIterator<ABDTowerBase> It(World); It; ++It)
			{
				++OnBoard;
			}
		}
		const int32 Defenders = OnBoard > 0 ? OnBoard : ReferenceDefenders;
		const int64 FullDefense = static_cast<int64>(Defenders) * PerDefender;
		const int64 ReferenceCost = static_cast<int64>(ReferenceDefenders) * PerDefender;

		UE_LOG(LogBDBribe, Log, TEXT("Bribe report on %s: a boss every %d waves, %d of them to wave %d. Rate: 1 bribe per %.0f health, %.0f%% back on removal."),
			*UEnum::GetValueAsString(Match->Difficulty), Interval, Bosses, WavesToWin,
			Balance.HealthPerBribe, Balance.EvolutionRefundRatio * 100.0f);
		UE_LOG(LogBDBribe, Log, TEXT("  measured against the %d defender(s) on the board: %d each to reach level %d = %lld public money. One level costs %d at the cheapest."),
			Defenders, PerDefender, UBDTowerData::MaxLevels, FullDefense, FirstStep);
		UE_LOG(LogBDBribe, Log, TEXT("  the reference full board: %d defender(s) (%d tower(s) + %d character(s)) = %lld public money."),
			ReferenceDefenders, Balance.ReferenceTowerCount, Balance.ReferenceCharacterCount, ReferenceCost);

		int64 Cumulative = 0;
		int32 PayableOnWave = 0;
		int32 HalfwayPercent = 0;
		bool bEveryBossBuysSomething = true;
		for (int32 Index = 1; Index <= Bosses; ++Index)
		{
			const int32 Wave = Index * Interval;
			const float Health = Balance.GetCandidateHealth(Enemy->MaxHealth, Wave);
			const int32 Drop = Balance.BribeForHealth(Health);
			Cumulative += Drop;
			if (Drop < FirstStep)
			{
				bEveryBossBuysSomething = false;
			}
			if (PayableOnWave == 0 && Cumulative >= FullDefense)
			{
				PayableOnWave = Wave;
			}
			if (Wave * 2 == WavesToWin || (HalfwayPercent == 0 && Wave * 2 >= WavesToWin))
			{
				HalfwayPercent = FullDefense > 0 ? static_cast<int32>(Cumulative * 100 / FullDefense) : 0;
			}

			UE_LOG(LogBDBribe, Log, TEXT("  boss %2d on wave %3d: %9.0f hp -> %8d bribe | %10lld earned so far, %3d%% of the full defense, %d level(s) affordable"),
				Index, Wave, Health, Drop, Cumulative,
				FullDefense > 0 ? static_cast<int32>(Cumulative * 100 / FullDefense) : 0,
				FirstStep > 0 ? static_cast<int32>(Cumulative / FirstStep) : 0);
		}

		UE_LOG(LogBDBribe, Log, TEXT("  every boss buys at least one level: %s. Halfway through the match the player holds %d%% of the full defense. The whole defense at level %d becomes payable %s."),
			bEveryBossBuysSomething ? TEXT("yes") : TEXT("NO - the early bosses drop less than one level"),
			HalfwayPercent, UBDTowerData::MaxLevels,
			PayableOnWave > 0 ? *FString::Printf(TEXT("on wave %d"), PayableOnWave) : TEXT("NEVER inside the match"));

		// What rate would land it on a chosen wave, so the calibration is read off rather
		// than guessed at.
		const int32 TargetWave = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
		if (TargetWave <= 0)
		{
			UE_LOG(LogBDBribe, Log, TEXT("  BD.Bribe.Report <wave> says which HealthPerBribe would make the full defense payable on that wave."));
			return;
		}

		// The bribe scales as 1 / HealthPerBribe, so the rate that lands the target is the
		// current one scaled by what was earned by then against what is needed.
		double EarnedByTarget = 0.0;
		for (int32 Index = 1; Index <= Bosses; ++Index)
		{
			const int32 Wave = Index * Interval;
			if (Wave > TargetWave)
			{
				break;
			}
			EarnedByTarget += Balance.GetCandidateHealth(Enemy->MaxHealth, Wave);
		}
		if (EarnedByTarget <= 0.0 || FullDefense <= 0)
		{
			UE_LOG(LogBDBribe, Warning, TEXT("  no boss falls on or before wave %d: nothing to solve for."), TargetWave);
			return;
		}

		UE_LOG(LogBDBribe, Log, TEXT("  to make the full defense payable by wave %d: HealthPerBribe = %.1f (it is %.1f)."),
			TargetWave, EarnedByTarget / static_cast<double>(FullDefense), Balance.HealthPerBribe);
	}

	/**
	 * The economy in one place: what each piece costs and when it comes into the hand,
	 * and, boss by boss, what a candidate drops against what a piece and a level cost on
	 * that wave. The two lines at the end are the question the price is calibrated for:
	 * can the twenty bosses pay for a full board AND a full evolution? The answer is meant
	 * to be no - they pay for one or the other, and the player chooses.
	 */
	static void ExecEconomyReport(const TArray<FString>& Args, UWorld* World)
	{
		const ABDMatchManager* Match = FindMatch(World);
		if (Match == nullptr || Match->GetDifficultyData() == nullptr)
		{
			return;
		}

		const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
		const int32 Interval = FMath::Max(1, Balance.CandidateInterval);
		const int32 WavesToWin = Match->GetWavesToWin();
		const int32 StartingFunds = Match->GetDifficultyData()->StartingFunds;

		TArray<const UBDPlaceableData*> Pieces;
		for (const TSoftObjectPtr<UBDPlaceableData>& Entry : UBDPlacementSettings::Get().Palette)
		{
			if (const UBDPlaceableData* Data = Entry.LoadSynchronous())
			{
				Pieces.Add(Data);
			}
		}

		UE_LOG(LogBDBribe, Log, TEXT("Economy on %s: %d public money to start, a piece of base %d costs %.2f candidate(s), boss every %d waves. Votes are never spent."),
			*UEnum::GetValueAsString(Match->Difficulty), StartingFunds, Balance.ReplacementReferenceCost, Balance.ReplacementCostRatio, Interval);

		int32 UpgradeBaseTotal = 0;
		int32 UpgradeBaseKinds = 0;
		for (const UBDPlaceableData* Data : Pieces)
		{
			const UBDTowerData* TowerData = Data->TowerData.LoadSynchronous();
			if (Data->IsDefender() && TowerData != nullptr)
			{
				UpgradeBaseTotal += TowerData->UpgradeCostBase;
				++UpgradeBaseKinds;
			}
			UE_LOG(LogBDBribe, Log, TEXT("  piece %-14s %-9s base %4d | unlocks after wave %3d | price wave 1: %6d, wave 50: %6d, wave 100: %6d"),
				*Data->GetName(), *StaticEnum<EBDPieceKind>()->GetNameStringByValue(static_cast<int64>(Data->GetPieceKind())),
				Data->GetBuildCost(), ABDMatchManager::GetUnlockWave(Data),
				Match->GetBuildPriceOnWave(Data, 1), Match->GetBuildPriceOnWave(Data, 50), Match->GetBuildPriceOnWave(Data, 100));
		}
		const int32 UpgradeBase = UpgradeBaseKinds > 0 ? UpgradeBaseTotal / UpgradeBaseKinds : 0;

		// A reference piece is a defender at the reference cost, whatever the palette says:
		// it is the unit the calibration talks in.
		const UBDEnemyData* Creep = UBDWaveSettings::Get().ResolveWaveEnemy();
		const float CreepHealth = Creep != nullptr ? Creep->MaxHealth : 0.0f;
		int64 Income = StartingFunds;
		int64 BoardCost = 0;
		for (int32 Wave = Interval; Wave <= WavesToWin; Wave += Interval)
		{
			const int32 Drop = Match->GetCandidateFunds(Wave);
			const int32 Reference = Balance.GetReplacementCost(Balance.ReplacementReferenceCost, CreepHealth, Wave);
			const int32 FirstLevel = Balance.GetUpgradeCostOnWave(UpgradeBase, 2, Wave);
			Income += Drop;

			int32 Available = 0;
			for (const UBDPlaceableData* Data : Pieces)
			{
				Available += ABDMatchManager::GetUnlockWave(Data) <= Wave ? 1 : 0;
			}
			UE_LOG(LogBDBribe, Log, TEXT("  boss on wave %3d drops %6d | a reference piece costs %6d (%.2f of the drop), a first level %4d (%.2f) | %d of %d pieces in the hand | %lld earned so far"),
				Wave, Drop, Reference, Drop > 0 ? static_cast<float>(Reference) / Drop : 0.0f,
				FirstLevel, Drop > 0 ? static_cast<float>(FirstLevel) / Drop : 0.0f, Available, Pieces.Num(), Income);
		}

		// The full board at the price of the middle of the match - the waves it is actually
		// bought over - and the full evolution of it, against everything the match pays.
		const int32 Mid = FMath::Max(1, WavesToWin / 2);
		const int32 Defenders = FMath::Max(0, Balance.ReferenceTowerCount) + FMath::Max(0, Balance.ReferenceCharacterCount);
		// How many of each kind a full board holds, shared evenly over the pieces of that kind.
		TMap<EBDPieceKind, int32> KindsInPalette;
		for (const UBDPlaceableData* Data : Pieces)
		{
			++KindsInPalette.FindOrAdd(Data->GetPieceKind());
		}
		const TMap<EBDPieceKind, int32> FullBoard = {
			{ EBDPieceKind::Tower, Balance.ReferenceTowerCount },
			{ EBDPieceKind::Character, Balance.ReferenceCharacterCount },
			{ EBDPieceKind::Platform, Match->GetDifficultyData()->PlatformBudget },
			{ EBDPieceKind::Divider, Match->GetDifficultyData()->DividerBudget } };
		for (const UBDPlaceableData* Data : Pieces)
		{
			const EBDPieceKind Kind = Data->GetPieceKind();
			const int32 Count = FullBoard.FindRef(Kind) / FMath::Max(1, KindsInPalette.FindRef(Kind));
			BoardCost += static_cast<int64>(Count) * Match->GetBuildPriceOnWave(Data, Mid);
		}
		const int64 EvolutionCost = static_cast<int64>(Defenders) * FMath::RoundToInt(Balance.GetEvolutionSpent(UpgradeBase, UBDTowerData::MaxLevels) * Balance.GetPriceScale(Mid));

		UE_LOG(LogBDBribe, Log, TEXT("  the match pays %lld in all (start + %d bosses). At wave %d prices: a full board (%d defenders, the platform and divider hands) %lld, evolving those %d defenders to level %d %lld."),
			Income, WavesToWin / Interval, Mid, Defenders, BoardCost, Defenders, UBDTowerData::MaxLevels, EvolutionCost);
		// The divider hand, which is not money: what it holds by a few waves along the match
		// if every wave is cleared and every candidate killed.
		const UBDDifficultyData* DifficultyData = Match->GetDifficultyData();
		FString Hand;
		for (const int32 Wave : { 10, 25, 50, 75, 100 })
		{
			const int32 Bosses = Wave / Interval;
			int32 Dividers = DifficultyData->DividerBudget + DifficultyData->DividersPerWave * Wave;
			for (int32 Ordinal = 1; Ordinal <= Bosses; ++Ordinal)
			{
				Dividers += DifficultyData->GetDividersForCandidate(Ordinal);
			}
			Hand += FString::Printf(TEXT(" wave %d: %d;"), Wave, Dividers);
		}
		UE_LOG(LogBDBribe, Log, TEXT("  dividers are their own hand, no money: %d to start, +%d per wave, +%d for the first boss and %d more for each later one. Held by then, all placed:%s"),
			DifficultyData->DividerBudget, DifficultyData->DividersPerWave, DifficultyData->DividersPerCandidate, DifficultyData->DividersPerCandidateStep, *Hand);

		UE_LOG(LogBDBribe, Log, TEXT("  board AND evolution = %lld, %.0f%% of what the match pays: %s."),
			BoardCost + EvolutionCost, Income > 0 ? 100.0 * (BoardCost + EvolutionCost) / Income : 0.0,
			BoardCost + EvolutionCost > Income ? TEXT("the player has to choose") : TEXT("BOTH are affordable, the choice is gone"));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdEconomyReport(
		TEXT("BD.Economy.Report"),
		TEXT("BD.Economy.Report: prices and unlock waves of every piece, and boss by boss what a candidate drops against what a piece and a level cost."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecEconomyReport));

	static FAutoConsoleCommandWithWorldAndArgs CmdStatus(
		TEXT("BD.Bribe.Status"),
		TEXT("BD.Bribe.Status: logs what the thief holds, what the mint holds and whether a conversion is running."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));

	static FAutoConsoleCommandWithWorldAndArgs CmdGrant(
		TEXT("BD.Bribe.Grant"),
		TEXT("BD.Bribe.Grant [amount]: pays public money straight into the mint, for testing evolution without playing to a boss."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecGrant));

	static FAutoConsoleCommandWithWorldAndArgs CmdFlush(
		TEXT("BD.Bribe.Flush"),
		TEXT("BD.Bribe.Flush: finishes the running conversion and everything queued at once, with no bag and no counting."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecFlush));

	static FAutoConsoleCommandWithWorldAndArgs CmdReport(
		TEXT("BD.Bribe.Report"),
		TEXT("BD.Bribe.Report [wave]: what every boss drops against the price of a fully evolved defense; with a wave, the rate that would make it payable then."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecReport));
}

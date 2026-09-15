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
		const UBDDifficultyData* Difficulty = Match->GetDifficultyData();
		const int32 Interval = FMath::Max(1, Balance.CandidateInterval);
		const int32 WavesToWin = Match->GetWavesToWin();

		// The price of the defense: every defender the budgets ever allow, each taken from
		// level 1 to the top, at the average upgrade cost base of the palette. The bosses
		// raise the ceilings as they fall, so the fleet the player ends with is bigger
		// than the one they started with.
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
		const int32 StartingDefenders = (Difficulty != nullptr ? Difficulty->TowerBudget + Difficulty->CharacterBudget : 0);
		const int32 GrantedDefenders = Bosses * (FMath::Max(0, Balance.TowerBudgetPerBoss) + FMath::Max(0, Balance.CharacterBudgetPerBoss));

		// Two denominators, because they answer different questions and the ceiling on its
		// own lies: the defense standing on the board right now is what the player is
		// actually paying to evolve, and the ceiling is what it could become if every
		// budget the bosses hand out is built. A board that never grows is measured
		// against the first; the report prints both rather than pretending one is the
		// truth.
		int32 OnBoard = 0;
		if (World != nullptr)
		{
			for (TActorIterator<ABDTowerBase> It(World); It; ++It)
			{
				++OnBoard;
			}
		}
		const int32 Ceiling = StartingDefenders + GrantedDefenders;
		const int32 Defenders = OnBoard > 0 ? OnBoard : Ceiling;
		const int64 FullDefense = static_cast<int64>(Defenders) * PerDefender;
		const int64 CeilingCost = static_cast<int64>(Ceiling) * PerDefender;

		UE_LOG(LogBDBribe, Log, TEXT("Bribe report on %s: a boss every %d waves, %d of them to wave %d. Rate: 1 bribe per %.0f health, %.0f%% back on removal."),
			*UEnum::GetValueAsString(Match->Difficulty), Interval, Bosses, WavesToWin,
			Balance.HealthPerBribe, Balance.EvolutionRefundRatio * 100.0f);
		UE_LOG(LogBDBribe, Log, TEXT("  measured against the %d defender(s) on the board: %d each to reach level %d = %lld public money. One level costs %d at the cheapest."),
			Defenders, PerDefender, UBDTowerData::MaxLevels, FullDefense, FirstStep);
		UE_LOG(LogBDBribe, Log, TEXT("  the ceiling, if every granted budget is built: %d defender(s) (%d from the difficulty + %d from the bosses) = %lld public money."),
			Ceiling, StartingDefenders, GrantedDefenders, CeilingCost);

		int64 Cumulative = 0;
		int32 PayableOnWave = 0;
		int32 HalfwayPercent = 0;
		bool bEveryBossBuysSomething = true;
		for (int32 Index = 1; Index <= Bosses; ++Index)
		{
			const int32 Wave = Index * Interval;
			const float Health = FMath::Max(1.0f, Enemy->MaxHealth * Balance.GetHealthScale(Wave) * FMath::Max(1.0f, Balance.CandidateHealthMultiplier));
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
			EarnedByTarget += Enemy->MaxHealth * Balance.GetHealthScale(Wave) * FMath::Max(1.0f, Balance.CandidateHealthMultiplier);
		}
		if (EarnedByTarget <= 0.0 || FullDefense <= 0)
		{
			UE_LOG(LogBDBribe, Warning, TEXT("  no boss falls on or before wave %d: nothing to solve for."), TargetWave);
			return;
		}

		UE_LOG(LogBDBribe, Log, TEXT("  to make the full defense payable by wave %d: HealthPerBribe = %.1f (it is %.1f)."),
			TargetWave, EarnedByTarget / static_cast<double>(FullDefense), Balance.HealthPerBribe);
	}

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

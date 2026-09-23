// Brazil Defense. Debug tooling: a whole match played without a player, to be measured.

#include "Debug/BDSimSubsystem.h"

#include "BDLog.h"
#include "Candidate/BDCandidateSubsystem.h"
#include "Debug/BDDebugAutoSetup.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Match/BDGameBalanceSettings.h"
#include "Match/BDMatchManager.h"
#include "Misc/App.h"
#include "Stats/Stats.h"
#include "Tower/BDTowerBase.h"
#include "Tower/BDTowerData.h"

namespace BDSimPrivate
{
	/** A run that has taken this long in real seconds is reported as stalled rather than waited on. */
	static constexpr float MaxRealSeconds = 3600.0f;
}

void UBDSimSubsystem::Deinitialize()
{
	if (ABDMatchManager* Match = BoundMatch.Get())
	{
		Match->OnWaveStarted.Remove(WaveStartedHandle);
	}
	BoundMatch.Reset();
	bRunning = false;

	Super::Deinitialize();
}

TStatId UBDSimSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBDSimSubsystem, STATGROUP_Tickables);
}

UBDSimSubsystem* UBDSimSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World != nullptr ? World->GetSubsystem<UBDSimSubsystem>() : nullptr;
}

ABDMatchManager* UBDSimSubsystem::GetMatch() const
{
	return ABDMatchManager::Get(GetWorld());
}

//~ Starting and stopping -----------------------------------------------------------

void UBDSimSubsystem::Start(const int32 InSeed, const int32 Waves, const bool bInEvolve, const float Speed, const bool bQuitWhenDone)
{
	UWorld* World = GetWorld();
	ABDMatchManager* Match = GetMatch();
	UBDDebugAutoSetup* Setup = World != nullptr ? World->GetSubsystem<UBDDebugAutoSetup>() : nullptr;
	if (Match == nullptr || Setup == nullptr)
	{
		UE_LOG(LogBDDebug, Error, TEXT("SIM needs a running match and the auto setup subsystem."));
		return;
	}

	Seed = InSeed;
	TargetWaves = FMath::Max(1, Waves);
	bEvolve = bInEvolve;
	bQuit = bQuitWhenDone;
	RealSecondsElapsed = 0.0f;
	FirstEvolutionWave = 0;
	LevelsBought = 0;
	MoneySpent = 0;
	LastSampledWave = -1;

	// The board of the run: the seed is the obstacle seed too, so one number brings the
	// whole scenario back. Every defender starts at level 1 whether or not the run
	// evolves; what separates the two runs is only what happens after.
	Setup->Run(Seed, /*DefenderLevel*/ 1);

	WaveStartedHandle = Match->OnWaveStarted.AddUObject(this, &UBDSimSubsystem::HandleWaveStarted);
	BoundMatch = Match;

	Match->SetGameSpeed(Speed);
	bRunning = true;

	int32 Defenders = 0;
	float AverageLevel = 0.0f;
	GetDefense(Defenders, AverageLevel);

	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
	UE_LOG(LogBDDebug, Log, TEXT("SIM START seed=%d waves=%d evolve=%d speed=%.0f defenders=%d difficulty=%s healthgrowth=%.3f healthperbribe=%.2f bossmult=%.0f"),
		Seed, TargetWaves, bEvolve ? 1 : 0, Speed, Defenders, *UEnum::GetValueAsString(Match->Difficulty),
		Balance.HealthScaleGrowth, Balance.HealthPerBribe, Balance.CandidateHealthMultiplier);
}

void UBDSimSubsystem::Stop()
{
	if (ABDMatchManager* Match = BoundMatch.Get())
	{
		Match->OnWaveStarted.Remove(WaveStartedHandle);
	}
	BoundMatch.Reset();
	bRunning = false;

	if (bQuit)
	{
		FPlatformMisc::RequestExit(false);
	}
}

//~ The run -------------------------------------------------------------------------

void UBDSimSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bRunning)
	{
		return;
	}

	// Real seconds, not dilated ones: this is the wall clock the run is capped against.
	RealSecondsElapsed += static_cast<float>(FApp::GetDeltaTime());
	if (RealSecondsElapsed > BDSimPrivate::MaxRealSeconds)
	{
		Report(TEXT("STALLED"));
		Stop();
		return;
	}

	ABDMatchManager* Match = GetMatch();
	if (Match == nullptr)
	{
		return;
	}

	if (Match->IsMatchOver())
	{
		Report(Match->GetPhase() == EBDMatchPhase::Victory ? TEXT("VICTORY") : TEXT("DEFEAT"));
		Stop();
		return;
	}

	if (Match->GetPhase() != EBDMatchPhase::Building)
	{
		return;
	}

	// The cap: reported from the building phase after the wave, so the last wave is in.
	if (Match->GetCurrentWave() >= TargetWaves)
	{
		Report(TEXT("WAVE CAP"));
		Stop();
		return;
	}

	// What the player would do between waves: put the public money the bosses paid in on the
	// board, then spend what is left on levels, then stop waiting. Building is first
	// because a new defender at level 1 is worth more than a level on an old one, and
	// because a player who just earned the price of one buys it.
	if (UBDDebugAutoSetup* Setup = GetWorld()->GetSubsystem<UBDDebugAutoSetup>())
	{
		Setup->BuildGrantedBudget();
	}

	if (bEvolve)
	{
		int32 Spent = 0;
		const int32 Bought = AutoEvolve(Spent);
		if (Bought > 0)
		{
			LevelsBought += Bought;
			MoneySpent += Spent;
			if (FirstEvolutionWave == 0)
			{
				FirstEvolutionWave = Match->GetCurrentWave() + 1;
				UE_LOG(LogBDDebug, Log, TEXT("SIM FIRSTEVOLUTION seed=%d wave=%d levels=%d spent=%d"),
					Seed, FirstEvolutionWave, Bought, Spent);
			}
		}
	}

	Match->CallWaveEarly();
}

int32 UBDSimSubsystem::AutoEvolve(int32& OutSpent)
{
	UWorld* World = GetWorld();
	OutSpent = 0;
	if (World == nullptr)
	{
		return 0;
	}

	// Cheapest first: a player spreading their money evenly rather than pouring it into
	// one defender, which is also what the exponential cost curve pushes towards.
	// CanUpgrade already refuses what cannot be paid for, what is at the cap and what
	// the block rule of a platform holds back, so the loop ends on its own.
	int32 Bought = 0;
	for (;;)
	{
		ABDTowerBase* Cheapest = nullptr;
		int32 BestCost = MAX_int32;
		for (TActorIterator<ABDTowerBase> It(World); It; ++It)
		{
			FString Refused;
			if (!It->CanUpgrade(Refused))
			{
				continue;
			}

			const int32 Cost = It->GetUpgradeCost();
			if (Cost < BestCost)
			{
				BestCost = Cost;
				Cheapest = *It;
			}
		}

		if (Cheapest == nullptr || !Cheapest->Upgrade())
		{
			break;
		}

		OutSpent += BestCost;
		++Bought;
	}

	return Bought;
}

void UBDSimSubsystem::HandleWaveStarted(const int32 Wave)
{
	// Every fifth wave is a boss wave, which is where the money and the pressure both
	// move; wave 1 and the cap are taken as well so the ends of the curve are in.
	if (Wave == LastSampledWave)
	{
		return;
	}

	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
	const int32 Interval = FMath::Max(1, Balance.CandidateInterval);
	if (Wave != 1 && Wave != TargetWaves && Wave % Interval != 0)
	{
		return;
	}

	LastSampledWave = Wave;
	LogSample(TEXT("SAMPLE"), Wave);
}

void UBDSimSubsystem::GetDefense(int32& OutCount, float& OutAverageLevel) const
{
	OutCount = 0;
	OutAverageLevel = 0.0f;

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	int32 LevelTotal = 0;
	for (TActorIterator<ABDTowerBase> It(World); It; ++It)
	{
		++OutCount;
		LevelTotal += It->GetTowerLevel();
	}
	OutAverageLevel = OutCount > 0 ? static_cast<float>(LevelTotal) / OutCount : 0.0f;
}

void UBDSimSubsystem::LogSample(const TCHAR* Tag, const int32 Wave) const
{
	const ABDMatchManager* Match = GetMatch();
	const UBDCandidateSubsystem* Candidates = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UBDCandidateSubsystem>() : nullptr;
	if (Match == nullptr)
	{
		return;
	}

	int32 Defenders = 0;
	float AverageLevel = 0.0f;
	GetDefense(Defenders, AverageLevel);

	// One line, one wave, every number the report needs: the count, the two currencies,
	// the state of the defense and how the bosses have gone.
	UE_LOG(LogBDDebug, Log, TEXT("SIM %s seed=%d evolve=%d wave=%d blue=%d red=%d null=%d bribe=%d money=%d defenders=%d avglevel=%.2f maxed=%d bosses_sent=%d bosses_fallen=%d levels=%d spent=%d"),
		Tag, Seed, bEvolve ? 1 : 0, Wave,
		Match->GetVotesBlue(), Match->GetVotesRed(), Match->GetVotesNull(),
		Match->GetBribeHeld(), Match->GetPublicMoney(),
		Defenders, AverageLevel,
		FMath::IsNearlyEqual(AverageLevel, static_cast<float>(UBDTowerData::MaxLevels)) ? 1 : 0,
		Candidates != nullptr ? Candidates->GetCandidatesSent() : 0,
		Candidates != nullptr ? Candidates->GetFallenCount() : 0,
		LevelsBought, MoneySpent);
}

void UBDSimSubsystem::Report(const TCHAR* Outcome)
{
	const ABDMatchManager* Match = GetMatch();
	if (Match == nullptr)
	{
		return;
	}

	LogSample(TEXT("FINAL"), Match->GetCurrentWave());

	int32 Defenders = 0;
	float AverageLevel = 0.0f;
	GetDefense(Defenders, AverageLevel);

	UE_LOG(LogBDDebug, Log, TEXT("SIM RESULT seed=%d evolve=%d outcome=%s wave=%d blue=%d red=%d null=%d money=%d defenders=%d avglevel=%.2f firstevolution=%d levels=%d spent=%d realseconds=%.0f"),
		Seed, bEvolve ? 1 : 0, Outcome, Match->GetCurrentWave(),
		Match->GetVotesBlue(), Match->GetVotesRed(), Match->GetVotesNull(), Match->GetPublicMoney(),
		Defenders, AverageLevel, FirstEvolutionWave, LevelsBought, MoneySpent, RealSecondsElapsed);
}

//~ Console -------------------------------------------------------------------------

namespace BDSimDebug
{
	static void ExecAutoEvolve(const TArray<FString>& Args, UWorld* World)
	{
		UBDSimSubsystem* Sim = UBDSimSubsystem::Get(World);
		const ABDMatchManager* Match = ABDMatchManager::Get(World);
		if (Sim == nullptr || Match == nullptr)
		{
			UE_LOG(LogBDDebug, Error, TEXT("BD.Debug.AutoEvolve needs a running match."));
			return;
		}

		int32 Spent = 0;
		const int32 Bought = Sim->AutoEvolve(Spent);
		UE_LOG(LogBDDebug, Log, TEXT("BD.Debug.AutoEvolve: %d level(s) bought for %d public money, %d left."),
			Bought, Spent, Match->GetPublicMoney());
	}

	static void ExecSimRun(const TArray<FString>& Args, UWorld* World)
	{
		UBDSimSubsystem* Sim = UBDSimSubsystem::Get(World);
		if (Sim == nullptr)
		{
			UE_LOG(LogBDDebug, Error, TEXT("BD.Sim.Run needs a game world."));
			return;
		}

		const int32 SimSeed = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
		const int32 Waves = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 100;
		const bool bEvolve = Args.Num() > 2 ? FCString::Atoi(*Args[2]) != 0 : true;
		const float Speed = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 4.0f;
		const bool bQuit = Args.Num() > 4 ? FCString::Atoi(*Args[4]) != 0 : true;
		Sim->Start(SimSeed, Waves, bEvolve, Speed, bQuit);
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdAutoEvolve(
		TEXT("BD.Debug.AutoEvolve"),
		TEXT("BD.Debug.AutoEvolve: spends the public money on evolution, cheapest level first, until nothing else is affordable."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecAutoEvolve));

	static FAutoConsoleCommandWithWorldAndArgs CmdSimRun(
		TEXT("BD.Sim.Run"),
		TEXT("BD.Sim.Run <seed> [waves] [evolve 0|1] [speed] [quit 0|1]: plays a whole match by itself and reports it on SIM lines."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSimRun));
}

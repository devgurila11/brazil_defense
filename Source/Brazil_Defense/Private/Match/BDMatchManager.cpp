// Brazil Defense. The clock and the ledger of a match.

#include "Match/BDMatchManager.h"

#include "BDLog.h"
#include "Components/SceneComponent.h"
#include "Day/BDDayCycleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Match/BDDifficultyData.h"
#include "Match/BDGameBalanceSettings.h"
#include "Obstacle/BDObstacleGenerator.h"

namespace BDMatchDebug
{
	/** Debug: holds the building countdown still, so placement can be tested at leisure. */
	static int32 GFreezeTimer = 0;

	static FAutoConsoleVariableRef CVarFreezeTimer(
		TEXT("BD.Match.FreezeTimer"),
		GFreezeTimer,
		TEXT("1 freezes the building phase countdown so the next wave never goes out on its own. 0 to resume."),
		ECVF_Cheat);
}

ABDMatchManager::ABDMatchManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	DayCycle = CreateDefaultSubobject<UBDDayCycleComponent>(TEXT("DayCycle"));

	SetHidden(true);
	SetCanBeDamaged(false);

#if WITH_EDITOR
	// A match manager that streams out is a match that forgets itself.
	bIsSpatiallyLoaded = false;
#endif
}

ABDMatchManager* ABDMatchManager::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<ABDMatchManager> It(World); It; ++It)
	{
		return *It;
	}

	return nullptr;
}

UBDGridSubsystem* ABDMatchManager::GetGrid() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
}

void ABDMatchManager::BeginPlay()
{
	Super::BeginPlay();

	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();

	DifficultyData = Balance.FindDifficultyData(Difficulty);
	if (DifficultyData == nullptr)
	{
		// The asset is an override, not a requirement: a project with no difficulty assets
		// yet still has to be playable, so the class defaults stand in.
		DifficultyData = GetDefault<UBDDifficultyData>();
		UE_LOG(LogBDMatch, Warning,
			TEXT("No difficulty asset configured for %s, using the built in defaults."),
			*StaticEnum<EBDDifficulty>()->GetNameStringByValue(static_cast<int64>(Difficulty)));
	}

	DividersRemaining = DifficultyData->DividerBudget;
	PlatformsRemaining = DifficultyData->PlatformBudget;
	TowersRemaining = DifficultyData->StartingTowers;

	SetupBoard();

	// Applied even though it is already 1, so a manager restarted mid session cannot
	// inherit the dilation of the last one.
	SetGameSpeed(GameSpeed);

	StartBuildingPhase();
}

void ABDMatchManager::SetupBoard()
{
	SetPhase(EBDMatchPhase::Setup);

	if (bRandomizeSeed)
	{
		ObstacleSeed = FMath::Rand();
	}

	UBDGridSubsystem* Grid = GetGrid();
	const UWorld* World = GetWorld();
	UBDObstacleGenerator* Generator = World != nullptr ? World->GetSubsystem<UBDObstacleGenerator>() : nullptr;

	if (Grid == nullptr || Generator == nullptr)
	{
		UE_LOG(LogBDMatch, Error, TEXT("Match setup found no grid or no obstacle generator."));
		return;
	}

	const int32 ObstacleCount = DifficultyData != nullptr ? DifficultyData->ObstacleCount : -1;
	Generator->GenerateObstacles(Grid, ObstacleSeed, ObstacleCount);

	UE_LOG(LogBDMatch, Log, TEXT("Match board built from seed %d."), ObstacleSeed);
}

void ABDMatchManager::SetPhase(const EBDMatchPhase NewPhase)
{
	if (Phase == NewPhase)
	{
		return;
	}

	Phase = NewPhase;
	OnPhaseChanged.Broadcast(NewPhase);
}

void ABDMatchManager::StartBuildingPhase()
{
	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();

	TimeUntilNextWave = CurrentWave == 0 && DifficultyData != nullptr
		? DifficultyData->FirstWaveDelay
		: Balance.GetWaveDelay(CurrentWave);

	SetPhase(EBDMatchPhase::Building);

	UE_LOG(LogBDMatch, Log, TEXT("Building phase open, %.1fs until wave %d."),
		TimeUntilNextWave, CurrentWave + 1);
}

void ABDMatchManager::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Phase != EBDMatchPhase::Building)
	{
		return;
	}

	if (BDMatchDebug::GFreezeTimer != 0)
	{
		return;
	}

	// DeltaSeconds already carries the global time dilation, which is the whole point of
	// running the speed control through it: this countdown needs no idea it exists.
	TimeUntilNextWave -= DeltaSeconds;
	if (TimeUntilNextWave <= 0.0f)
	{
		StartWave();
	}
}

void ABDMatchManager::StartWave()
{
	++CurrentWave;
	TimeUntilNextWave = 0.0f;

	SetPhase(EBDMatchPhase::WaveActive);

	if (DayCycle != nullptr)
	{
		DayCycle->SetWave(CurrentWave);
	}

	OnWaveStarted.Broadcast(CurrentWave);

	UE_LOG(LogBDMatch, Log, TEXT("Wave %d is out. Dividers are%s still removable."),
		CurrentWave,
		CanRemove(EBDPieceKind::Divider) ? TEXT("") : TEXT(" no longer"));
}

void ABDMatchManager::CallWaveEarly()
{
	if (Phase != EBDMatchPhase::Building)
	{
		UE_LOG(LogBDMatch, Warning, TEXT("CallWaveEarly outside the building phase does nothing."));
		return;
	}

	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
	const float TimeRemaining = FMath::Max(0.0f, TimeUntilNextWave);
	const int32 Bonus = FMath::RoundToInt(TimeRemaining * Balance.EarlyCallBonusPerSecond);

	EarlyCallBonus += Bonus;

	UE_LOG(LogBDMatch, Log, TEXT("Wave called %.1fs early, bonus %d (banked total %d)."),
		TimeRemaining, Bonus, EarlyCallBonus);

	StartWave();
}

void ABDMatchManager::DebugForcePhase(const EBDMatchPhase NewPhase)
{
	switch (NewPhase)
	{
	case EBDMatchPhase::Building:
		// Back to the start of the match, not just to the phase: a locked build would make
		// the building phase useless for what this is for.
		CurrentWave = 0;
		StartBuildingPhase();
		if (DayCycle != nullptr)
		{
			DayCycle->SetWave(CurrentWave);
		}
		UE_LOG(LogBDMatch, Warning, TEXT("Phase forced to Building; wave count rewound to 0."));
		break;

	case EBDMatchPhase::WaveActive:
		StartWave();
		UE_LOG(LogBDMatch, Warning, TEXT("Phase forced to WaveActive."));
		break;

	default:
		SetPhase(NewPhase);
		UE_LOG(LogBDMatch, Warning, TEXT("Phase forced to %s."),
			*StaticEnum<EBDMatchPhase>()->GetNameStringByValue(static_cast<int64>(NewPhase)));
		break;
	}
}

void ABDMatchManager::OnWaveCleared()
{
	if (Phase != EBDMatchPhase::WaveActive)
	{
		UE_LOG(LogBDMatch, Warning, TEXT("OnWaveCleared with no wave out does nothing."));
		return;
	}

	UE_LOG(LogBDMatch, Log, TEXT("Wave %d cleared."), CurrentWave);
	StartBuildingPhase();
}

bool ABDMatchManager::SetGameSpeed(const float Speed)
{
	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
	if (!Balance.IsGameSpeedAllowed(Speed))
	{
		UE_LOG(LogBDMatch, Error, TEXT("Game speed %.2f is not one of the allowed speeds."), Speed);
		return false;
	}

	GameSpeed = Speed;
	UGameplayStatics::SetGlobalTimeDilation(this, Speed);

	UE_LOG(LogBDMatch, Log, TEXT("Game speed %.0fx."), Speed);
	return true;
}

int32* ABDMatchManager::FindBudget(const EBDPieceKind Kind)
{
	switch (Kind)
	{
	case EBDPieceKind::Divider:
		return &DividersRemaining;

	case EBDPieceKind::Platform:
		return &PlatformsRemaining;

	case EBDPieceKind::Tower:
		return &TowersRemaining;

	default:
		return nullptr;
	}
}

const int32* ABDMatchManager::FindBudget(const EBDPieceKind Kind) const
{
	return const_cast<ABDMatchManager*>(this)->FindBudget(Kind);
}

bool ABDMatchManager::CanPlace(const EBDPieceKind Kind) const
{
	const int32* Budget = FindBudget(Kind);
	if (Budget == nullptr || *Budget <= 0)
	{
		return false;
	}

	if (Kind == EBDPieceKind::Tower)
	{
		// Towers are the one thing that stays placeable once the maze is locked in.
		return Phase == EBDMatchPhase::Building || Phase == EBDMatchPhase::WaveActive;
	}

	return Phase == EBDMatchPhase::Building && !IsBuildLocked();
}

bool ABDMatchManager::ConsumeBudget(const EBDPieceKind Kind)
{
	if (!CanPlace(Kind))
	{
		return false;
	}

	int32* Budget = FindBudget(Kind);
	--(*Budget);

	UE_LOG(LogBDMatch, Verbose, TEXT("Budget for %s is now %d."),
		*StaticEnum<EBDPieceKind>()->GetNameStringByValue(static_cast<int64>(Kind)), *Budget);

	return true;
}

bool ABDMatchManager::CanRemove(const EBDPieceKind Kind) const
{
	if (FindBudget(Kind) == nullptr)
	{
		return false;
	}

	if (Kind == EBDPieceKind::Tower)
	{
		return true;
	}

	if (!IsBuildLocked())
	{
		return true;
	}

	// Past the first wave the maze is set. Dividers keep a grace window, because the early
	// waves are the only read the player has on their own maze and a match lost to a choice
	// made blind is not difficulty. Platforms get no such window.
	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
	return Kind == EBDPieceKind::Divider && CurrentWave <= Balance.DividerRemovalGraceWave;
}

void ABDMatchManager::RefundRemoval(const EBDPieceKind Kind)
{
	int32* Budget = FindBudget(Kind);
	if (Budget == nullptr)
	{
		return;
	}

	if (Kind == EBDPieceKind::Tower)
	{
		// A tower taken back is a tower held again, whenever it happens.
		++(*Budget);
		return;
	}

	if (IsBuildLocked())
	{
		// Inside the grace window the piece comes off the board but nothing comes back:
		// a correction, not a strategy.
		return;
	}

	// Budgets are counts today, so the ratio can only round to a whole piece. It is a
	// fraction because the economy that will replace these counters spends currency.
	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
	*Budget += FMath::RoundToInt(Balance.BuildingPhaseRefundRatio);
}

namespace BDMatchCommands
{
	static constexpr int32 ArgCountSpeed = 1;

	static ABDMatchManager* FindMatch(UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogBDMatch, Error, TEXT("This command needs a world."));
			return nullptr;
		}

		ABDMatchManager* Match = ABDMatchManager::Get(World);
		if (Match == nullptr)
		{
			UE_LOG(LogBDMatch, Error, TEXT("No match manager in this world. Is the game running?"));
		}

		return Match;
	}

	static void ExecSpeed(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != ArgCountSpeed)
		{
			UE_LOG(LogBDMatch, Error, TEXT("Usage: BD.Match.Speed <1|2|4>"));
			return;
		}

		if (ABDMatchManager* Match = FindMatch(World))
		{
			Match->SetGameSpeed(FCString::Atof(*Args[0]));
		}
	}

	static void ExecStatus(const TArray<FString>& Args, UWorld* World)
	{
		const ABDMatchManager* Match = FindMatch(World);
		if (Match == nullptr)
		{
			return;
		}

		UE_LOG(LogBDMatch, Log,
			TEXT("Phase %s | wave %d | %.1fs to next | dividers %d | platforms %d | towers %d | speed %.0fx | bonus %d"),
			*StaticEnum<EBDMatchPhase>()->GetNameStringByValue(static_cast<int64>(Match->GetPhase())),
			Match->GetCurrentWave(), Match->GetTimeUntilNextWave(),
			Match->GetDividersRemaining(), Match->GetPlatformsRemaining(), Match->GetTowersRemaining(),
			Match->GetGameSpeed(), Match->GetEarlyCallBonus());
	}

	static void ExecCallWave(const TArray<FString>& Args, UWorld* World)
	{
		if (ABDMatchManager* Match = FindMatch(World))
		{
			Match->CallWaveEarly();
		}
	}

	static void ExecClearWave(const TArray<FString>& Args, UWorld* World)
	{
		if (ABDMatchManager* Match = FindMatch(World))
		{
			Match->OnWaveCleared();
		}
	}

	static constexpr int32 ArgCountSetPhase = 1;

	static void ExecSetPhase(const TArray<FString>& Args, UWorld* World)
	{
		const UEnum* PhaseEnum = StaticEnum<EBDMatchPhase>();
		const int64 PhaseValue = Args.Num() == ArgCountSetPhase ? PhaseEnum->GetValueByNameString(Args[0]) : INDEX_NONE;
		if (PhaseValue == INDEX_NONE)
		{
			UE_LOG(LogBDMatch, Error, TEXT("Usage: BD.Match.SetPhase <Building|WaveActive|Setup|Defeat|Victory>"));
			return;
		}

		if (ABDMatchManager* Match = FindMatch(World))
		{
			Match->DebugForcePhase(static_cast<EBDMatchPhase>(PhaseValue));
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSetPhase(
		TEXT("BD.Match.SetPhase"),
		TEXT("BD.Match.SetPhase <Building|WaveActive>: forces the phase. Building also rewinds the wave count to 0."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSetPhase));

	static FAutoConsoleCommandWithWorldAndArgs CmdSpeed(
		TEXT("BD.Match.Speed"),
		TEXT("BD.Match.Speed <1|2|4>: sets how fast the match runs, through global time dilation."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSpeed));

	static FAutoConsoleCommandWithWorldAndArgs CmdStatus(
		TEXT("BD.Match.Status"),
		TEXT("BD.Match.Status: logs the phase, wave, countdown and remaining budgets."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));

	static FAutoConsoleCommandWithWorldAndArgs CmdCallWave(
		TEXT("BD.Match.CallWave"),
		TEXT("BD.Match.CallWave: skips the countdown and sends the next wave out."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecCallWave));

	static FAutoConsoleCommandWithWorldAndArgs CmdClearWave(
		TEXT("BD.Match.ClearWave"),
		TEXT("BD.Match.ClearWave: stands in for the spawner reporting the board empty."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecClearWave));
}

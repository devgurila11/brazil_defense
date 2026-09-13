// Brazil Defense. The clock and the ledger of a match.

#include "Match/BDMatchManager.h"

#include "BDLog.h"
#include "Candidate/BDCandidateSubsystem.h"
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
#include "Tower/BDTowerBase.h"

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
	TowersRemaining = DifficultyData->TowerBudget;
	CharactersRemaining = DifficultyData->CharacterBudget;
	ObjectivesRemaining = 1;

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

	// A match that is over freezes the board where it stands: the clock is the one
	// place every system reads, so it is the one place to stop them all. Rewinding to
	// Building from the console brings the chosen speed back.
	if (IsMatchOver())
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 0.0f);
	}
	else if (!FMath::IsNearlyEqual(UGameplayStatics::GetGlobalTimeDilation(this), GameSpeed))
	{
		UGameplayStatics::SetGlobalTimeDilation(this, GameSpeed);
	}

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

	// The pause a candidate kill buys holds the next wave back, countdown included.
	const UBDCandidateSubsystem* Candidates = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UBDCandidateSubsystem>() : nullptr;
	if (Candidates != nullptr && Candidates->IsCountFrozen())
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

	UE_LOG(LogBDMatch, Log, TEXT("Wave %d is out. Selling pays %.0f%% (dividers %.0f%%), moving costs %.0f%%."),
		CurrentWave,
		GetSellRefundRatio(EBDPieceKind::Platform) * 100.0f,
		GetSellRefundRatio(EBDPieceKind::Divider) * 100.0f,
		GetMoveTaxRate() * 100.0f);
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

float ABDMatchManager::GetHealthScale() const
{
	return UBDGameBalanceSettings::Get().GetHealthScale(CurrentWave);
}

void ABDMatchManager::DebugRegenerateObstacles(const int32 Seed)
{
	UBDGridSubsystem* Grid = GetGrid();
	const UWorld* World = GetWorld();
	UBDObstacleGenerator* Generator = World != nullptr ? World->GetSubsystem<UBDObstacleGenerator>() : nullptr;
	if (Grid == nullptr || Generator == nullptr)
	{
		return;
	}

	ObstacleSeed = Seed;
	Generator->GenerateObstacles(Grid, ObstacleSeed, DifficultyData != nullptr ? DifficultyData->ObstacleCount : -1);
	UE_LOG(LogBDMatch, Log, TEXT("Match board rebuilt from seed %d."), ObstacleSeed);
}

void ABDMatchManager::DebugResetBudgets()
{
	if (DifficultyData == nullptr)
	{
		return;
	}

	DividersRemaining = DifficultyData->DividerBudget;
	PlatformsRemaining = DifficultyData->PlatformBudget;
	TowersRemaining = DifficultyData->TowerBudget;
	CharactersRemaining = DifficultyData->CharacterBudget;
	ObjectivesRemaining = 1;

	UE_LOG(LogBDMatch, Warning, TEXT("Budgets reset: %d dividers, %d platforms, %d towers, %d characters, 1 objective."),
		DividersRemaining, PlatformsRemaining, TowersRemaining, CharactersRemaining);
}

void ABDMatchManager::DebugSetWave(const int32 Wave)
{
	CurrentWave = FMath::Max(0, Wave);
	if (DayCycle != nullptr)
	{
		DayCycle->SetWave(CurrentWave);
	}

	UE_LOG(LogBDMatch, Warning, TEXT("Wave counter set to %d: creeps spawn at x%.2f health, %d per spawn point, move tax %.0f%%."),
		CurrentWave, GetHealthScale(), UBDGameBalanceSettings::Get().GetCreepsPerSpawnPoint(CurrentWave), GetMoveTaxRate() * 100.0f);
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

	// The one line a match is audited by afterwards: the scoreboard at the end of every wave.
	UE_LOG(LogBDMatch, Log, TEXT("Wave %d cleared. Votes: blue %d, red %d."), CurrentWave, VotesBlue, VotesRed);
	StartBuildingPhase();
}

void ABDMatchManager::DeclareDefeat(const FString& Reason)
{
	if (IsMatchOver())
	{
		UE_LOG(LogBDMatch, Warning, TEXT("Defeat declared (%s) but the match is already over."), *Reason);
		return;
	}

	UE_LOG(LogBDMatch, Log, TEXT("DEFEAT on wave %d: %s. Board frozen. Votes: blue %d, red %d."),
		CurrentWave, *Reason, VotesBlue, VotesRed);
	SetPhase(EBDMatchPhase::Defeat);
}

void ABDMatchManager::AddVotesBlue(const int32 Votes)
{
	if (Votes <= 0)
	{
		return;
	}

	VotesBlue += Votes;
	OnVotesChanged.Broadcast(VotesBlue, VotesRed);

	UE_LOG(LogBDMatch, Verbose, TEXT("Blue +%d votes, now %d blue / %d red."), Votes, VotesBlue, VotesRed);
}

void ABDMatchManager::AddVotesRed(const int32 Votes)
{
	if (Votes <= 0)
	{
		return;
	}

	// While the count is frozen after a candidate kill, an arrival scores nothing: that
	// is what the kill bought.
	const UBDCandidateSubsystem* Candidates = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UBDCandidateSubsystem>() : nullptr;
	if (Candidates != nullptr && Candidates->IsCountFrozen())
	{
		UE_LOG(LogBDMatch, Log, TEXT("Red +%d not counted: the count is frozen for %.0fs more. Still %d blue / %d red."),
			Votes, Candidates->GetPauseRemaining(), VotesBlue, VotesRed);
		return;
	}

	VotesRed += Votes;
	OnVotesChanged.Broadcast(VotesBlue, VotesRed);

	UE_LOG(LogBDMatch, Verbose, TEXT("Red +%d votes, now %d blue / %d red."), Votes, VotesBlue, VotesRed);
}

bool ABDMatchManager::SpendVotesBlue(const int32 Votes)
{
	if (Votes < 0 || !CanAffordVotesBlue(Votes))
	{
		return false;
	}

	if (Votes == 0)
	{
		return true;
	}

	VotesBlue -= Votes;
	OnVotesChanged.Broadcast(VotesBlue, VotesRed);

	UE_LOG(LogBDMatch, Verbose, TEXT("Blue -%d votes, now %d blue / %d red."), Votes, VotesBlue, VotesRed);
	return true;
}

int32 ABDMatchManager::GetUpgradeCost(const ABDTowerBase* Tower) const
{
	return Tower != nullptr ? Tower->GetUpgradeCost() : 0;
}

bool ABDMatchManager::WouldInvertScoreboard(const int32 Cost) const
{
	// Only a lead that is lost counts as an inversion; a player already behind is told nothing new.
	return VotesBlue >= VotesRed && VotesBlue - Cost < VotesRed;
}

float ABDMatchManager::GetSellRefundRatio(const EBDPieceKind Kind) const
{
	return UBDGameBalanceSettings::Get().GetSellRefundRatio(Kind, CurrentWave);
}

int32 ABDMatchManager::GetSellRefund(const EBDPieceKind Kind, const int32 BuildCost) const
{
	return FMath::RoundToInt(FMath::Max(0, BuildCost) * GetSellRefundRatio(Kind));
}

int32 ABDMatchManager::RefundSale(const EBDPieceKind Kind, const int32 BuildCost)
{
	const int32 Refund = GetSellRefund(Kind, BuildCost);
	AddVotesBlue(Refund);
	return Refund;
}

float ABDMatchManager::GetMoveTaxRate() const
{
	return UBDGameBalanceSettings::Get().GetMoveTaxRate(CurrentWave);
}

int32 ABDMatchManager::GetMoveCost(const int32 BuildCost) const
{
	return FMath::RoundToInt(FMath::Max(0, BuildCost) * GetMoveTaxRate());
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
	if (IsMatchOver())
	{
		// Kept for when the match is rewound; a frozen board stays frozen.
		UE_LOG(LogBDMatch, Log, TEXT("Game speed %.0fx noted; the board stays frozen until the match is rewound."), Speed);
		return true;
	}

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

	case EBDPieceKind::Character:
		return &CharactersRemaining;

	case EBDPieceKind::Objective:
		return &ObjectivesRemaining;

	default:
		return nullptr;
	}
}

const int32* ABDMatchManager::FindBudget(const EBDPieceKind Kind) const
{
	return const_cast<ABDMatchManager*>(this)->FindBudget(Kind);
}

int32 ABDMatchManager::GetBudgetRemaining(const EBDPieceKind Kind) const
{
	const int32* Budget = FindBudget(Kind);
	return Budget != nullptr ? *Budget : 0;
}

bool ABDMatchManager::CanPlace(const EBDPieceKind Kind) const
{
	const int32* Budget = FindBudget(Kind);
	if (Budget == nullptr || *Budget <= 0)
	{
		return false;
	}

	if (Kind == EBDPieceKind::Tower || Kind == EBDPieceKind::Character)
	{
		// Defenders are the one thing that stays placeable once the maze is locked in.
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

	// The urn is put down once. Everything the player builds afterwards is built around
	// it, so taking it back would invalidate the whole maze. Everything else can be sold
	// at any point of the match: a player who saw the truck is in the wrong place has to
	// be able to fix it, paying for it. Nothing on the board is permanent.
	return Kind != EBDPieceKind::Objective;
}

void ABDMatchManager::RefundRemoval(const EBDPieceKind Kind)
{
	// A piece taken back is a piece held again, whenever it happens. What the sale of it
	// was worth is a separate matter, settled in votes by RefundSale.
	if (int32* Budget = FindBudget(Kind))
	{
		++(*Budget);
	}
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
			TEXT("Phase %s | wave %d (health x%.2f) | %.1fs to next | dividers %d | platforms %d | towers %d | characters %d | objectives %d | speed %.0fx | bonus %d | votes %d blue / %d red"),
			*StaticEnum<EBDMatchPhase>()->GetNameStringByValue(static_cast<int64>(Match->GetPhase())),
			Match->GetCurrentWave(), Match->GetHealthScale(), Match->GetTimeUntilNextWave(),
			Match->GetDividersRemaining(), Match->GetPlatformsRemaining(), Match->GetTowersRemaining(),
			Match->GetCharactersRemaining(), Match->GetObjectivesRemaining(),
			Match->GetGameSpeed(), Match->GetEarlyCallBonus(), Match->GetVotesBlue(), Match->GetVotesRed());
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

	static void ExecSetWave(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != 1)
		{
			UE_LOG(LogBDMatch, Error, TEXT("Usage: BD.Match.SetWave <wave>"));
			return;
		}

		if (ABDMatchManager* Match = FindMatch(World))
		{
			Match->DebugSetWave(FCString::Atoi(*Args[0]));
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSetWave(
		TEXT("BD.Match.SetWave"),
		TEXT("BD.Match.SetWave <wave>: jumps the wave counter, for testing the scaling of a given wave."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSetWave));

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
		TEXT("BD.Match.ClearWave: reports the board empty without waiting for the creeps to leave it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecClearWave));
}

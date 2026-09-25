// Brazil Defense. The clock and the ledger of a match.

#include "Match/BDMatchManager.h"

#include "BDBuildInfo.h"
#include "BDLog.h"
#include "Candidate/BDCandidateSubsystem.h"
#include "Components/SceneComponent.h"
#include "Day/BDDayCycleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Match/BDDifficultyData.h"
#include "Objective/BDObjectiveSubsystem.h"
#include "Placement/BDPlacementComponent.h"
#include "Save/BDMatchSave.h"
#include "Save/BDProgressSave.h"
#include "Report/BDPostMatch.h"
#include "Wave/BDWaveSubsystem.h"
#include "Wave/BDWaveSettings.h"
#include "Enemy/BDEnemyData.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementSettings.h"
#include "Tower/BDTowerData.h"
#include "Match/BDGameBalanceSettings.h"
#include "Obstacle/BDObstacleGenerator.h"
#include "Tower/BDTowerBase.h"
#include "UI/BDUISubsystem.h"
#include "Engine/GameInstance.h"

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

	// The menu's choice wins over whatever the actor or the settings say; a level that
	// came up on its own (Play in Editor) has no choice to take and keeps its own.
	UBDUISubsystem* UI = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UBDUISubsystem>() : nullptr;
	const TOptional<EBDDifficulty> Chosen = UI != nullptr ? UI->TakeChosenDifficulty() : TOptional<EBDDifficulty>();
	if (Chosen.IsSet())
	{
		Difficulty = Chosen.GetValue();
	}

	ResolveDifficulty();
	ApplyStartingBudgets();

	UE_LOG(LogBDMatch, Log, TEXT("Match on %s%s (%s): %d waves to win, %d dividers, %d platforms, %d saves. Defenders are limited by public money alone."),
		*StaticEnum<EBDDifficulty>()->GetNameStringByValue(static_cast<int64>(Difficulty)),
		Chosen.IsSet() ? TEXT(" (chosen in the menu)") : TEXT(""), *BDBuildInfo::GetLabel(),
		GetWavesToWin(), DividersRemaining, PlatformsRemaining, SavesRemaining);

	// The vote part of the bonus is the one thing a budget reset must not hand out
	// again, so it stays here, at the one start a match has: a head start on the count.
	if (bChainBonusApplied && DifficultyData->ChainBonus.Votes > 0)
	{
		VotesBlue += DifficultyData->ChainBonus.Votes;
	}
	OnVotesChanged.Broadcast(VotesBlue, VotesRed);
	OnMoneyChanged.Broadcast(BribeHeld, PublicMoney);
	UE_LOG(LogBDMatch, Log, TEXT("Opening capital: %d public money to build with; %d blue vote(s) on the count, which are never spent."),
		PublicMoney, VotesBlue);
	OpenLedger(-1);

	// Leaving a match half played is an end too, and the report wants it: the world
	// tearing down is told before the actors lose their components, so the board can
	// still be counted then. EndPlay stays as the fallback.
	TearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &ABDMatchManager::HandleWorldBeginTearDown);

	SetupBoard();

	// Applied even though it is already 1, so a manager restarted mid session cannot
	// inherit the dilation of the last one.
	SetGameSpeed(GameSpeed);

	StartBuildingPhase();
}

void ABDMatchManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FWorldDelegates::OnWorldBeginTearDown.Remove(TearDownHandle);
	ReportAbandoned(TEXT("the match was left"));
	Super::EndPlay(EndPlayReason);
}

void ABDMatchManager::HandleWorldBeginTearDown(UWorld* World)
{
	if (World == GetWorld())
	{
		ReportAbandoned(TEXT("the match was left"));
	}
}

void ABDMatchManager::OpenLedger(const int32 LoadedAtWave)
{
	Ledger = FBDMatchLedger();
	Ledger.StartingFunds = PublicMoney;
	Ledger.LoadedAtWave = LoadedAtWave;
	Ledger.LastGrowthWave = CurrentWave;
	Ledger.RealStartSeconds = FPlatformTime::Seconds();
	Ledger.GameStartSeconds = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0;
	Ledger.StartedAt = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
	Ledger.WaveMark = BDWaveLog::Mark(*this);
}

void ABDMatchManager::ReportAbandoned(const FString& Why)
{
	// A match that never sent a wave out was not played; one already settled has its row.
	if (Ledger.bReportWritten || IsMatchOver() || CurrentWave < 1)
	{
		return;
	}

	Ledger.EndReason = Why;
	Ledger.PublicMoneyAtEnd = PublicMoney;
	Ledger.BribeHeldAtEnd = BribeHeld;
	BDPostMatch::Write(*this, TEXT("Abandoned"));
}

void ABDMatchManager::ResolveDifficulty()
{
	DifficultyData = UBDGameBalanceSettings::Get().FindDifficultyData(Difficulty);
	if (DifficultyData == nullptr)
	{
		// The asset is an override, not a requirement: a project with no difficulty assets
		// yet still has to be playable, so the class defaults stand in.
		DifficultyData = GetDefault<UBDDifficultyData>();
		UE_LOG(LogBDMatch, Warning,
			TEXT("No difficulty asset configured for %s, using the built in defaults."),
			*StaticEnum<EBDDifficulty>()->GetNameStringByValue(static_cast<int64>(Difficulty)));
	}
}

EBDDifficulty ABDMatchManager::GetDifficultyBelow(const EBDDifficulty Difficulty)
{
	return Difficulty == EBDDifficulty::Easy || Difficulty >= EBDDifficulty::Count
		? EBDDifficulty::Count
		: static_cast<EBDDifficulty>(static_cast<uint8>(Difficulty) - 1);
}

void ABDMatchManager::ApplyStartingBudgets()
{
	DividersRemaining = DifficultyData->DividerBudget;
	PlatformsRemaining = DifficultyData->PlatformBudget;
	ObjectivesRemaining = 1;
	SavesRemaining = DifficultyData->SaveBudget;

	// The opening capital, in public money, and the head start on the count. Set rather
	// than added: starting budgets are what a match opens with, and a rewind has to land
	// on the same numbers as a fresh start.
	PublicMoney = DifficultyData->StartingFunds;
	VotesBlue = DifficultyData->StartingVotes;

	const EBDDifficulty Below = GetDifficultyBelow(Difficulty);
	const FBDChainBonus& Bonus = DifficultyData->ChainBonus;
	bChainBonusApplied = Below != EBDDifficulty::Count && !Bonus.IsEmpty() && UBDProgressSave::HasWonDifficulty(Below);
	if (!bChainBonusApplied)
	{
		return;
	}

	DividersRemaining += Bonus.Dividers;
	PlatformsRemaining += Bonus.Platforms;
	SavesRemaining += Bonus.Saves;
	PublicMoney += Bonus.Funds;

	UE_LOG(LogBDMatch, Log, TEXT("Chain bonus for having won %s: +%d public money, +%d votes, +%d dividers, +%d platforms, +%d saves."),
		*StaticEnum<EBDDifficulty>()->GetNameStringByValue(static_cast<int64>(Below)),
		Bonus.Funds, Bonus.Votes, Bonus.Dividers, Bonus.Platforms, Bonus.Saves);
}

UBDPlacementComponent* ABDMatchManager::GetPlacement() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	return Controller != nullptr ? Controller->FindComponentByClass<UBDPlacementComponent>() : nullptr;
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

	// An end held back by a candidate is settled the moment he is gone between waves.
	if (IsEndDue())
	{
		ResolveEnd();
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

	ApplyStartingBudgets();

	OnMoneyChanged.Broadcast(BribeHeld, PublicMoney);
	UE_LOG(LogBDMatch, Warning, TEXT("Budgets reset: %d public money, %d dividers, %d platforms, 1 objective, %d saves%s. Defenders were never counted."),
		PublicMoney, DividersRemaining, PlatformsRemaining, SavesRemaining,
		bChainBonusApplied ? TEXT(" (chain bonus in)") : TEXT(""));
}

void ABDMatchManager::DebugSetWave(const int32 Wave)
{
	CurrentWave = FMath::Max(0, Wave);
	if (DayCycle != nullptr)
	{
		DayCycle->SetWave(CurrentWave);
	}
	// The wave log goes on from the wave set, not from the one it was at.
	Ledger.WaveMark = BDWaveLog::Mark(*this);

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
		bWon = false;
		bEndless = false;
		StartBuildingPhase();
		if (DayCycle != nullptr)
		{
			DayCycle->SetWave(CurrentWave);
		}
		// Taken after the phase change, which is what starts the combat and candidate totals over.
		Ledger.WaveMark = BDWaveLog::Mark(*this);
		UE_LOG(LogBDMatch, Warning, TEXT("Phase forced to Building; wave count rewound to 0."));
		break;

	case EBDMatchPhase::WaveActive:
		StartWave();
		UE_LOG(LogBDMatch, Warning, TEXT("Phase forced to WaveActive."));
		break;

	case EBDMatchPhase::Victory:
		DeclareVictory();
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

	// The maze grows with the match: every wave held is a few more dividers to draw with.
	if (DifficultyData != nullptr)
	{
		GrantDividers(DifficultyData->DividersPerWave, FString::Printf(TEXT("wave %d cleared"), CurrentWave));
	}

	// The one line a match is audited by afterwards, and its row in the wave log: written
	// before the end is resolved, so the last wave has its row before the money goes.
	BDWaveLog::Write(*this, TEXT("Cleared"));

	if (IsEndDue())
	{
		ResolveEnd();
		return;
	}

	if (!bWon && CurrentWave >= GetWavesToWin())
	{
		UE_LOG(LogBDMatch, Log, TEXT("Wave %d of %d cleared with a candidate on the board: the count waits on him."), CurrentWave, GetWavesToWin());
	}

	StartBuildingPhase();
}

bool ABDMatchManager::IsEndDue() const
{
	if (bWon || IsMatchOver() || CurrentWave < GetWavesToWin())
	{
		return false;
	}

	const UBDCandidateSubsystem* Candidates = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UBDCandidateSubsystem>() : nullptr;
	return Candidates == nullptr || (!Candidates->HasCandidateOnBoard() && !Candidates->IsReturnActive());
}

void ABDMatchManager::ResolveEnd()
{
	// The count is the verdict: blue ahead, or level, is the win; red ahead is the loss.
	if (VotesBlue >= VotesRed)
	{
		DeclareVictory();
	}
	else
	{
		DeclareDefeat(FString::Printf(TEXT("red was ahead at the end of wave %d, %d to %d"), CurrentWave, VotesRed, VotesBlue));
	}
}

void ABDMatchManager::EqualizeVotesDown(const FString& Why)
{
	const int32 Lower = FMath::Min(VotesBlue, VotesRed);
	if (VotesBlue == Lower && VotesRed == Lower)
	{
		return;
	}

	UE_LOG(LogBDMatch, Log, TEXT("Count levelled downwards (%s): %d blue / %d red -> %d / %d."), *Why, VotesBlue, VotesRed, Lower, Lower);
	VotesBlue = Lower;
	VotesRed = Lower;
	++LevelDownCount;
	OnVotesChanged.Broadcast(VotesBlue, VotesRed);
}

int32 ABDMatchManager::GetWavesToWin() const
{
	return FMath::Max(1, DifficultyData != nullptr ? DifficultyData->WavesToWin : GetDefault<UBDDifficultyData>()->WavesToWin);
}

int32 ABDMatchManager::GetPrisonersFreed() const
{
	return DifficultyData != nullptr ? DifficultyData->PrisonersFreed : GetDefault<UBDDifficultyData>()->PrisonersFreed;
}

void ABDMatchManager::DeclareVictory()
{
	if (IsMatchOver())
	{
		UE_LOG(LogBDMatch, Warning, TEXT("Victory declared but the match is already over."));
		return;
	}

	bWon = true;
	UE_LOG(LogBDMatch, Log, TEXT("VICTORY on wave %d: %d prisoner(s) freed. Board frozen. Votes: blue %d, red %d."),
		CurrentWave, GetPrisonersFreed(), VotesBlue, VotesRed);
	UBDProgressSave::RecordWin(Difficulty);

	// Read before the money goes: the report wants what was left unspent.
	Ledger.EndReason = FString::Printf(TEXT("the count after wave %d, %d blue to %d red"), CurrentWave, VotesBlue, VotesRed);
	Ledger.PublicMoneyAtEnd = PublicMoney;
	Ledger.BribeHeldAtEnd = BribeHeld;

	// The match is settled, so the money is settled with it: what was not spent on the
	// defense is not kept. Endless starts over on the bribes its own bosses drop.
	DropMoney(TEXT("the match was won"));
	SetPhase(EBDMatchPhase::Victory);
	BDPostMatch::Write(*this, TEXT("Victory"));
}

//~ Saving ------------------------------------------------------------------------

bool ABDMatchManager::CanSaveMatch() const
{
	return Phase == EBDMatchPhase::Building && SavesRemaining > 0;
}

bool ABDMatchManager::SaveMatch()
{
	if (!CanSaveMatch())
	{
		UE_LOG(LogBDMatch, Warning, TEXT("Save refused: %s."),
			Phase != EBDMatchPhase::Building ? TEXT("only between waves") : TEXT("no saves left"));
		return false;
	}

	UBDPlacementComponent* Placement = GetPlacement();
	if (Placement == nullptr)
	{
		UE_LOG(LogBDMatch, Error, TEXT("Save refused: no placement component to read the board from."));
		return false;
	}

	UBDMatchSave* Save = NewObject<UBDMatchSave>(GetTransientPackage());
	Save->Difficulty = Difficulty;
	Save->ObstacleSeed = ObstacleSeed;
	Save->Wave = CurrentWave;
	Save->VotesBlue = VotesBlue;
	Save->VotesRed = VotesRed;
	Save->VotesNull = VotesNull;
	Save->BribeHeld = BribeHeld;
	Save->PublicMoney = PublicMoney;
	Save->EarlyCallBonus = EarlyCallBonus;
	Save->GameSpeed = GameSpeed;
	// Spent before it is written: a load hands back the match, not the save.
	Save->SavesRemaining = SavesRemaining - 1;
	Save->bWon = bWon;
	Save->bEndless = bEndless;
	Save->DividersRemaining = DividersRemaining;
	Save->PlatformsRemaining = PlatformsRemaining;
	Save->SavedAt = FDateTime::Now();
	Placement->CaptureBoard(Save->Pieces);

	if (!Save->WriteToSlot())
	{
		UE_LOG(LogBDMatch, Error, TEXT("Save failed: the slot could not be written. Nothing spent."));
		return false;
	}

	SavesRemaining = Save->SavesRemaining;
	UE_LOG(LogBDMatch, Log, TEXT("Match saved after wave %d: %d piece(s), votes %d blue / %d red, %d public money, seed %d. %d save(s) left."),
		CurrentWave, Save->Pieces.Num(), VotesBlue, VotesRed, PublicMoney, ObstacleSeed, SavesRemaining);
	return true;
}

bool ABDMatchManager::HasSavedMatch()
{
	return UBDMatchSave::Exists();
}

bool ABDMatchManager::LoadMatch()
{
	const UBDMatchSave* Save = UBDMatchSave::LoadFromSlot();
	if (Save == nullptr)
	{
		UE_LOG(LogBDMatch, Warning, TEXT("Load: no saved match."));
		return false;
	}

	RestoreMatch(*Save);
	return true;
}

void ABDMatchManager::RestoreMatch(const UBDMatchSave& Save)
{
	UWorld* World = GetWorld();
	UBDWaveSubsystem* Waves = World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr;
	UBDPlacementComponent* Placement = GetPlacement();
	if (Waves == nullptr || Placement == nullptr)
	{
		UE_LOG(LogBDMatch, Error, TEXT("Load refused: the match needs the wave subsystem and a placement component."));
		return;
	}

	UE_LOG(LogBDMatch, Log, TEXT("Loading the match saved %s: wave %d, seed %d, %d piece(s)."),
		*Save.SavedAt.ToString(), Save.Wave, Save.ObstacleSeed, Save.Pieces.Num());

	// The match running now is thrown away for the saved one: it ends here, unfinished.
	ReportAbandoned(TEXT("a save was loaded over it"));

	if (Save.Difficulty != Difficulty)
	{
		Difficulty = Save.Difficulty;
		ResolveDifficulty();
	}

	// The board is emptied and the match rewound to its very start, which is the one
	// state that lets every kind of piece back on: the candidate's pause and arming go
	// with the rewind, the creeps and the candidate himself with the despawn.
	Waves->DespawnAll();
	Placement->DebugRemoveAll();
	// The urn is not one of the placement's pieces. It comes off too, and before the
	// obstacles: the generator lays a different board for a placed urn than for the
	// empty zone it saw when this match was first built.
	if (UBDObjectiveSubsystem* Objectives = World->GetSubsystem<UBDObjectiveSubsystem>())
	{
		Objectives->ClearObjective();
	}
	CurrentWave = 0;
	bWon = false;
	bEndless = false;
	VotesNull = 0;
	// Wiped before the rewind so nothing of the running match survives it; the saved
	// balances go back on further down, once the board is whole again.
	DropMoney(TEXT("the match is being rebuilt from a save"));
	SetPhase(EBDMatchPhase::Building);
	DebugResetBudgets();

	bRandomizeSeed = false;
	ObstacleSeed = Save.ObstacleSeed;
	DebugRegenerateObstacles(ObstacleSeed);

	// Restoring is not building, so it must not touch the money. The balance is read on
	// both sides of the restore rather than trusted: a board bought over ninety waves
	// costs many times the opening capital, so a restore that charged would quietly drop
	// whatever the capital did not cover, and the player would load a thinner board than
	// the one they saved.
	const int32 CapitalBeforeRestore = PublicMoney;
	Placement->RestoreBoard(Save.Pieces);
	UE_CLOG(PublicMoney != CapitalBeforeRestore, LogBDMatch, Error,
		TEXT("Restore charged the board: %d public money before, %d after. The saved pieces were paid for once already."),
		CapitalBeforeRestore, PublicMoney);

	// What the pieces consumed should be what the save says was left; the save wins,
	// and a mismatch is a piece that did not come back.
	if (DividersRemaining != Save.DividersRemaining || PlatformsRemaining != Save.PlatformsRemaining)
	{
		UE_LOG(LogBDMatch, Warning, TEXT("Budgets after restore (%d/%d) differ from the saved ones (%d/%d); the saved ones stand."),
			DividersRemaining, PlatformsRemaining, Save.DividersRemaining, Save.PlatformsRemaining);
	}
	DividersRemaining = Save.DividersRemaining;
	PlatformsRemaining = Save.PlatformsRemaining;

	CurrentWave = Save.Wave;
	EarlyCallBonus = Save.EarlyCallBonus;
	SavesRemaining = Save.SavesRemaining;
	bWon = Save.bWon;
	bEndless = Save.bEndless;
	VotesBlue = Save.VotesBlue;
	VotesRed = Save.VotesRed;
	VotesNull = Save.VotesNull;
	BribeHeld = Save.BribeHeld;
	PublicMoney = Save.PublicMoney;
	if (DayCycle != nullptr)
	{
		DayCycle->SetWave(CurrentWave);
	}
	SetGameSpeed(Save.GameSpeed);

	// The report counts from here: what happened before the save is in no board now.
	OpenLedger(Save.Wave);

	// Told last, with the board whole: an inverted score sends the candidate out again.
	OnVotesChanged.Broadcast(VotesBlue, VotesRed);
	OnMoneyChanged.Broadcast(BribeHeld, PublicMoney);
	StartBuildingPhase();

	UE_LOG(LogBDMatch, Log, TEXT("Match loaded: building for wave %d, votes %d blue / %d red, %d public money, %d save(s) left."),
		CurrentWave + 1, VotesBlue, VotesRed, PublicMoney, SavesRemaining);
}

bool ABDMatchManager::ContinueEndless()
{
	if (Phase != EBDMatchPhase::Victory)
	{
		UE_LOG(LogBDMatch, Warning, TEXT("ContinueEndless outside Victory does nothing."));
		return false;
	}

	bEndless = true;
	// The win has its row; the endless run that follows gets one of its own when it ends.
	Ledger.bReportWritten = false;
	UE_LOG(LogBDMatch, Log, TEXT("Endless from wave %d: the win stands, the waves go on."), CurrentWave + 1);
	StartBuildingPhase();
	return true;
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
	Ledger.EndReason = Reason;
	Ledger.PublicMoneyAtEnd = PublicMoney;
	Ledger.BribeHeldAtEnd = BribeHeld;
	// A wave lost while out still gets its row; one lost on the count after it cleared has it already.
	BDWaveLog::Write(*this, TEXT("Defeat"));
	DropMoney(TEXT("the match was lost"));
	SetPhase(EBDMatchPhase::Defeat);
	BDPostMatch::Write(*this, TEXT("Defeat"));
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

	// The urn registers the vote out loud.
	if (UBDObjectiveSubsystem* Objectives = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UBDObjectiveSubsystem>() : nullptr)
	{
		Objectives->PlayVoteSound(Votes);
	}

	UE_LOG(LogBDMatch, Verbose, TEXT("Red +%d votes, now %d blue / %d red."), Votes, VotesBlue, VotesRed);
}

void ABDMatchManager::AddVotesNull(const int32 Votes)
{
	if (Votes > 0)
	{
		VotesNull += Votes;
	}
}

//~ The bribe and the public money --------------------------------------------------

void ABDMatchManager::AddBribe(const int32 Amount, const FString& Why)
{
	if (Amount <= 0)
	{
		return;
	}

	BribeHeld += Amount;
	Ledger.BribeEarned += Amount;
	OnMoneyChanged.Broadcast(BribeHeld, PublicMoney);
	UE_LOG(LogBDBribe, Verbose, TEXT("Bribe +%d (%s): %d held, %d public."), Amount, *Why, BribeHeld, PublicMoney);
}

int32 ABDMatchManager::ConvertBribe(const int32 Amount)
{
	const int32 Moved = FMath::Clamp(Amount, 0, BribeHeld);
	if (Moved == 0)
	{
		return 0;
	}

	BribeHeld -= Moved;
	PublicMoney += Moved;
	OnMoneyChanged.Broadcast(BribeHeld, PublicMoney);
	UE_LOG(LogBDBribe, Verbose, TEXT("Minted %d: %d held, %d public."), Moved, BribeHeld, PublicMoney);
	return Moved;
}

void ABDMatchManager::AddPublicMoney(const int32 Amount, const FString& Why)
{
	if (Amount <= 0)
	{
		return;
	}

	PublicMoney += Amount;
	Ledger.Granted += Amount;
	OnMoneyChanged.Broadcast(BribeHeld, PublicMoney);
	UE_LOG(LogBDBribe, Log, TEXT("Public money +%d (%s): now %d."), Amount, *Why, PublicMoney);
}

void ABDMatchManager::PayRefund(const int32 Amount, const FString& Why)
{
	if (Amount <= 0)
	{
		return;
	}

	PublicMoney += Amount;
	Ledger.Refunded += Amount;
	OnMoneyChanged.Broadcast(BribeHeld, PublicMoney);
	UE_LOG(LogBDBribe, Log, TEXT("Public money +%d (%s): now %d."), Amount, *Why, PublicMoney);
}

bool ABDMatchManager::SpendPublicMoney(const int32 Amount, const EBDFundsUse Use)
{
	if (Amount < 0 || !CanAffordPublicMoney(Amount))
	{
		return false;
	}

	// Counted even when free: a divider comes out of its own hand, not the purse, and is
	// still a piece built.
	PublicMoney -= Amount;
	switch (Use)
	{
	case EBDFundsUse::Build:
		Ledger.SpentBuild += Amount;
		++Ledger.PiecesBuilt;
		Ledger.LastGrowthWave = CurrentWave;
		break;
	case EBDFundsUse::Evolve:
		Ledger.SpentEvolve += Amount;
		++Ledger.LevelsBought;
		Ledger.LastGrowthWave = CurrentWave;
		break;
	case EBDFundsUse::Move:
		Ledger.SpentMove += Amount;
		break;
	}
	OnMoneyChanged.Broadcast(BribeHeld, PublicMoney);
	UE_LOG(LogBDBribe, Verbose, TEXT("Public money -%d, now %d."), Amount, PublicMoney);
	return true;
}

void ABDMatchManager::ReturnPublicMoney(const int32 Amount, const EBDFundsUse Use, const FString& Why)
{
	if (Amount < 0)
	{
		return;
	}

	// The charge is undone rather than refunded: the ledger forgets it was ever made.
	PublicMoney += Amount;
	switch (Use)
	{
	case EBDFundsUse::Build:
		Ledger.SpentBuild -= Amount;
		--Ledger.PiecesBuilt;
		break;
	case EBDFundsUse::Evolve:
		Ledger.SpentEvolve -= Amount;
		--Ledger.LevelsBought;
		break;
	case EBDFundsUse::Move:
		Ledger.SpentMove -= Amount;
		break;
	}
	OnMoneyChanged.Broadcast(BribeHeld, PublicMoney);
	UE_LOG(LogBDBribe, Log, TEXT("Public money +%d back (%s): now %d."), Amount, *Why, PublicMoney);
}

void ABDMatchManager::DropMoney(const FString& Why)
{
	if (BribeHeld == 0 && PublicMoney == 0)
	{
		return;
	}

	UE_LOG(LogBDBribe, Log, TEXT("Money dropped (%s): %d bribe and %d public money gone. Nothing carries to the next match."),
		*Why, BribeHeld, PublicMoney);
	BribeHeld = 0;
	PublicMoney = 0;
	OnMoneyChanged.Broadcast(BribeHeld, PublicMoney);
}

int32 ABDMatchManager::GetFreeCharacterSlots() const
{
	const UBDPlacementComponent* Placement = GetPlacement();
	return Placement != nullptr ? Placement->CountFreeSlots() : 0;
}

void ABDMatchManager::GrantDividers(const int32 Count, const FString& Why)
{
	if (Count <= 0)
	{
		return;
	}

	DividersRemaining += Count;
	Ledger.DividersGranted += Count;
	UE_LOG(LogBDMatch, Log, TEXT("Dividers +%d (%s): %d in hand."), Count, *Why, DividersRemaining);
}

void ABDMatchManager::RewardCandidateKill(const int32 Ordinal)
{
	if (DifficultyData != nullptr)
	{
		GrantDividers(DifficultyData->GetDividersForCandidate(Ordinal), FString::Printf(TEXT("candidate %d killed"), Ordinal));
	}
}

void ABDMatchManager::AdjustDividerBudget(const int32 Delta, const FString& Why)
{
	if (Delta == 0)
	{
		return;
	}

	const int32 Before = DividersRemaining;
	DividersRemaining = FMath::Max(0, DividersRemaining + Delta);
	UE_LOG(LogBDMatch, Log, TEXT("Divider ceiling %s%d (%s): %d -> %d."),
		Delta > 0 ? TEXT("+") : TEXT(""), Delta, *Why, Before, DividersRemaining);
}

int32 ABDMatchManager::GetUpgradeCost(const ABDTowerBase* Tower) const
{
	return Tower != nullptr ? Tower->GetUpgradeCost() : 0;
}

int32 ABDMatchManager::GetCandidateFunds(const int32 Wave) const
{
	// The candidate's health is read off the wave creep, like the candidate subsystem
	// does when it sends one out: the price and the drop are the same number by design.
	const UBDEnemyData* Creep = UBDWaveSettings::Get().ResolveWaveEnemy();
	return UBDGameBalanceSettings::Get().GetCandidateFunds(Creep != nullptr ? Creep->MaxHealth : 0.0f, Wave);
}

int32 ABDMatchManager::GetBuildPriceOnWave(const UBDPlaceableData* Piece, const int32 Wave) const
{
	if (Piece == nullptr)
	{
		return 0;
	}

	// The dividers are a hand of their own and cost no public money: drawing the path
	// never competes with defending it.
	if (Piece->GetPieceKind() == EBDPieceKind::Divider)
	{
		return 0;
	}

	const UBDEnemyData* Creep = UBDWaveSettings::Get().ResolveWaveEnemy();
	return UBDGameBalanceSettings::Get().GetReplacementCost(Piece->GetBuildCost(), Creep != nullptr ? Creep->MaxHealth : 0.0f, FMath::Max(1, Wave));
}

int32 ABDMatchManager::GetBuildPrice(const UBDPlaceableData* Piece) const
{
	return GetBuildPriceOnWave(Piece, GetPriceWave());
}

int32 ABDMatchManager::GetUnlockWave(const UBDPlaceableData* Piece)
{
	return UBDPlacementSettings::Get().GetUnlockWave(Piece);
}

float ABDMatchManager::GetSellRefundRatio(const EBDPieceKind Kind) const
{
	return UBDGameBalanceSettings::Get().GetSellRefundRatio(Kind, CurrentWave);
}

int32 ABDMatchManager::GetSellRefund(const EBDPieceKind Kind, const int32 PaidCost) const
{
	return FMath::RoundToInt(FMath::Max(0, PaidCost) * GetSellRefundRatio(Kind));
}

int32 ABDMatchManager::RefundSale(const EBDPieceKind Kind, const int32 PaidCost)
{
	const int32 Refund = GetSellRefund(Kind, PaidCost);
	PayRefund(Refund, TEXT("a piece sold"));
	return Refund;
}

float ABDMatchManager::GetMoveTaxRate() const
{
	return UBDGameBalanceSettings::Get().GetMoveTaxRate(CurrentWave);
}

int32 ABDMatchManager::GetMoveCost(const int32 PaidCost) const
{
	return FMath::RoundToInt(FMath::Max(0, PaidCost) * GetMoveTaxRate());
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

bool ABDMatchManager::IsPlaceableKind(const EBDPieceKind Kind)
{
	return Kind == EBDPieceKind::Divider || Kind == EBDPieceKind::Platform
		|| Kind == EBDPieceKind::Tower || Kind == EBDPieceKind::Character
		|| Kind == EBDPieceKind::Objective;
}

bool ABDMatchManager::HasBudgetCeiling(const EBDPieceKind Kind)
{
	// The maze is a hand: so many dividers, so many platforms, and one urn. Defenders are
	// not counted at all - a tower is stopped by the money and by the cells left, a
	// character by the money and by a slot to stand on.
	return Kind == EBDPieceKind::Divider || Kind == EBDPieceKind::Platform || Kind == EBDPieceKind::Objective;
}

int32* ABDMatchManager::FindBudget(const EBDPieceKind Kind)
{
	switch (Kind)
	{
	case EBDPieceKind::Divider:
		return &DividersRemaining;

	case EBDPieceKind::Platform:
		return &PlatformsRemaining;

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
	if (!IsPlaceableKind(Kind))
	{
		return false;
	}

	const int32* Budget = FindBudget(Kind);
	if (Budget != nullptr && *Budget <= 0)
	{
		return false;
	}

	if (Kind == EBDPieceKind::Tower || Kind == EBDPieceKind::Character)
	{
		// New pieces go down between waves only, defenders like the rest: under a wave the
		// player reacts by evolving what stands, not by dropping more on the board.
		if (Phase != EBDMatchPhase::Building)
		{
			return false;
		}

		// And a character still needs somewhere to stand. Not a budget: a platform with
		// every slot taken is a board that has no room for one more, and saying so here
		// is what keeps the button honest instead of letting the click find out.
		return Kind != EBDPieceKind::Character || GetFreeCharacterSlots() > 0;
	}

	// The maze grows between waves, never under a running one: a fence dropped in front of
	// a creep mid route is a different game. The urn alone stays where the first wave
	// found it - everything was built around it.
	return Phase == EBDMatchPhase::Building && (Kind != EBDPieceKind::Objective || !IsBuildLocked());
}

bool ABDMatchManager::ConsumeBudget(const EBDPieceKind Kind)
{
	if (!CanPlace(Kind))
	{
		return false;
	}

	// Nothing to charge for an uncounted kind: the money is the whole price of a defender.
	if (int32* Budget = FindBudget(Kind))
	{
		--(*Budget);
		UE_LOG(LogBDMatch, Verbose, TEXT("Budget for %s is now %d."),
			*StaticEnum<EBDPieceKind>()->GetNameStringByValue(static_cast<int64>(Kind)), *Budget);
	}

	return true;
}

bool ABDMatchManager::CanRemove(const EBDPieceKind Kind) const
{
	if (!IsPlaceableKind(Kind))
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
	// A piece taken back is a piece held again, whenever it happens - when the kind is
	// held at all. A defender sold frees its cell or its slot, and that is the whole of
	// what comes back to the hand. What the sale was worth is a separate matter, settled
	// in public money by RefundSale.
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
			TEXT("Phase %s | wave %d of %d%s (health x%.2f) | %.1fs to next | dividers %d | platforms %d | free slots %d | objectives %d | speed %.0fx | bonus %d | votes %d blue / %d red | %d public money"),
			*StaticEnum<EBDMatchPhase>()->GetNameStringByValue(static_cast<int64>(Match->GetPhase())),
			Match->GetCurrentWave(), Match->GetWavesToWin(),
			Match->IsEndless() ? TEXT(" endless") : Match->HasWon() ? TEXT(" won") : TEXT(""),
			Match->GetHealthScale(), Match->GetTimeUntilNextWave(),
			Match->GetDividersRemaining(), Match->GetPlatformsRemaining(),
			Match->GetFreeCharacterSlots(), Match->GetObjectivesRemaining(),
			Match->GetGameSpeed(), Match->GetEarlyCallBonus(), Match->GetVotesBlue(), Match->GetVotesRed(), Match->GetPublicMoney());
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

	static void ExecEndless(const TArray<FString>& Args, UWorld* World)
	{
		if (ABDMatchManager* Match = FindMatch(World))
		{
			Match->ContinueEndless();
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdEndless(
		TEXT("BD.Match.Endless"),
		TEXT("BD.Match.Endless: from Victory, keeps the match going into endless waves."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecEndless));

	static void ExecSaveWrite(const TArray<FString>& Args, UWorld* World)
	{
		if (ABDMatchManager* Match = FindMatch(World))
		{
			Match->SaveMatch();
		}
	}

	static void ExecSaveLoad(const TArray<FString>& Args, UWorld* World)
	{
		if (ABDMatchManager* Match = FindMatch(World))
		{
			Match->LoadMatch();
		}
	}

	static void ExecSaveStatus(const TArray<FString>& Args, UWorld* World)
	{
		const ABDMatchManager* Match = FindMatch(World);
		const UBDMatchSave* Save = UBDMatchSave::LoadFromSlot();
		UE_LOG(LogBDMatch, Log, TEXT("Saves left: %d. Slot: %s"),
			Match != nullptr ? Match->GetSavesRemaining() : 0,
			Save != nullptr
				? *FString::Printf(TEXT("wave %d, seed %d, %d piece(s), votes %d/%d, %d save(s) left, written %s."),
					Save->Wave, Save->ObstacleSeed, Save->Pieces.Num(), Save->VotesBlue, Save->VotesRed, Save->SavesRemaining, *Save->SavedAt.ToString())
				: TEXT("empty."));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSaveWrite(
		TEXT("BD.Save.Write"),
		TEXT("BD.Save.Write: spends a save and writes the match to the slot."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSaveWrite));

	static FAutoConsoleCommandWithWorldAndArgs CmdSaveLoad(
		TEXT("BD.Save.Load"),
		TEXT("BD.Save.Load: rebuilds the match from the slot, in place."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSaveLoad));

	static FAutoConsoleCommandWithWorldAndArgs CmdSaveStatus(
		TEXT("BD.Save.Status"),
		TEXT("BD.Save.Status: logs the saves left and what the slot holds."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSaveStatus));

	static void ExecProgressStatus(const TArray<FString>& Args, UWorld* World)
	{
		const UEnum* DifficultyEnum = StaticEnum<EBDDifficulty>();
		FString Won;
		for (uint8 Index = 0; Index < static_cast<uint8>(EBDDifficulty::Count); ++Index)
		{
			const EBDDifficulty Difficulty = static_cast<EBDDifficulty>(Index);
			Won += FString::Printf(TEXT(" %s:%s"), *DifficultyEnum->GetNameStringByValue(Index),
				UBDProgressSave::HasWonDifficulty(Difficulty) ? TEXT("won") : TEXT("-"));
		}

		const ABDMatchManager* Match = FindMatch(World);
		UE_LOG(LogBDMatch, Log, TEXT("Progress:%s. This match: %s, chain bonus %s."), *Won,
			Match != nullptr ? *DifficultyEnum->GetNameStringByValue(static_cast<int64>(Match->Difficulty)) : TEXT("none"),
			Match != nullptr && Match->WasChainBonusApplied() ? TEXT("applied") : TEXT("not applied"));
	}

	static void ExecProgressSetWon(const TArray<FString>& Args, UWorld* World)
	{
		const UEnum* DifficultyEnum = StaticEnum<EBDDifficulty>();
		const int64 Value = Args.Num() == 1 ? DifficultyEnum->GetValueByNameString(Args[0]) : INDEX_NONE;
		if (Value == INDEX_NONE || Value >= static_cast<int64>(EBDDifficulty::Count))
		{
			UE_LOG(LogBDMatch, Error, TEXT("Usage: BD.Progress.SetWon <Easy|Normal|Hard>"));
			return;
		}
		UBDProgressSave::RecordWin(static_cast<EBDDifficulty>(Value));
	}

	static void ExecProgressReset(const TArray<FString>& Args, UWorld* World)
	{
		UBDProgressSave::ResetProgress();
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdProgressStatus(
		TEXT("BD.Progress.Status"),
		TEXT("BD.Progress.Status: logs which difficulties have been won and whether this match got the chain bonus."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecProgressStatus));

	static FAutoConsoleCommandWithWorldAndArgs CmdProgressSetWon(
		TEXT("BD.Progress.SetWon"),
		TEXT("BD.Progress.SetWon <Easy|Normal|Hard>: records a win without playing it."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecProgressSetWon));

	static FAutoConsoleCommandWithWorldAndArgs CmdProgressReset(
		TEXT("BD.Progress.Reset"),
		TEXT("BD.Progress.Reset: forgets every win."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecProgressReset));

	/**
	 * The horde of a wave against the best defense the budgets buy: every defender of the
	 * palette at the reference level, hitting for the reference fraction of the time a
	 * creep spends on the average route. Ratio under 1 means the defense out-damages the
	 * wave; the last line says what health growth would make the winning wave a draw.
	 */
	static void ExecBalanceReport(const TArray<FString>& Args, UWorld* World)
	{
		const ABDMatchManager* Match = FindMatch(World);
		UBDWaveSubsystem* Waves = World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr;
		const UBDEnemyData* Enemy = UBDWaveSettings::Get().ResolveWaveEnemy();
		if (Match == nullptr || Waves == nullptr || Enemy == nullptr)
		{
			UE_LOG(LogBDMatch, Error, TEXT("BD.Balance.Report needs a match, the wave subsystem and a wave enemy."));
			return;
		}

		const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();

		// The defense: the reference count x DPS at the reference level, per kind, from the palette.
		float TowerDps = 0.0f;
		int32 TowerKinds = 0;
		float CharacterDps = 0.0f;
		int32 CharacterKinds = 0;
		for (const TSoftObjectPtr<UBDPlaceableData>& Entry : UBDPlacementSettings::Get().Palette)
		{
			const UBDPlaceableData* Data = Entry.LoadSynchronous();
			const UBDTowerData* TowerData = Data != nullptr ? Data->TowerData.LoadSynchronous() : nullptr;
			const FBDTowerLevel* Base = TowerData != nullptr ? TowerData->GetLevel(1) : nullptr;
			if (Base == nullptr)
			{
				continue;
			}
			const float Dps = Base->Damage * Base->FireRate * Balance.GetUpgradeDamageScale(Balance.ReferenceMaxLevel);
			if (Data->GetPieceKind() == EBDPieceKind::Tower) { TowerDps += Dps; ++TowerKinds; }
			else if (Data->GetPieceKind() == EBDPieceKind::Character) { CharacterDps += Dps; ++CharacterKinds; }
		}
		// Nothing caps how many defenders get built any more, so the report is told what a
		// full board looks like rather than reading it off a ceiling that no longer exists.
		const int32 TowerCount = Balance.ReferenceTowerCount;
		const int32 CharacterCount = Balance.ReferenceCharacterCount;
		const float DefenseDps = (TowerKinds > 0 ? TowerCount * TowerDps / TowerKinds : 0.0f)
			+ (CharacterKinds > 0 ? CharacterCount * CharacterDps / CharacterKinds : 0.0f);

		// The route: average length over the mouths that have one, at the creep's speed.
		const TArray<FBDSpawnPoint>& Points = Waves->GetSpawnPoints();
		int32 RouteCells = 0;
		int32 Routed = 0;
		for (const FBDSpawnPoint& Point : Points)
		{
			if (Point.Route.Num() > 0) { RouteCells += Point.Route.Num(); ++Routed; }
		}
		const float AverageRoute = Routed > 0 ? static_cast<float>(RouteCells) / Routed : 0.0f;
		const float RouteSeconds = Enemy->MoveSpeed > 0.0f ? AverageRoute / Enemy->MoveSpeed : 0.0f;
		const float CapacityPerWave = DefenseDps * RouteSeconds * Balance.ReferenceEngagementEfficiency;

		UE_LOG(LogBDMatch, Log, TEXT("Balance: defense %.0f dps at level %d (%d towers, %d characters), route %.0f cells = %.0fs at %.2f cells/s, efficiency %.0f%% -> %.0f damage per wave."),
			DefenseDps, Balance.ReferenceMaxLevel, TowerCount, CharacterCount, AverageRoute, RouteSeconds, Enemy->MoveSpeed,
			Balance.ReferenceEngagementEfficiency * 100.0f, CapacityPerWave);

		const int32 WavesToWin = Match->GetWavesToWin();
		const int32 Only = Args.Num() == 1 ? FCString::Atoi(*Args[0]) : 0;
		for (int32 Wave = 1; Wave <= FMath::Max(WavesToWin, Only); ++Wave)
		{
			if (Only == 0 ? (Wave != 1 && Wave != WavesToWin && Wave % 5 != 0) : Wave != Only)
			{
				continue;
			}
			const int32 Creeps = Balance.GetCreepsPerSpawnPoint(Wave) * Points.Num();
			const float Health = Enemy->MaxHealth * Balance.GetHealthScale(Wave);
			const float Horde = Creeps * Health;
			UE_LOG(LogBDMatch, Log, TEXT("  wave %2d: %3d creeps x %6.0f hp = %8.0f | ratio to defense %.2f"),
				Wave, Creeps, Health, Horde, CapacityPerWave > 0.0f ? Horde / CapacityPerWave : 0.0f);
		}

		// What the exponential fallback would need so the winning wave comes out even.
		const int32 FinalCreeps = Balance.GetCreepsPerSpawnPoint(WavesToWin) * Points.Num();
		if (FinalCreeps > 0 && Enemy->MaxHealth > 0.0f && CapacityPerWave > 0.0f && WavesToWin > 1)
		{
			const float NeededScale = CapacityPerWave / (FinalCreeps * Enemy->MaxHealth);
			UE_LOG(LogBDMatch, Log, TEXT("  a draw on wave %d needs health x%.2f there: HealthScaleGrowth %.4f per wave (curve unset), or the curve ending at %.2f."),
				WavesToWin, NeededScale, FMath::Pow(NeededScale, 1.0f / (WavesToWin - 1)), NeededScale);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdBalanceReport(
		TEXT("BD.Balance.Report"),
		TEXT("BD.Balance.Report [wave]: horde health per wave against the best defense the budgets buy, and the growth that would make the winning wave even."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecBalanceReport));
}

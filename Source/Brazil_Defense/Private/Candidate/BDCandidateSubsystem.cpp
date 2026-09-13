// Brazil Defense. When the red candidate comes out, and what its end does to the match.

#include "Candidate/BDCandidateSubsystem.h"

#include "BDLog.h"
#include "Enemy/BDCandidate.h"
#include "Enemy/BDEnemyData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Match/BDGameBalanceSettings.h"
#include "Match/BDMatchManager.h"
#include "Stats/Stats.h"
#include "Wave/BDWaveSettings.h"
#include "Wave/BDWaveSubsystem.h"

void UBDCandidateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The candidate is spawned through the wave subsystem, which has to be there first.
	Collection.InitializeDependency<UBDWaveSubsystem>();
}

void UBDCandidateSubsystem::Deinitialize()
{
	if (ABDMatchManager* Match = BoundMatch.Get())
	{
		Match->OnVotesChanged.Remove(VotesChangedHandle);
		Match->OnWaveStarted.Remove(WaveStartedHandle);
		Match->OnPhaseChanged.Remove(PhaseChangedHandle);
	}
	BoundMatch.Reset();
	Candidate.Reset();

	Super::Deinitialize();
}

TStatId UBDCandidateSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBDCandidateSubsystem, STATGROUP_Tickables);
}

UBDCandidateSubsystem* UBDCandidateSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	return World != nullptr ? World->GetSubsystem<UBDCandidateSubsystem>() : nullptr;
}

ABDMatchManager* UBDCandidateSubsystem::GetMatch() const
{
	return ABDMatchManager::Get(GetWorld());
}

UBDWaveSubsystem* UBDCandidateSubsystem::GetWaves() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr;
}

void UBDCandidateSubsystem::EnsureMatchBinding()
{
	if (BoundMatch.IsValid())
	{
		return;
	}

	ABDMatchManager* Match = GetMatch();
	if (Match == nullptr)
	{
		return;
	}

	VotesChangedHandle = Match->OnVotesChanged.AddUObject(this, &UBDCandidateSubsystem::HandleVotesChanged);
	WaveStartedHandle = Match->OnWaveStarted.AddUObject(this, &UBDCandidateSubsystem::HandleWaveStarted);
	PhaseChangedHandle = Match->OnPhaseChanged.AddUObject(this, &UBDCandidateSubsystem::HandlePhaseChanged);
	BoundMatch = Match;
}

void UBDCandidateSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	EnsureMatchBinding();

	if (PauseRemaining <= 0.0f)
	{
		return;
	}

	// DeltaTime is dilated, so the pause follows the game speed like everything else.
	PauseRemaining -= DeltaTime;
	if (PauseRemaining > 0.0f)
	{
		return;
	}

	PauseRemaining = 0.0f;

	const ABDMatchManager* Match = GetMatch();
	UE_LOG(LogBDCandidate, Log, TEXT("Pause over: waves resume and the red count moves again. %d blue / %d red%s."),
		Match != nullptr ? Match->GetVotesBlue() : 0, Match != nullptr ? Match->GetVotesRed() : 0,
		IsScoreboardInverted() ? TEXT(", still inverted: the candidate returns with the next wave") : TEXT(""));
}

//~ Trigger -----------------------------------------------------------------------

bool UBDCandidateSubsystem::IsScoreboardInverted() const
{
	// Red ahead means red scored at least once; the second check is the rule spelled
	// out: a player who let nobody through never meets the candidate.
	const ABDMatchManager* Match = GetMatch();
	return Match != nullptr && Match->GetVotesRed() > 0 && Match->GetVotesRed() > Match->GetVotesBlue();
}

void UBDCandidateSubsystem::HandleVotesChanged(const int32 Blue, const int32 Red)
{
	EvaluateTrigger(TEXT("the votes moved"));
}

void UBDCandidateSubsystem::HandleWaveStarted(const int32 Wave)
{
	// A kill disarms the trigger; the next wave going out is what arms it again.
	bArmed = true;
	EvaluateTrigger(TEXT("the wave went out"));
}

void UBDCandidateSubsystem::HandlePhaseChanged(const EBDMatchPhase NewPhase)
{
	// The match rewound to its start from the console: nothing of the last one carries over.
	const ABDMatchManager* Match = GetMatch();
	if (NewPhase == EBDMatchPhase::Building && Match != nullptr && Match->GetCurrentWave() == 0)
	{
		if (PauseRemaining > 0.0f)
		{
			UE_LOG(LogBDCandidate, Log, TEXT("Pause dropped: the match was rewound."));
		}
		PauseRemaining = 0.0f;
		bArmed = true;
	}
}

void UBDCandidateSubsystem::EvaluateTrigger(const TCHAR* Why)
{
	if (!bArmed || IsCountFrozen() || Candidate.IsValid())
	{
		return;
	}

	const ABDMatchManager* Match = GetMatch();
	if (Match == nullptr || (Match->GetPhase() != EBDMatchPhase::Building && Match->GetPhase() != EBDMatchPhase::WaveActive))
	{
		return;
	}

	if (IsScoreboardInverted())
	{
		SpawnCandidate(Why);
	}
}

ABDCandidate* UBDCandidateSubsystem::SpawnCandidate(const TCHAR* Why)
{
	if (const ABDCandidate* Existing = Candidate.Get())
	{
		UE_LOG(LogBDCandidate, Warning, TEXT("No second candidate: %s is still on the board (%.0f/%.0f health)."),
			*Existing->GetName(), Existing->GetCurrentHealth(), Existing->GetMaxHealth());
		return nullptr;
	}

	ABDMatchManager* Match = GetMatch();
	UBDWaveSubsystem* Waves = GetWaves();
	if (Match == nullptr || Waves == nullptr)
	{
		UE_LOG(LogBDCandidate, Error, TEXT("The candidate needs a running match and a wave subsystem."));
		return nullptr;
	}

	if (Match->GetPhase() == EBDMatchPhase::Defeat || Match->GetPhase() == EBDMatchPhase::Victory)
	{
		UE_LOG(LogBDCandidate, Warning, TEXT("No candidate: the match is over."));
		return nullptr;
	}

	const UBDWaveSettings& Settings = UBDWaveSettings::Get();
	const UBDEnemyData* Data = Settings.CandidateEnemy.LoadSynchronous();
	if (Data == nullptr)
	{
		UE_LOG(LogBDCandidate, Error, TEXT("No candidate to send: set Candidate Enemy in Project Settings > Brazil Defense - Waves."));
		return nullptr;
	}

	// Drawn among the mouths that can reach the urn: one the player walled off is no mouth.
	TArray<int32> Usable;
	const TArray<FBDSpawnPoint>& Points = Waves->GetSpawnPoints();
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		if (Points[Index].Route.Num() > 0)
		{
			Usable.Add(Index);
		}
	}

	if (Usable.Num() == 0)
	{
		if (!bWarnedNoMouth)
		{
			bWarnedNoMouth = true;
			UE_LOG(LogBDCandidate, Warning, TEXT("The scoreboard calls for the candidate but no mouth has a route to the urn. It waits."));
		}
		return nullptr;
	}
	bWarnedNoMouth = false;

	// Seeded off the match, the wave and how many came before, like the mouths of a wave:
	// the same match played again meets its candidate at the same door.
	FRandomStream Stream(static_cast<int32>(HashCombine(
		HashCombine(::GetTypeHash(Match->ObstacleSeed), ::GetTypeHash(Match->GetCurrentWave())),
		::GetTypeHash(CandidatesSent))));
	const int32 Mouth = Usable[Stream.RandRange(0, Usable.Num() - 1)];

	// As tough as the creeps of the wave, times the multiplier. The health on its own
	// asset is only the fallback for a project with no wave enemy set.
	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
	const UBDEnemyData* WaveCreep = Settings.ResolveWaveEnemy();
	const float CreepHealth = (WaveCreep != nullptr ? WaveCreep->MaxHealth : Data->MaxHealth) * Match->GetHealthScale();
	const float Health = FMath::Max(1.0f, CreepHealth * FMath::Max(1.0f, Balance.CandidateHealthMultiplier));

	// The class on the asset when it is a candidate; a plain creep class there would walk
	// out as a creep, and this is the one spawn where that is not acceptable.
	UClass* Class = Data->EnemyClass.IsNull() ? nullptr : Data->EnemyClass.LoadSynchronous();
	if (Class == nullptr || !Class->IsChildOf(ABDCandidate::StaticClass()))
	{
		UE_CLOG(Class != nullptr, LogBDCandidate, Warning, TEXT("%s names %s as its class, which is not a BD Candidate; BDCandidate is used instead."),
			*Data->GetName(), *Class->GetName());
		Class = ABDCandidate::StaticClass();
	}

	ABDCandidate* Spawned = Cast<ABDCandidate>(Waves->SpawnEnemyAs(Class, Data, Mouth, Health));
	if (Spawned == nullptr)
	{
		UE_LOG(LogBDCandidate, Error, TEXT("The candidate failed to spawn out of mouth %d."), Mouth);
		return nullptr;
	}

	Candidate = Spawned;
	++CandidatesSent;

	UE_LOG(LogBDCandidate, Log, TEXT("CANDIDATE out on wave %d from mouth %d with %.0f health (%d of the match, because %s): %d blue / %d red."),
		Match->GetCurrentWave(), Mouth, Spawned->GetMaxHealth(), CandidatesSent, Why,
		Match->GetVotesBlue(), Match->GetVotesRed());

	return Spawned;
}

//~ Reports from the candidate ----------------------------------------------------

void UBDCandidateSubsystem::NotifyCandidateArrived(ABDCandidate* Arrived)
{
	if (Arrived == nullptr)
	{
		return;
	}

	if (Arrived != Candidate.Get())
	{
		// Spawned by hand through the wave console, never tracked here. It still ends the
		// match: a candidate at the urn is a candidate at the urn.
		UE_LOG(LogBDCandidate, Warning, TEXT("%s reached the urn without having been sent by the match."), *Arrived->GetName());
	}

	ABDMatchManager* Match = GetMatch();
	UE_LOG(LogBDCandidate, Log, TEXT("CANDIDATE reached the urn on wave %d after %.1fs: DEFEAT. %d blue / %d red."),
		Match != nullptr ? Match->GetCurrentWave() : 0, Arrived->GetTimeAlive(),
		Match != nullptr ? Match->GetVotesBlue() : 0, Match != nullptr ? Match->GetVotesRed() : 0);

	Candidate.Reset();
	if (Match != nullptr)
	{
		Match->DeclareDefeat(TEXT("the candidate reached the urn"));
	}
}

void UBDCandidateSubsystem::NotifyCandidateKilled(ABDCandidate* Killed)
{
	if (Killed == nullptr)
	{
		return;
	}

	if (Killed != Candidate.Get())
	{
		UE_LOG(LogBDCandidate, Verbose, TEXT("%s killed; it was not the candidate of the match, nothing pauses."), *Killed->GetName());
		return;
	}

	Candidate.Reset();
	bArmed = false;

	const ABDMatchManager* Match = GetMatch();
	const float PauseSeconds = FMath::Max(0.0f, UBDGameBalanceSettings::Get().CandidateKillPauseSeconds);
	PauseRemaining = PauseSeconds;

	UE_LOG(LogBDCandidate, Log, TEXT("CANDIDATE killed on wave %d after %.1fs alive: no votes for it. %d blue / %d red."),
		Match != nullptr ? Match->GetCurrentWave() : 0, Killed->GetTimeAlive(),
		Match != nullptr ? Match->GetVotesBlue() : 0, Match != nullptr ? Match->GetVotesRed() : 0);

	if (PauseSeconds > 0.0f)
	{
		UE_LOG(LogBDCandidate, Log, TEXT("Pause started: no wave goes out and the red count is frozen for %.0fs. Creeps already out keep walking."), PauseSeconds);
	}
}

// Brazil Defense. The candidates: the bosses of the match, and what the count does with them.

#include "Candidate/BDCandidateSubsystem.h"

#include "BDLog.h"
#include "Bribe/BDBribeSubsystem.h"
#include "Enemy/BDCandidate.h"
#include "Enemy/BDEnemyData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Match/BDGameBalanceSettings.h"
#include "Match/BDMatchManager.h"
#include "Stats/Stats.h"
#include "Wave/BDWaveSettings.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDCandidatePrivate
{
	/** A distinct colour per ordinal, by the golden angle round the hue wheel: blocking-out debug, nothing more. */
	static FLinearColor TintForOrdinal(const int32 Ordinal)
	{
		const float Hue = FMath::Fmod(static_cast<float>(FMath::Max(0, Ordinal - 1)) * 137.508f, 360.0f);
		return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue / 360.0f * 255.0f), 230, 255);
	}
}

void UBDCandidateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// The candidates are spawned through the wave subsystem, which has to be there first.
	Collection.InitializeDependency<UBDWaveSubsystem>();
}

void UBDCandidateSubsystem::Deinitialize()
{
	if (ABDMatchManager* Match = BoundMatch.Get())
	{
		Match->OnVotesChanged.Remove(VotesChangedHandle);
		Match->OnPhaseChanged.Remove(PhaseChangedHandle);
	}
	if (UBDWaveSubsystem* Waves = BoundWaves.Get())
	{
		Waves->OnWaveDealt.Remove(WaveDealtHandle);
	}
	BoundMatch.Reset();
	BoundWaves.Reset();
	Living.Reset();

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
	PhaseChangedHandle = Match->OnPhaseChanged.AddUObject(this, &UBDCandidateSubsystem::HandlePhaseChanged);
	BoundMatch = Match;

	if (UBDWaveSubsystem* Waves = GetWaves())
	{
		WaveDealtHandle = Waves->OnWaveDealt.AddUObject(this, &UBDCandidateSubsystem::HandleWaveDealt);
		BoundWaves = Waves;
	}
}

//~ State ------------------------------------------------------------------------

ABDCandidate* UBDCandidateSubsystem::GetCandidate() const
{
	for (const TWeakObjectPtr<ABDCandidate>& Weak : Living)
	{
		if (ABDCandidate* Candidate = Weak.Get())
		{
			return Candidate;
		}
	}
	return nullptr;
}

void UBDCandidateSubsystem::GetLivingCandidates(TArray<ABDCandidate*>& OutCandidates) const
{
	OutCandidates.Reset();
	for (const TWeakObjectPtr<ABDCandidate>& Weak : Living)
	{
		if (ABDCandidate* Candidate = Weak.Get())
		{
			OutCandidates.Add(Candidate);
		}
	}
}

int32 UBDCandidateSubsystem::GetReturnRemaining() const
{
	return bReturnActive ? ReturnQueue.Num() + ReturnAlive : 0;
}

bool UBDCandidateSubsystem::IsScoreboardInverted() const
{
	const ABDMatchManager* Match = GetMatch();
	return Match != nullptr && Match->GetVotesRed() > 0 && Match->GetVotesRed() > Match->GetVotesBlue();
}

//~ Tick ---------------------------------------------------------------------------

void UBDCandidateSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	EnsureMatchBinding();

	// Drop the dead weak pointers so the counts stay honest.
	Living.RemoveAll([](const TWeakObjectPtr<ABDCandidate>& Weak) { return !Weak.IsValid(); });

	if (!bReturnActive || ReturnQueue.Num() == 0)
	{
		return;
	}

	// The parade: one more of the fallen at each interval, in the order they first fell.
	ReturnTimer -= DeltaTime;
	if (ReturnTimer > 0.0f)
	{
		return;
	}
	ReturnTimer = ReturnInterval;

	const FBDCandidateRecord Record = ReturnQueue[0];
	if (SpawnFromRecord(Record, /*bReturning*/ true, TEXT("the fallen return")) != nullptr)
	{
		ReturnQueue.RemoveAt(0);
		++ReturnAlive;
	}
}

//~ Match events -----------------------------------------------------------------

void UBDCandidateSubsystem::HandleVotesChanged(const int32 Blue, const int32 Red)
{
	// The count turning red is what brings the fallen back; while they are back, or with
	// none fallen, nothing happens here.
	if (!bReturnActive && Fallen.Num() > 0 && IsScoreboardInverted())
	{
		BeginReturn(TEXT("red passed blue"));
	}
}

void UBDCandidateSubsystem::HandleWaveDealt(const int32 Wave)
{
	const int32 Interval = FMath::Max(0, UBDGameBalanceSettings::Get().CandidateInterval);
	const bool bDue = Interval > 0 && Wave % Interval == 0;
	if (!bDue && !bSchedulePending)
	{
		return;
	}

	// One scheduled candidate at a time: a wave that finds one still walking hands its
	// candidate to the next wave that finds the board clear of them.
	if (HasCandidateOnBoard() || bReturnActive)
	{
		bSchedulePending = true;
		UE_LOG(LogBDCandidate, Log, TEXT("Wave %d owes a candidate: one is still walking, he comes with a later wave."), Wave);
		return;
	}

	bSchedulePending = false;
	if (SpawnCandidate(bDue ? TEXT("the schedule") : TEXT("the schedule, owed from an earlier wave")) == nullptr)
	{
		return;
	}

	// He is the first thing out of the buses on his wave: the creeps wait until he has
	// had a head start, so the player sees him walk out alone and can make him the
	// priority. In the middle of the horde he went by unnoticed.
	if (UBDWaveSubsystem* Waves = GetWaves())
	{
		Waves->HoldWaveSpawns(UBDGameBalanceSettings::Get().CandidateLeadSeconds, TEXT("the candidate walks out first"));
	}
}

void UBDCandidateSubsystem::HandlePhaseChanged(const EBDMatchPhase NewPhase)
{
	// The match rewound to its start from the console or a load: nothing carries over.
	const ABDMatchManager* Match = GetMatch();
	if (NewPhase == EBDMatchPhase::Building && Match != nullptr && Match->GetCurrentWave() == 0)
	{
		ResetForNewMatch();
	}
}

void UBDCandidateSubsystem::ResetForNewMatch()
{
	if (Sent.Num() > 0 || bReturnActive)
	{
		UE_LOG(LogBDCandidate, Log, TEXT("Candidates forgotten: the match was rewound."));
	}
	Living.Reset();
	Sent.Reset();
	Fallen.Reset();
	ReturnQueue.Reset();
	bReturnActive = false;
	bSchedulePending = false;
	ReturnAlive = 0;
	ReturnsStarted = 0;
}

//~ Sending them out ---------------------------------------------------------------

ABDCandidate* UBDCandidateSubsystem::SpawnFromRecord(const FBDCandidateRecord& Record, const bool bReturning, const TCHAR* Why)
{
	ABDMatchManager* Match = GetMatch();
	UBDWaveSubsystem* Waves = GetWaves();
	if (Match == nullptr || Waves == nullptr)
	{
		UE_LOG(LogBDCandidate, Error, TEXT("A candidate needs a running match and a wave subsystem."));
		return nullptr;
	}

	if (Match->IsMatchOver())
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
			UE_LOG(LogBDCandidate, Warning, TEXT("A candidate is due but no mouth has a route to the urn. He waits."));
		}
		return nullptr;
	}
	bWarnedNoMouth = false;

	// Seeded off the match, the wave and the ordinal, like the mouths of a wave: the same
	// match played again meets its candidates at the same doors.
	FRandomStream Stream(static_cast<int32>(HashCombine(
		HashCombine(::GetTypeHash(Match->ObstacleSeed), ::GetTypeHash(Match->GetCurrentWave())),
		::GetTypeHash(Record.Ordinal * (bReturning ? 31 : 1)))));
	const int32 Mouth = Usable[Stream.RandRange(0, Usable.Num() - 1)];

	// The class on the asset when it is a candidate; a plain creep class there would walk
	// out as a creep, and this is the one spawn where that is not acceptable.
	UClass* Class = Data->EnemyClass.IsNull() ? nullptr : Data->EnemyClass.LoadSynchronous();
	if (Class == nullptr || !Class->IsChildOf(ABDCandidate::StaticClass()))
	{
		UE_CLOG(Class != nullptr, LogBDCandidate, Warning, TEXT("%s names %s as its class, which is not a BD Candidate; BDCandidate is used instead."),
			*Data->GetName(), *Class->GetName());
		Class = ABDCandidate::StaticClass();
	}

	ABDCandidate* Spawned = Cast<ABDCandidate>(Waves->SpawnEnemyAs(Class, Data, Mouth, FMath::Max(1.0f, Record.MaxHealth)));
	if (Spawned == nullptr)
	{
		UE_LOG(LogBDCandidate, Error, TEXT("Candidate %d failed to spawn out of mouth %d."), Record.Ordinal, Mouth);
		return nullptr;
	}

	Spawned->Ordinal = Record.Ordinal;
	Spawned->bReturning = bReturning;
	Spawned->SetDebugTint(BDCandidatePrivate::TintForOrdinal(Record.Ordinal));
	// "Candidate N" until the twenty have names of their own.
	FFormatNamedArguments NameArgs;
	NameArgs.Add(TEXT("Ordinal"), Record.Ordinal);
	Spawned->DisplayName = FText::Format(NSLOCTEXT("BrazilDefense", "CandidateName", "Candidate {Ordinal}"), NameArgs);
	Living.Add(Spawned);

	UE_LOG(LogBDCandidate, Log, TEXT("CANDIDATE %d %s on wave %d from mouth %d with %.0f health (because %s): %d blue / %d red."),
		Record.Ordinal, bReturning ? TEXT("RETURNS") : TEXT("out"), Match->GetCurrentWave(), Mouth, Spawned->GetMaxHealth(), Why,
		Match->GetVotesBlue(), Match->GetVotesRed());

	return Spawned;
}

ABDCandidate* UBDCandidateSubsystem::SpawnCandidate(const TCHAR* Why)
{
	ABDMatchManager* Match = GetMatch();
	if (Match == nullptr)
	{
		return nullptr;
	}

	// As tough as the creeps of the wave, times the multiplier: the later, the tougher.
	// The same health the match prices pieces off (ABDMatchManager::GetCandidateFunds).
	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
	const UBDWaveSettings& Settings = UBDWaveSettings::Get();
	const UBDEnemyData* WaveCreep = Settings.ResolveWaveEnemy();
	const UBDEnemyData* Data = Settings.CandidateEnemy.LoadSynchronous();
	const float BaseHealth = WaveCreep != nullptr ? WaveCreep->MaxHealth : (Data != nullptr ? Data->MaxHealth : 100.0f);
	const float Health = Balance.GetCandidateHealth(BaseHealth, Match->GetCurrentWave());

	FBDCandidateRecord Record;
	Record.Ordinal = Sent.Num() + 1;
	Record.Wave = Match->GetCurrentWave();
	Record.MaxHealth = Health;

	ABDCandidate* Spawned = SpawnFromRecord(Record, /*bReturning*/ false, Why);
	if (Spawned != nullptr)
	{
		Sent.Add(Record);
	}
	return Spawned;
}

void UBDCandidateSubsystem::BeginReturn(const TCHAR* Why)
{
	ABDMatchManager* Match = GetMatch();
	if (Match == nullptr || Match->IsMatchOver())
	{
		return;
	}
	if (bReturnActive)
	{
		UE_LOG(LogBDCandidate, Warning, TEXT("The fallen are already returning."));
		return;
	}
	if (Fallen.Num() == 0)
	{
		UE_LOG(LogBDCandidate, Log, TEXT("The count turned (%s) but no candidate has fallen yet: nothing returns."), Why);
		return;
	}

	bReturnActive = true;
	++ReturnsStarted;
	ReturnQueue = Fallen;
	ReturnAlive = 0;
	const float Seconds = FMath::Max(1.0f, UBDGameBalanceSettings::Get().ReturnParadeSeconds);
	ReturnInterval = Seconds / ReturnQueue.Num();
	ReturnTimer = 0.0f;

	// The wave is theirs: whatever creeps it still had to send are not sent.
	if (UBDWaveSubsystem* Waves = GetWaves())
	{
		Waves->CancelRemainingSpawns(TEXT("the fallen candidates return"));
	}

	UE_LOG(LogBDCandidate, Log, TEXT("THE FALLEN RETURN (%s): %d candidate(s) over %.0fs, one every %.1fs, at the health they fell with. No creep walks until they are down. %d blue / %d red."),
		Why, ReturnQueue.Num(), Seconds, ReturnInterval, Match->GetVotesBlue(), Match->GetVotesRed());
}

//~ Reports from the candidates ----------------------------------------------------

void UBDCandidateSubsystem::NotifyCandidateArrived(ABDCandidate* Arrived)
{
	if (Arrived == nullptr)
	{
		return;
	}

	ABDMatchManager* Match = GetMatch();
	UE_LOG(LogBDCandidate, Log, TEXT("CANDIDATE %d%s reached the urn on wave %d after %.1fs: DEFEAT. %d blue / %d red."),
		Arrived->Ordinal, Arrived->bReturning ? TEXT(" (returning)") : TEXT(""),
		Match != nullptr ? Match->GetCurrentWave() : 0, Arrived->GetTimeAlive(),
		Match != nullptr ? Match->GetVotesBlue() : 0, Match != nullptr ? Match->GetVotesRed() : 0);

	Living.Remove(Arrived);
	if (Match != nullptr)
	{
		Match->DeclareDefeat(TEXT("a candidate reached the urn"));
	}
}

void UBDCandidateSubsystem::NotifyCandidateKilled(ABDCandidate* Killed)
{
	if (Killed == nullptr)
	{
		return;
	}

	const bool bTracked = Living.Remove(Killed) > 0;
	ABDMatchManager* Match = GetMatch();
	UE_LOG(LogBDCandidate, Log, TEXT("CANDIDATE %d%s killed on wave %d after %.1fs alive: no votes for it. %d blue / %d red."),
		Killed->Ordinal, Killed->bReturning ? TEXT(" (returning)") : TEXT(""),
		Match != nullptr ? Match->GetCurrentWave() : 0, Killed->GetTimeAlive(),
		Match != nullptr ? Match->GetVotesBlue() : 0, Match != nullptr ? Match->GetVotesRed() : 0);
	if (!bTracked)
	{
		return;
	}

	// A first death joins the fallen: from now on he is one of those who come back.
	const bool bKnown = Fallen.ContainsByPredicate([Killed](const FBDCandidateRecord& Record) { return Record.Ordinal == Killed->Ordinal; });
	if (!bKnown)
	{
		const FBDCandidateRecord* SentRecord = Sent.FindByPredicate([Killed](const FBDCandidateRecord& Record) { return Record.Ordinal == Killed->Ordinal; });
		if (SentRecord != nullptr)
		{
			Fallen.Add(*SentRecord);
		}
	}

	// A scheduled candidate down pays in bribe, and in nothing else: there are no ceilings
	// left to raise, so what a boss is worth is the bag that falls out of him, worth his
	// own health - the twentieth pays many times what the first did. The bribe subsystem
	// drops it, counts it and mints it. One of the fallen come back drops nothing, on
	// purpose: the return is a punishment, and paying for it would make the count worth
	// throwing.
	if (!Killed->bReturning)
	{
		const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
		if (UBDBribeSubsystem* Bribes = UBDBribeSubsystem::Get(this))
		{
			Bribes->Collect(Balance.BribeForHealth(Killed->GetMaxHealth()), Killed->GetActorLocation(), Killed->Ordinal);
		}
		return;
	}
	if (!bReturnActive)
	{
		return;
	}

	// The parade: all of them down levels the count downwards, blue to red. Nothing is
	// handed out; the bleeding stops, that is all.
	ReturnAlive = FMath::Max(0, ReturnAlive - 1);
	if (ReturnQueue.Num() == 0 && ReturnAlive == 0)
	{
		bReturnActive = false;
		if (Match != nullptr)
		{
			Match->EqualizeVotesDown(TEXT("every returning candidate is down"));
		}
		UE_LOG(LogBDCandidate, Log, TEXT("THE FALLEN ARE DOWN AGAIN: the count is levelled downwards to %d / %d. The waves go on."),
			Match != nullptr ? Match->GetVotesBlue() : 0, Match != nullptr ? Match->GetVotesRed() : 0);
	}
}

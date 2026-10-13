// Brazil Defense. A candidate's rally: he stops, speaks untouchable, and a crowd rises around him.

#include "Candidate/BDRallyComponent.h"

#include "Audio/BDSpeechMarks.h"
#include "BDLog.h"
#include "Candidate/BDCandidateSubsystem.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemy/BDCandidate.h"
#include "Enemy/BDCreepSoundSubsystem.h"
#include "Enemy/BDEnemyData.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Grid/BDGridSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Match/BDGameBalanceSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "Wave/BDWaveSettings.h"
#include "Wave/BDWaveSubsystem.h"

UBDRallyComponent::UBDRallyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

const TCHAR* UBDRallyComponent::StateName(const EBDRallyState InState)
{
	switch (InState)
	{
	case EBDRallyState::Tell: return TEXT("Tell");
	case EBDRallyState::Speaking: return TEXT("Speaking");
	case EBDRallyState::Spawning: return TEXT("Spawning");
	default: return TEXT("Walking");
	}
}

ABDCandidate* UBDRallyComponent::GetCandidate() const
{
	return Cast<ABDCandidate>(GetOwner());
}

void UBDRallyComponent::Setup(const FBDRallyParams& InParams, const bool bInEnabled, const int32 Seed)
{
	Params = InParams;
	bEnabled = bInEnabled && Params.RallyCount >= 0;
	Stream.Initialize(Seed);
	State = EBDRallyState::Walking;
	NextRallyIn = FMath::Max(0.0f, Params.RallyFirstDelay);
	StateRemaining = 0.0f;
}

float UBDRallyComponent::DrawInterval()
{
	const float Jitter = FMath::Clamp(Params.RallyEveryJitter, 0.0f, 0.9f);
	return FMath::Max(1.0f, Params.RallyEvery * (1.0f + Stream.FRandRange(-Jitter, Jitter)));
}

bool UBDRallyComponent::ForceRally(FString& OutWhy)
{
	if (!bEnabled)
	{
		OutWhy = TEXT("his rallies are off (the master switch, his data, or one of the fallen come back)");
		return false;
	}
	if (State != EBDRallyState::Walking)
	{
		OutWhy = FString::Printf(TEXT("he is already in a rally (%s)"), StateName(State));
		return false;
	}
	NextRallyIn = 0.0f;
	return true;
}

void UBDRallyComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ABDCandidate* Candidate = GetCandidate();
	if (!bEnabled || Candidate == nullptr || Candidate->HasArrived() || Candidate->GetCurrentHealth() <= 0.0f)
	{
		return;
	}

	switch (State)
	{
	case EBDRallyState::Walking:
	{
		NextRallyIn = FMath::Max(0.0f, NextRallyIn - DeltaTime);
		if (NextRallyIn > 0.0f)
		{
			break;
		}
		// One rally on the board at a time: due while another candidate holds it, he waits.
		UBDCandidateSubsystem* Candidates = UBDCandidateSubsystem::Get(this);
		if (Candidates != nullptr && Candidates->TryClaimRally(*Candidate))
		{
			bHoldsTurn = true;
			EnterTell();
		}
		break;
	}
	case EBDRallyState::Tell:
		StateRemaining -= DeltaTime;
		if (StateRemaining <= 0.0f)
		{
			EnterSpeaking();
		}
		break;
	case EBDRallyState::Speaking:
		StateRemaining -= DeltaTime;
		UpdateAura(DeltaTime);
		if (StateRemaining <= 0.0f)
		{
			EnterSpawning();
		}
		break;
	case EBDRallyState::Spawning:
		StateRemaining -= DeltaTime;
		if (StateRemaining <= 0.0f)
		{
			EnterWalking();
		}
		break;
	}
}

void UBDRallyComponent::EnterTell()
{
	ABDCandidate* Candidate = GetCandidate();
	State = EBDRallyState::Tell;
	StateRemaining = FMath::Max(0.0f, Params.RallyTellSeconds);
	++RalliesStarted;

	// The warning: the red exclamation over him for the whole of it, and a short sound.
	if (UBDSpeechMarkSubsystem* Marks = UBDSpeechMarkSubsystem::Get(this))
	{
		Marks->NoteTimedMark(*Candidate, EBDSpeakerSide::Opponent, FMath::Max(0.1f, StateRemaining));
	}
	if (USoundBase* Tell = UBDGameBalanceSettings::Get().RallyTellSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Tell, Candidate->GetActorLocation());
	}
	if (UBDCandidateSubsystem* Candidates = UBDCandidateSubsystem::Get(this))
	{
		Candidates->NotifyRallyStarted(*Candidate);
	}
	UE_LOG(LogBDCandidate, Log, TEXT("RALLY: candidate %d warns (%.1fs), rally %d; %d militant(s) to rise."),
		Candidate->Ordinal, StateRemaining, RalliesStarted, Params.RallyCount);
}

void UBDRallyComponent::EnterSpeaking()
{
	ABDCandidate* Candidate = GetCandidate();
	State = EBDRallyState::Speaking;

	// His words, the only ones on the board until he is done. No rally cue of his own yet:
	// what he says walking stands in, and the length is the rally's.
	USoundBase* Cue = Params.RallySpeech.LoadSynchronous();
	USoundBase* Words = Cue;
	if (Words == nullptr && Candidate->GetData() != nullptr)
	{
		Words = Candidate->GetData()->SpeechSound.LoadSynchronous();
	}
	const float CueSeconds = Cue != nullptr ? Cue->GetDuration() : 0.0f;
	SpeechSeconds = FMath::Max(Params.RallyMinDuration,
		CueSeconds > 0.0f && CueSeconds < INDEFINITELY_LOOPING_DURATION ? CueSeconds : Params.RallyDuration);
	StateRemaining = SpeechSeconds;

	if (UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(this))
	{
		Sounds->TakeFloor(*Candidate);
		bHoldsFloor = true;
		SpeechAudio = Sounds->SayOnFloor(*Candidate, Words);
	}
	ShowAura(true);
	UE_LOG(LogBDCandidate, Log, TEXT("RALLY: candidate %d speaks for %.1fs (%s): untouchable, out of the Mito's sight, the only voice on the board."),
		Candidate->Ordinal, SpeechSeconds, Cue != nullptr ? *Cue->GetName() : (Words != nullptr ? *FString::Printf(TEXT("%s standing in"), *Words->GetName()) : TEXT("no words yet")));
}

void UBDRallyComponent::EnterSpawning()
{
	ShowAura(false);
	if (UAudioComponent* Audio = SpeechAudio.Get())
	{
		Audio->Stop();
	}
	SpeechAudio.Reset();
	if (bHoldsFloor)
	{
		if (UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(this))
		{
			Sounds->GiveBackFloor(*GetOwner());
		}
		bHoldsFloor = false;
	}

	State = EBDRallyState::Spawning;
	StateRemaining = FMath::Max(0.0f, UBDGameBalanceSettings::Get().RallyRiseSeconds);
	RaiseCrowd();
}

void UBDRallyComponent::EnterWalking()
{
	if (bHoldsTurn)
	{
		if (UBDCandidateSubsystem* Candidates = UBDCandidateSubsystem::Get(this))
		{
			Candidates->ReleaseRally(*GetCandidate());
		}
		bHoldsTurn = false;
	}
	State = EBDRallyState::Walking;
	StateRemaining = 0.0f;
	NextRallyIn = DrawInterval();
	UE_LOG(LogBDCandidate, Log, TEXT("RALLY: candidate %d walks on; the next rally in %.1fs."), GetCandidate()->Ordinal, NextRallyIn);
}

void UBDRallyComponent::RaiseCrowd()
{
	ABDCandidate* Candidate = GetCandidate();
	UWorld* World = GetWorld();
	UBDGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
	UBDWaveSubsystem* Waves = World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr;
	LastAsked = FMath::Max(0, Params.RallyCount);
	LastRaised = 0;
	LastCrowd.Reset();
	if (Grid == nullptr || Waves == nullptr || LastAsked == 0)
	{
		return;
	}

	// Who rises, by weight; the creep of the wave when the list gives nobody.
	TArray<const UBDEnemyData*> Kinds;
	TArray<float> Weights;
	float TotalWeight = 0.0f;
	for (const FBDRallyCrowdEntry& Entry : Params.RallyComposition)
	{
		const UBDEnemyData* Kind = Entry.Weight > 0.0f ? Entry.Enemy.LoadSynchronous() : nullptr;
		if (Kind != nullptr)
		{
			Kind->PreloadSkins();
			Kinds.Add(Kind);
			Weights.Add(Entry.Weight);
			TotalWeight += Entry.Weight;
		}
	}
	if (Kinds.Num() == 0)
	{
		if (const UBDEnemyData* WaveCreep = UBDWaveSettings::Get().ResolveWaveEnemy())
		{
			Kinds.Add(WaveCreep);
			Weights.Add(1.0f);
			TotalWeight = 1.0f;
		}
	}
	if (Kinds.Num() == 0)
	{
		UE_LOG(LogBDCandidate, Warning, TEXT("RALLY: candidate %d has nobody to raise: no kind in his RallyComposition loads and no wave enemy is set."), Candidate->Ordinal);
		return;
	}

	// Where: ring by ring outwards over the walkable cells, so the crowd fills the corridor
	// both ways along the route. A cell takes one of them, and only one that is free
	// ground - never a platform, the palace or the scenery, which are not walkable, nor the
	// urn itself - and that leads to the urn, which SpawnEnemyAtCell checks.
	FBDCellCoord Start;
	if (!Grid->WorldToCell(Candidate->GetActorLocation(), Start))
	{
		Start = Candidate->GetHeadingCell();
	}
	if (!Grid->IsValidCoord(Start))
	{
		UE_LOG(LogBDCandidate, Warning, TEXT("RALLY: candidate %d stands off the board at %s; nobody rises."), Candidate->Ordinal, *Start.ToString());
		return;
	}
	const int32 SizeX = Grid->GetSizeX();
	const int32 MaxSteps = FMath::Max(1, UBDGameBalanceSettings::Get().RallyMaxSteps);
	TArray<int32> Steps;
	Steps.Init(INDEX_NONE, SizeX * Grid->GetSizeY());
	TArray<FBDCellCoord> Queue;
	Queue.Add(Start);
	Steps[Start.Y * SizeX + Start.X] = 0;
	static const FBDCellCoord Offsets[] = { FBDCellCoord(1, 0), FBDCellCoord(-1, 0), FBDCellCoord(0, 1), FBDCellCoord(0, -1) };
	int32 FarthestStep = 0;
	for (int32 Head = 0; Head < Queue.Num() && LastRaised < LastAsked; ++Head)
	{
		const FBDCellCoord Cell = Queue[Head];
		const int32 Step = Steps[Cell.Y * SizeX + Cell.X];
		const EBDCellState CellState = Grid->GetCellState(Cell);
		const bool bGround = CellState == EBDCellState::Free || CellState == EBDCellState::Tower || CellState == EBDCellState::Spawn;
		if (Step > 0 && bGround)
		{
			// The kind drawn by weight off his own stream: the same match raises the same crowd.
			float Roll = Stream.FRandRange(0.0f, TotalWeight);
			int32 Pick = 0;
			for (; Pick < Kinds.Num() - 1 && Roll > Weights[Pick]; ++Pick)
			{
				Roll -= Weights[Pick];
			}
			if (ABDEnemyBase* Raised = Waves->SpawnEnemyAtCell(Kinds[Pick], Cell, TEXT("a rally")))
			{
				Raised->BeginRise(UBDGameBalanceSettings::Get().RallyRiseSeconds);
				LastCrowd.Add(Raised);
				++LastRaised;
				FarthestStep = Step;
			}
		}
		if (Step >= MaxSteps)
		{
			continue;
		}
		for (const FBDCellCoord& Offset : Offsets)
		{
			const FBDCellCoord Next(Cell.X + Offset.X, Cell.Y + Offset.Y);
			if (!Grid->IsValidCoord(Next) || Steps[Next.Y * SizeX + Next.X] != INDEX_NONE
				|| !Grid->IsWalkable(Next) || Grid->IsEdgeBlocked(Cell, Next))
			{
				continue;
			}
			Steps[Next.Y * SizeX + Next.X] = Step + 1;
			Queue.Add(Next);
		}
	}

	UE_LOG(LogBDCandidate, Log, TEXT("RALLY: candidate %d raised %d of %d militant(s) around %s, out to %d step(s)%s."),
		Candidate->Ordinal, LastRaised, LastAsked, *Start.ToString(), FarthestStep,
		LastRaised < LastAsked ? *FString::Printf(TEXT(" - no room for %d within %d steps"), LastAsked - LastRaised, MaxSteps) : TEXT(""));
}

void UBDRallyComponent::ShowAura(const bool bShow)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}
	if (bShow && Aura == nullptr)
	{
		const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
		UStaticMesh* Disc = Balance.RallyAuraMesh.LoadSynchronous();
		UMaterialInterface* Material = Balance.RallyAuraMaterial.LoadSynchronous();
		if (Disc == nullptr || Material == nullptr)
		{
			UE_LOG(LogBDCandidate, Warning, TEXT("RALLY: no aura to show (Rally Aura Mesh / Material in Project Settings > Brazil Defense - Balance)."));
			return;
		}
		Aura = NewObject<UStaticMeshComponent>(Owner, TEXT("RallyAura"));
		Aura->SetupAttachment(Owner->GetRootComponent());
		Aura->SetStaticMesh(Disc);
		Aura->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Aura->SetGenerateOverlapEvents(false);
		Aura->SetCanEverAffectNavigation(false);
		Aura->SetCastShadow(false);
		Aura->SetUsingAbsoluteRotation(true);
		AuraMaterial = UMaterialInstanceDynamic::Create(Material, this);
		Aura->SetMaterial(0, AuraMaterial);
		const UWorld* World = GetWorld();
		const UBDGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
		const float Diameter = 2.0f * Balance.RallyAuraRadiusCells * (Grid != nullptr ? Grid->GetCellSize() : 100.0f);
		const FBoxSphereBounds Bounds = Disc->GetBounds();
		const float Across = FMath::Max(1.0f, 2.0f * FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y));
		const float Tall = FMath::Max(1.0f, 2.0f * Bounds.BoxExtent.Z);
		// Flat on the floor, a couple of centimetres thick, its bottom just over the ground.
		Aura->SetRelativeScale3D(FVector(Diameter / Across, Diameter / Across, 2.0f / Tall));
		Aura->SetRelativeLocation(FVector(0.0f, 0.0f, 2.0f - Bounds.Origin.Z * 2.0f / Tall));
		Aura->RegisterComponent();
	}
	if (Aura != nullptr)
	{
		AuraClock = 0.0f;
		UpdateAura(0.0f);
		Aura->SetVisibility(bShow);
	}
}

void UBDRallyComponent::UpdateAura(const float DeltaTime)
{
	if (AuraMaterial == nullptr)
	{
		return;
	}
	// On the game's clock, from bright: one pulse a second, never quite out.
	AuraClock += DeltaTime;
	const float Hz = UBDGameBalanceSettings::Get().RallyAuraPulseHz;
	const float Pulse = 0.55f + 0.45f * FMath::Cos(UE_TWO_PI * Hz * AuraClock);
	AuraMaterial->SetScalarParameterValue(TEXT("Pulse"), Pulse);
}

void UBDRallyComponent::ReleaseAll()
{
	ShowAura(false);
	if (UAudioComponent* Audio = SpeechAudio.Get())
	{
		Audio->Stop();
	}
	SpeechAudio.Reset();
	AActor* Owner = GetOwner();
	if (bHoldsFloor && Owner != nullptr)
	{
		if (UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(this))
		{
			Sounds->GiveBackFloor(*Owner);
		}
	}
	bHoldsFloor = false;
	if (bHoldsTurn && GetCandidate() != nullptr)
	{
		if (UBDCandidateSubsystem* Candidates = UBDCandidateSubsystem::Get(this))
		{
			Candidates->ReleaseRally(*GetCandidate());
		}
	}
	bHoldsTurn = false;
}

void UBDRallyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Down in the middle of a rally (killed in the warning, or the board wiped): the turn,
	// the floor and the disc are given back at once.
	ReleaseAll();
	Super::EndPlay(EndPlayReason);
}

// Brazil Defense. The bribe: what a dead candidate drops, and its way to the mint.

#include "Bribe/BDBribeSubsystem.h"

#include "BDLog.h"
#include "Bribe/BDBribeSettings.h"
#include "Bribe/BDMoneyBag.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Match/BDMatchManager.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "Stats/Stats.h"
#include "UI/BDUISettings.h"

void UBDBribeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UBDBribeSubsystem::Deinitialize()
{
	if (ABDMatchManager* Match = BoundMatch.Get())
	{
		Match->OnPhaseChanged.Remove(PhaseChangedHandle);
	}
	BoundMatch.Reset();

	if (ABDMoneyBag* Falling = Bag.Get())
	{
		Falling->Destroy();
	}
	Bag.Reset();
	Queue.Reset();
	Stage = EBDBribeStage::Idle;

	Super::Deinitialize();
}

TStatId UBDBribeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBDBribeSubsystem, STATGROUP_Tickables);
}

UBDBribeSubsystem* UBDBribeSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World != nullptr ? World->GetSubsystem<UBDBribeSubsystem>() : nullptr;
}

ABDMatchManager* UBDBribeSubsystem::GetMatch() const
{
	return ABDMatchManager::Get(GetWorld());
}

void UBDBribeSubsystem::EnsureMatchBinding()
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

	PhaseChangedHandle = Match->OnPhaseChanged.AddUObject(this, &UBDBribeSubsystem::HandlePhaseChanged);
	BoundMatch = Match;
}

void UBDBribeSubsystem::HandlePhaseChanged(const EBDMatchPhase NewPhase)
{
	// The match ended, or was rewound to its start from the console or a load. Either
	// way nothing of the running conversion belongs to what comes next: the match
	// manager has already dropped the counters, and the show follows them out.
	const ABDMatchManager* Match = GetMatch();
	const bool bRewound = NewPhase == EBDMatchPhase::Building && Match != nullptr && Match->GetCurrentWave() == 0;
	const bool bOver = NewPhase == EBDMatchPhase::Defeat || NewPhase == EBDMatchPhase::Victory;
	if (bRewound || bOver)
	{
		ResetForNewMatch(bOver ? TEXT("the match is over") : TEXT("the match was rewound"));
	}
}

void UBDBribeSubsystem::ResetForNewMatch(const TCHAR* Why)
{
	if (Stage == EBDBribeStage::Idle && Queue.Num() == 0)
	{
		return;
	}

	UE_LOG(LogBDBribe, Log, TEXT("Conversion dropped (%s): %d of candidate %d's bribe unconverted, %d more queued."),
		Why, FMath::Max(0, Current.Amount - Transferred), Current.Ordinal, Queue.Num());

	if (ABDMoneyBag* Falling = Bag.Get())
	{
		Falling->Destroy();
	}
	Bag.Reset();
	Queue.Reset();
	Stage = EBDBribeStage::Idle;
	Current = FBDPendingBribe();
	StageElapsed = 0.0f;
	Counted = 0;
	Transferred = 0;
}

//~ Collecting ------------------------------------------------------------------

void UBDBribeSubsystem::Collect(const int32 Amount, const FVector& Where, const int32 Ordinal)
{
	if (Amount <= 0)
	{
		return;
	}

	FBDPendingBribe Pending;
	Pending.Amount = Amount;
	Pending.Where = Where;
	Pending.Ordinal = Ordinal;
	Queue.Add(Pending);

	UE_LOG(LogBDBribe, Log, TEXT("BRIBE DROPPED by candidate %d: %d, waiting to be counted (%d conversion(s) queued)."),
		Ordinal, Amount, Queue.Num());
}

float UBDBribeSubsystem::GetConversionProgress() const
{
	if (Stage == EBDBribeStage::Idle || Current.Amount <= 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(static_cast<float>(Transferred) / Current.Amount, 0.0f, 1.0f);
}

void UBDBribeSubsystem::BeginNext()
{
	Current = Queue[0];
	Queue.RemoveAt(0);
	StageElapsed = 0.0f;
	Counted = 0;
	Transferred = 0;
	Stage = EBDBribeStage::Bag;

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// Over the body, and falling to where the body stands: the bag belongs to the spot
	// the candidate died on, not to the camera.
	const UBDBribeSettings& Settings = UBDBribeSettings::Get();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Start = Current.Where + FVector(0.0f, 0.0f, Settings.DropHeight);
	if (ABDMoneyBag* Spawned = World->SpawnActor<ABDMoneyBag>(ABDMoneyBag::StaticClass(), Start, FRotator::ZeroRotator, Params))
	{
		Spawned->Drop(Current.Where.Z);
		Bag = Spawned;
	}
}

//~ Tick ------------------------------------------------------------------------

void UBDBribeSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	EnsureMatchBinding();

	ABDMatchManager* Match = GetMatch();
	if (Match == nullptr || Match->IsMatchOver())
	{
		return;
	}

	if (Stage == EBDBribeStage::Idle)
	{
		if (Queue.Num() > 0)
		{
			BeginNext();
		}
		return;
	}

	const UBDBribeSettings& Settings = UBDBribeSettings::Get();
	StageElapsed += DeltaTime;

	switch (Stage)
	{
	case EBDBribeStage::Bag:
	{
		// The bag has the stage to itself: it rings its own coins while it bounces.
		if (StageElapsed >= FMath::Max(0.1f, Settings.BagSeconds))
		{
			Stage = EBDBribeStage::Counting;
			StageElapsed = 0.0f;
		}
		break;
	}
	case EBDBribeStage::Counting:
	{
		const float Duration = FMath::Max(0.0f, Settings.CountSeconds);
		const float Alpha = Duration > 0.0f ? FMath::Clamp(StageElapsed / Duration, 0.0f, 1.0f) : 1.0f;
		AdvanceCounting(Alpha);
		if (Alpha >= 1.0f)
		{
			// The count is in: the till rings, and the money is about to stop being the
			// thief's.
			if (USoundBase* Till = Settings.CashRegisterSound.LoadSynchronous())
			{
				PlaySoundAt(Till, Current.Where, Settings.CashRegisterVolume, 1.0f);
			}
			UE_LOG(LogBDBribe, Log, TEXT("BRIBE COUNTED from candidate %d: %d recovered, now on its way to the mint."),
				Current.Ordinal, Current.Amount);
			Stage = EBDBribeStage::Register;
			StageElapsed = 0.0f;
		}
		break;
	}
	case EBDBribeStage::Register:
	{
		if (StageElapsed >= FMath::Max(0.0f, Settings.RegisterSeconds))
		{
			Stage = EBDBribeStage::Transfer;
			StageElapsed = 0.0f;
		}
		break;
	}
	case EBDBribeStage::Transfer:
	{
		const float Duration = FMath::Max(0.0f, Settings.TransferSeconds);
		const float Alpha = Duration > 0.0f ? FMath::Clamp(StageElapsed / Duration, 0.0f, 1.0f) : 1.0f;
		AdvanceTransfer(Alpha);
		if (Alpha >= 1.0f)
		{
			UE_LOG(LogBDBribe, Log, TEXT("PUBLIC MONEY from candidate %d: +%d minted, %d bribe held, %d public money to spend on evolution."),
				Current.Ordinal, Transferred, Match->GetBribeHeld(), Match->GetPublicMoney());
			Stage = EBDBribeStage::Idle;
			Current = FBDPendingBribe();
			StageElapsed = 0.0f;
		}
		break;
	}
	default:
		break;
	}
}

void UBDBribeSubsystem::AdvanceCounting(const float Alpha)
{
	ABDMatchManager* Match = GetMatch();
	if (Match == nullptr)
	{
		return;
	}

	const int32 Target = FMath::RoundToInt(Current.Amount * Alpha);
	const int32 Step = Target - Counted;
	if (Step <= 0)
	{
		return;
	}

	Match->AddBribe(Step, FString::Printf(TEXT("candidate %d"), Current.Ordinal));
	Counted = Target;
}

void UBDBribeSubsystem::AdvanceTransfer(const float Alpha)
{
	ABDMatchManager* Match = GetMatch();
	if (Match == nullptr)
	{
		return;
	}

	const int32 Target = FMath::RoundToInt(Current.Amount * Alpha);
	const int32 Step = Target - Transferred;
	if (Step <= 0)
	{
		return;
	}

	Transferred += Match->ConvertBribe(Step);
}

void UBDBribeSubsystem::FlushNow(const TCHAR* Why)
{
	ABDMatchManager* Match = GetMatch();
	if (Match == nullptr || !IsConverting())
	{
		return;
	}

	// Whatever stage it was in, the whole of the running conversion and everything
	// behind it lands at once. The ledger ends where the show would have left it.
	if (Stage != EBDBribeStage::Idle)
	{
		AdvanceCounting(1.0f);
		AdvanceTransfer(1.0f);
	}
	for (const FBDPendingBribe& Pending : Queue)
	{
		Match->AddBribe(Pending.Amount, FString::Printf(TEXT("candidate %d, flushed"), Pending.Ordinal));
		Match->ConvertBribe(Pending.Amount);
	}

	UE_LOG(LogBDBribe, Log, TEXT("Conversions flushed (%s): %d bribe held, %d public money."),
		Why, Match->GetBribeHeld(), Match->GetPublicMoney());

	if (ABDMoneyBag* Falling = Bag.Get())
	{
		Falling->Destroy();
	}
	Bag.Reset();
	Queue.Reset();
	Stage = EBDBribeStage::Idle;
	Current = FBDPendingBribe();
	StageElapsed = 0.0f;
	Counted = 0;
	Transferred = 0;
}

//~ Sound -------------------------------------------------------------------------

void UBDBribeSubsystem::PlaySoundAt(USoundBase* Sound, const FVector& Where, const float Volume, const float Pitch)
{
	UWorld* World = GetWorld();
	if (Sound == nullptr || World == nullptr)
	{
		return;
	}

	const UBDBribeSettings& Settings = UBDBribeSettings::Get();
	if (Concurrency == nullptr)
	{
		Concurrency = NewObject<USoundConcurrency>(this, TEXT("BribeConcurrency"));
		Concurrency->Concurrency.MaxCount = FMath::Clamp(Settings.SoundMaxConcurrent, 1, 16);
		Concurrency->Concurrency.bLimitToOwner = false;
		Concurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
	}

	// The same falloff the urn's beep uses: the listener is the match camera, so the
	// radii are wide and the coins read from the overview without drowning it.
	FSoundAttenuationSettings Attenuation;
	Attenuation.bAttenuate = true;
	Attenuation.bSpatialize = true;
	Attenuation.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_Default;
	Attenuation.StereoSpread = 400.0f;
	Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Attenuation.dBAttenuationAtMax = Settings.SoundAttenuationAtMax;
	Attenuation.AttenuationShapeExtents = FVector(Settings.SoundInnerRadius, 0.0f, 0.0f);
	Attenuation.FalloffDistance = Settings.SoundFalloffDistance;
	Attenuation.FalloffMode = ENaturalSoundFalloffMode::Hold;
	Attenuation.bEnableListenerFocus = false;
	Attenuation.bAttenuateWithLPF = true;
	Attenuation.bEnableLogFrequencyScaling = true;
	Attenuation.LPFRadiusMin = Settings.SoundInnerRadius;
	Attenuation.LPFRadiusMax = Settings.SoundInnerRadius + Settings.SoundFalloffDistance;
	Attenuation.LPFFrequencyAtMax = 3000.0f;
	Attenuation.bEnableOcclusion = false;

	UAudioComponent* Audio = UGameplayStatics::SpawnSoundAtLocation(World, Sound, Where, FRotator::ZeroRotator,
		Volume, Pitch, 0.0f, nullptr, Concurrency, /*bAutoDestroy*/ true);
	if (Audio == nullptr)
	{
		return;
	}

	// Through the effects class, so the options slider and mute apply to it.
	if (USoundClass* Effects = UBDUISettings::Get().EffectsSoundClass.LoadSynchronous())
	{
		Audio->SoundClassOverride = Effects;
	}
	Audio->bOverrideAttenuation = true;
	Audio->AttenuationOverrides = Attenuation;
	Audio->Play();
}

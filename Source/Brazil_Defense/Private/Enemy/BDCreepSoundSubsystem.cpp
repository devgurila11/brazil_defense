// Brazil Defense. Plays what the creeps say and the steps they take, under one budget.

#include "Enemy/BDCreepSoundSubsystem.h"

#include "AudioDevice.h"
#include "BDLog.h"
#include "Components/AudioComponent.h"
#include "Enemy/BDCreepSoundSettings.h"
#include "Enemy/BDEnemyBase.h"
#include "Enemy/BDEnemyData.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"

namespace BDCreepSoundPrivate
{
	/**
	 * The falloff the urn's beep and the coins use, with radii of its own: the listener is
	 * the match camera, so the sphere is wide, and a low pass closes in with the distance
	 * so a far creep is a murmur rather than a quiet copy of a near one.
	 */
	static FSoundAttenuationSettings MakeAttenuation(const float InnerRadius, const float FalloffDistance, const float AttenuationAtMax)
	{
		FSoundAttenuationSettings Attenuation;
		Attenuation.bAttenuate = true;
		Attenuation.bSpatialize = true;
		Attenuation.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_Default;
		Attenuation.StereoSpread = 400.0f;
		Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		Attenuation.dBAttenuationAtMax = AttenuationAtMax;
		Attenuation.AttenuationShapeExtents = FVector(InnerRadius, 0.0f, 0.0f);
		Attenuation.FalloffDistance = FalloffDistance;
		// Silent past the falloff, not held at the floor: a creep out of range must not
		// take a place in the budget from one the camera is looking at.
		Attenuation.FalloffMode = ENaturalSoundFalloffMode::Silent;
		Attenuation.bEnableListenerFocus = false;
		Attenuation.bAttenuateWithLPF = true;
		Attenuation.bEnableLogFrequencyScaling = true;
		Attenuation.LPFRadiusMin = InnerRadius;
		Attenuation.LPFRadiusMax = InnerRadius + FalloffDistance;
		Attenuation.LPFFrequencyAtMax = 3000.0f;
		Attenuation.bEnableOcclusion = false;
		return Attenuation;
	}

	static USoundConcurrency* MakeConcurrency(UObject* Outer, const TCHAR* Name, const int32 MaxCount, const EMaxConcurrentResolutionRule::Type Rule)
	{
		USoundConcurrency* Concurrency = NewObject<USoundConcurrency>(Outer, Name);
		Concurrency->Concurrency.MaxCount = MaxCount;
		Concurrency->Concurrency.bLimitToOwner = false;
		Concurrency->Concurrency.ResolutionRule = Rule;
		return Concurrency;
	}

	/**
	 * Whether a sound at Where would reach the listener at all, with the range of its
	 * attenuation. Asked on the game thread before anything is spawned: with a board full
	 * of creeps most of them are out of range, and a step or a word nobody can hear should
	 * cost nothing. True when there is no listener to ask, so nothing is lost by accident.
	 */
	static bool InRange(const UWorld& World, const FVector& Where, const float Range)
	{
		const APlayerController* Player = World.GetFirstPlayerController();
		if (Player == nullptr)
		{
			return true;
		}
		FVector Listener;
		FVector Front;
		FVector Right;
		Player->GetAudioListenerPosition(Listener, Front, Right);
		return FVector::DistSquared(Listener, Where) <= FMath::Square(Range);
	}

	static USoundBase* Resolve(const TSoftObjectPtr<USoundBase>& Sound)
	{
		if (Sound.IsNull())
		{
			return nullptr;
		}
		USoundBase* Loaded = Sound.Get();
		return Loaded != nullptr ? Loaded : Sound.LoadSynchronous();
	}
}

UBDCreepSoundSubsystem* UBDCreepSoundSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World != nullptr ? World->GetSubsystem<UBDCreepSoundSubsystem>() : nullptr;
}

bool UBDCreepSoundSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UBDCreepSoundSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBDCreepSoundSubsystem, STATGROUP_Tickables);
}

void UBDCreepSoundSubsystem::Tick(const float DeltaTime)
{
	if (VoicesRequested == 0)
	{
		return;
	}
	// Real seconds: the question is what the ear gets, whatever the game speed.
	const int32 Count = CountActiveVoices();
	PeakVoices = FMath::Max(PeakVoices, Count);
	if (SecondsAtCount.Num() <= Count)
	{
		SecondsAtCount.SetNumZeroed(Count + 1);
	}
	const UWorld* World = GetWorld();
	SecondsAtCount[Count] += World != nullptr ? World->GetDeltaSeconds() / FMath::Max(KINDA_SMALL_NUMBER, World->GetWorldSettings()->GetEffectiveTimeDilation()) : DeltaTime;
}

void UBDCreepSoundSubsystem::EnsureObjects()
{
	if (VoiceConcurrency != nullptr)
	{
		return;
	}

	const UBDCreepSoundSettings& Settings = UBDCreepSoundSettings::Get();
	// A full voice budget gives way farthest first, so the creep the camera is on keeps its
	// voice over one across the board; among equals the oldest goes. Steps are too short
	// for it to matter and simply drop the oldest.
	VoiceConcurrency = BDCreepSoundPrivate::MakeConcurrency(this, TEXT("CreepVoiceConcurrency"),
		FMath::Clamp(Settings.VoiceMaxConcurrent, 1, 16), EMaxConcurrentResolutionRule::StopFarthestThenOldest);
	FootstepConcurrency = BDCreepSoundPrivate::MakeConcurrency(this, TEXT("CreepFootstepConcurrency"),
		FMath::Clamp(Settings.FootstepMaxConcurrent, 1, 32), EMaxConcurrentResolutionRule::StopOldest);

	VoiceAttenuation = NewObject<USoundAttenuation>(this, TEXT("CreepVoiceAttenuation"));
	VoiceAttenuation->Attenuation = BDCreepSoundPrivate::MakeAttenuation(
		Settings.VoiceInnerRadius, Settings.VoiceFalloffDistance, Settings.VoiceAttenuationAtMax);

	FootstepAttenuation = NewObject<USoundAttenuation>(this, TEXT("CreepFootstepAttenuation"));
	FootstepAttenuation->Attenuation = BDCreepSoundPrivate::MakeAttenuation(
		Settings.FootstepInnerRadius, Settings.FootstepFalloffDistance, Settings.FootstepAttenuationAtMax);
}

bool UBDCreepSoundSubsystem::PlayVocal(ABDEnemyBase& Creep)
{
	const UBDEnemyData* Data = Creep.GetData();
	if (Data == nullptr || !Data->IsVocal())
	{
		return false;
	}

	// Words or the call, as the data shares them; whichever one the data lacks, the other.
	USoundBase* Speech = BDCreepSoundPrivate::Resolve(Data->SpeechSound);
	USoundBase* Call = BDCreepSoundPrivate::Resolve(Data->CallSound);
	const bool bSpeak = Speech != nullptr && (Call == nullptr || FMath::FRand() < Data->SpeechShare);
	USoundBase* Sound = bSpeak ? Speech : Call;
	if (Sound == nullptr)
	{
		return false;
	}

	++VoicesRequested;
	const UBDCreepSoundSettings& Settings = UBDCreepSoundSettings::Get();
	if (!BDCreepSoundPrivate::InRange(*Creep.GetWorld(), Creep.GetActorLocation(), Settings.VoiceInnerRadius + Settings.VoiceFalloffDistance))
	{
		return false;
	}
	EnsureObjects();
	++VoicesInRange;

	// Attached, so the sentence leaves with the donkey; left to finish if the creep dies
	// mid word, or every kill would cut a voice off.
	const float Pitch = bSpeak ? Data->VoicePitch : 1.0f;
	UAudioComponent* Audio = UGameplayStatics::SpawnSoundAttached(Sound, Creep.GetRootComponent(), NAME_None,
		FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, /*bStopWhenAttachedToDestroyed*/ false,
		Settings.VoiceVolume, Pitch, 0.0f, VoiceAttenuation, VoiceConcurrency, /*bAutoDestroy*/ true);
	if (Audio == nullptr)
	{
		return false;
	}

	++VoicesStarted;
	Voices.Add(Audio);
	return true;
}

bool UBDCreepSoundSubsystem::PlayFootstep(const ABDEnemyBase& Creep)
{
	const UBDEnemyData* Data = Creep.GetData();
	USoundBase* Sound = Data != nullptr ? BDCreepSoundPrivate::Resolve(Data->FootstepSound) : nullptr;
	UWorld* World = GetWorld();
	if (Sound == nullptr || World == nullptr)
	{
		return false;
	}

	++StepsRequested;
	const UBDCreepSoundSettings& Settings = UBDCreepSoundSettings::Get();
	if (!BDCreepSoundPrivate::InRange(*World, Creep.GetActorLocation(), Settings.FootstepInnerRadius + Settings.FootstepFalloffDistance))
	{
		return false;
	}
	EnsureObjects();
	++StepsPlayed;
	UGameplayStatics::PlaySoundAtLocation(World, Sound, Creep.GetActorLocation(), FRotator::ZeroRotator,
		Settings.FootstepVolume, 1.0f, 0.0f, FootstepAttenuation, FootstepConcurrency, &Creep);
	return true;
}

int32 UBDCreepSoundSubsystem::CountActiveVoices()
{
	Voices.RemoveAll([](const TWeakObjectPtr<UAudioComponent>& Voice)
	{
		return !Voice.IsValid() || !Voice->IsPlaying();
	});
	return Voices.Num();
}

void UBDCreepSoundSubsystem::ResetStats()
{
	PeakVoices = CountActiveVoices();
	SecondsAtCount.Reset();
	VoicesRequested = 0;
	VoicesInRange = 0;
	VoicesStarted = 0;
	StepsRequested = 0;
	StepsPlayed = 0;
}

//~ Console -------------------------------------------------------------------------

namespace BDCreepSoundDebug
{
	static void ExecStats(const TArray<FString>& Args, UWorld* World)
	{
		UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(World);
		if (Sounds == nullptr)
		{
			UE_LOG(LogBDWave, Error, TEXT("BD.Sound.Stats needs a game world."));
			return;
		}

		UE_LOG(LogBDWave, Display, TEXT("Creep sound: %d voices now, peak %d together (budget %d). Voices: %d due, %d in range, %d started. Steps: %d due, %d in range."),
			Sounds->CountActiveVoices(), Sounds->GetPeakVoices(), UBDCreepSoundSettings::Get().VoiceMaxConcurrent,
			Sounds->GetVoicesRequested(), Sounds->GetVoicesInRange(), Sounds->GetVoicesStarted(),
			Sounds->GetStepsRequested(), Sounds->GetStepsPlayed());

		const TArray<float>& Seconds = Sounds->GetSecondsAtCount();
		float Total = 0.0f;
		for (const float Value : Seconds)
		{
			Total += Value;
		}
		FString Spread;
		for (int32 Count = 0; Count < Seconds.Num(); ++Count)
		{
			Spread += FString::Printf(TEXT(" %d:%.1fs(%.1f%%)"), Count, Seconds[Count], Total > 0.0f ? 100.0f * Seconds[Count] / Total : 0.0f);
		}
		UE_LOG(LogBDWave, Display, TEXT("Creep sound: time with N voices together:%s"), *Spread);

		if (Args.Num() > 0 && Args[0].Equals(TEXT("reset"), ESearchCase::IgnoreCase))
		{
			Sounds->ResetStats();
		}
	}

	/** Silences the output without touching the saved options, so a headless check can run with sound on. */
	static void ExecMute(const TArray<FString>& Args, UWorld* World)
	{
		FAudioDeviceHandle Device = World != nullptr ? World->GetAudioDevice() : FAudioDeviceHandle();
		if (!Device.IsValid())
		{
			UE_LOG(LogBDWave, Warning, TEXT("BD.Sound.Mute: this world has no audio device (-nosound?)."));
			return;
		}
		const bool bMute = Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0;
		Device->SetTransientPrimaryVolume(bMute ? 0.0f : 1.0f);
		UE_LOG(LogBDWave, Display, TEXT("BD.Sound.Mute: output %s."), bMute ? TEXT("muted") : TEXT("restored"));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdStats(
		TEXT("BD.Sound.Stats"),
		TEXT("BD.Sound.Stats [reset]: creep voices sounding now, the peak sounding together and how many were asked for; 'reset' starts the count over after logging."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStats));

	static FAutoConsoleCommandWithWorldAndArgs CmdMute(
		TEXT("BD.Sound.Mute"),
		TEXT("BD.Sound.Mute [1|0]: silences the audio output for this session only (debug, for headless checks with sound). The saved volume options are not touched."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecMute));
}

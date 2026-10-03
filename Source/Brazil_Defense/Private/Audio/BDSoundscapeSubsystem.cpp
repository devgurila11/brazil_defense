// Brazil Defense. The music of the waves and the ambience under everything.

#include "Audio/BDSoundscapeSubsystem.h"

#include "Audio/BDAudioSettings.h"
#include "BDLog.h"
#include "Components/AudioComponent.h"
#include "Day/BDDayCycleComponent.h"
#include "Day/BDDaySettings.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Match/BDMatchManager.h"
#include "Match/BDMatchTypes.h"
#include "Misc/App.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "UI/BDUISettings.h"

namespace BDSoundscapePrivate
{
	/** Seconds a whole ambience layer takes to come in or go, apart from the clip crossfades. */
	static constexpr float AmbienceFade = 2.0f;

	/** How fast a strand's level follows the wanted one, in level per second. */
	static constexpr float GainRate = 0.5f;

	/** How fast the ambience ducks under the music and comes back, in level per second. */
	static constexpr float DuckRate = 0.25f;

	/** Below this a layer is not worth a voice: it stops, and starts again above it. */
	static constexpr float SilentGain = 0.005f;

	/** A clip with no usable length is treated as this long, so a strand never spins. */
	static constexpr float FallbackDuration = 30.0f;

	/** The share of a short clip a crossfade may take at most: a third, so a clip is mostly itself. */
	static constexpr float MaxOverlapShare = 1.0f / 3.0f;

	/** Every clip of every slot, kept loaded for the match so no wave starts on a disk read. */
	static void CollectClips(TArray<FSoftObjectPath>& OutPaths)
	{
		const UBDAudioSettings& Settings = UBDAudioSettings::Get();
		for (const TArray<TSoftObjectPtr<USoundBase>>* List : { &Settings.BattleMusic, &Settings.AmbienceCityDay, &Settings.AmbienceBirdsDay, &Settings.AmbienceNight })
		{
			for (const TSoftObjectPtr<USoundBase>& Clip : *List)
			{
				if (!Clip.IsNull())
				{
					OutPaths.AddUnique(Clip.ToSoftObjectPath());
				}
			}
		}
	}
}

bool UBDSoundscapeSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UBDSoundscapeSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBDSoundscapeSubsystem, STATGROUP_Tickables);
}

UBDSoundscapeSubsystem* UBDSoundscapeSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine != nullptr ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World != nullptr ? World->GetSubsystem<UBDSoundscapeSubsystem>() : nullptr;
}

void UBDSoundscapeSubsystem::Deinitialize()
{
	for (FBDSoundStrand* Strand : { &Music, &City, &Birds, &Night })
	{
		for (UAudioComponent* Voice : Strand->Voices)
		{
			if (Voice != nullptr)
			{
				Voice->Stop();
			}
		}
	}
	Super::Deinitialize();
}

float UBDSoundscapeSubsystem::GetNightAmount() const
{
	const ABDMatchManager* Match = ABDMatchManager::Get(GetWorld());
	const UBDDayCycleComponent* Day = Match != nullptr ? Match->GetDayCycle() : nullptr;
	if (Day == nullptr)
	{
		return 0.0f;
	}

	// Night from the night hour round to sunrise; dark coming on from sunset, going by day.
	const UBDDaySettings& Hours = UBDDaySettings::Get();
	const float Hour = Day->GetHour();
	if (Hour >= Hours.NightHour || Hour < Hours.SunriseHour)
	{
		return 1.0f;
	}
	if (Hour >= Hours.SunsetHour)
	{
		return FMath::SmoothStep(Hours.SunsetHour, Hours.NightHour, Hour);
	}
	if (Hour < Hours.DayHour)
	{
		return 1.0f - FMath::SmoothStep(Hours.SunriseHour, Hours.DayHour, Hour);
	}
	return 0.0f;
}

bool UBDSoundscapeSubsystem::WantsMusic() const
{
	if (ForcedMusic != INDEX_NONE)
	{
		return ForcedMusic != 0;
	}
	const ABDMatchManager* Match = ABDMatchManager::Get(GetWorld());
	return Match != nullptr && !Match->IsMatchOver() && Match->GetPhase() == EBDMatchPhase::WaveActive;
}

void UBDSoundscapeSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	using namespace BDSoundscapePrivate;

	// No match, no soundscape: the menu level has its own.
	UWorld* World = GetWorld();
	if (World == nullptr || ABDMatchManager::Get(World) == nullptr)
	{
		return;
	}

	// The clips of every slot, loaded once in the background as the match starts.
	if (!Preload.IsValid())
	{
		TArray<FSoftObjectPath> Paths;
		CollectClips(Paths);
		if (Paths.Num() > 0)
		{
			Preload = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate(), FStreamableManager::DefaultAsyncLoadPriority, /*bManageActiveHandle*/ true);
		}
	}

	// Real seconds: the soundscape runs through a pause and at the board's speed alike.
	const float Seconds = FMath::Min(static_cast<float>(FApp::GetDeltaTime()), 0.25f);
	const UBDAudioSettings& Settings = UBDAudioSettings::Get();
	const UBDUISettings& UI = UBDUISettings::Get();
	USoundClass* MusicClass = UI.MusicSoundClass.LoadSynchronous();
	USoundClass* EffectsClass = UI.EffectsSoundClass.LoadSynchronous();

	const bool bMusic = WantsMusic() && Settings.BattleMusic.Num() > 0;
	StepStrand(Music, Settings.BattleMusic, bMusic, Settings.MusicVolume, Settings.MusicCrossfade,
		Settings.MusicFadeIn, Settings.MusicFadeOut, MusicClass, Seconds, TEXT("Music"));

	// Present in the pause, under the music in the wave; eased so it follows the fade.
	Duck = FMath::FInterpConstantTo(Duck, bMusic ? Settings.AmbienceUnderMusic : 1.0f, Seconds, DuckRate);

	const float NightAmount = GetNightAmount();
	const float CityGain = Settings.CityVolume * FMath::Lerp(1.0f, Settings.CityAtNight, NightAmount) * Duck;
	const float BirdsGain = Settings.BirdsVolume * (1.0f - NightAmount) * Duck;
	const float NightGain = Settings.NightVolume * NightAmount * Duck;
	StepStrand(City, Settings.AmbienceCityDay, CityGain > SilentGain, CityGain, Settings.AmbienceCrossfade,
		AmbienceFade, AmbienceFade, EffectsClass, Seconds, TEXT("City"));
	StepStrand(Birds, Settings.AmbienceBirdsDay, BirdsGain > SilentGain, BirdsGain, Settings.AmbienceCrossfade,
		AmbienceFade, AmbienceFade, EffectsClass, Seconds, TEXT("Birds"));
	StepStrand(Night, Settings.AmbienceNight, NightGain > SilentGain, NightGain, Settings.AmbienceCrossfade,
		AmbienceFade, AmbienceFade, EffectsClass, Seconds, TEXT("Night"));

	ReportTimer -= Seconds;
	if (ReportTimer <= 0.0f)
	{
		ReportTimer = 10.0f;
		UE_LOG(LogBDAudio, Verbose, TEXT("Soundscape: %s"), *Describe());
	}
}

void UBDSoundscapeSubsystem::StepStrand(FBDSoundStrand& Strand, const TArray<TSoftObjectPtr<USoundBase>>& Clips, const bool bWanted,
	const float WantedGain, const float Crossfade, const float FadeIn, const float FadeOut, USoundClass* SoundClass, const float DeltaSeconds, const TCHAR* Label)
{
	using namespace BDSoundscapePrivate;

	if (!bWanted || Clips.Num() == 0)
	{
		if (Strand.IsPlaying())
		{
			StopStrand(Strand, FadeOut);
			UE_LOG(LogBDAudio, Log, TEXT("%s out over %.1fs."), Label, FadeOut);
		}
		return;
	}

	if (!Strand.IsPlaying())
	{
		// Comes in at the level it is wanted at; the fade does the rest.
		Strand.Gain = WantedGain;
		StartNextClip(Strand, Clips, FadeIn, SoundClass, Label);
	}
	else
	{
		Strand.Elapsed += DeltaSeconds;
		const float Overlap = FMath::Min(Crossfade, Strand.Duration * MaxOverlapShare);
		if (Strand.Elapsed >= Strand.Duration - Overlap)
		{
			// The next clip over the end of this one: the end fades as the start comes in.
			if (UAudioComponent* Leaving = Strand.Voices[Strand.Current])
			{
				Leaving->FadeOut(Overlap, 0.0f);
			}
			StartNextClip(Strand, Clips, Overlap, SoundClass, Label);
		}
	}

	Strand.Gain = FMath::FInterpConstantTo(Strand.Gain, WantedGain, DeltaSeconds, GainRate);
	for (UAudioComponent* Voice : Strand.Voices)
	{
		if (Voice != nullptr)
		{
			Voice->SetVolumeMultiplier(Strand.Gain);
		}
	}
}

bool UBDSoundscapeSubsystem::StartNextClip(FBDSoundStrand& Strand, const TArray<TSoftObjectPtr<USoundBase>>& Clips, const float Fade,
	USoundClass* SoundClass, const TCHAR* Label)
{
	using namespace BDSoundscapePrivate;

	// Any clip but the one just heard; with one clip, that one again.
	int32 Pick = FMath::RandRange(0, Clips.Num() - 1);
	if (Clips.Num() > 1 && Pick == Strand.LastPick)
	{
		Pick = (Pick + FMath::RandRange(1, Clips.Num() - 1)) % Clips.Num();
	}
	USoundBase* Sound = Clips[Pick].LoadSynchronous();
	if (Sound == nullptr)
	{
		UE_LOG(LogBDAudio, Warning, TEXT("%s: slot %d (%s) does not load."), Label, Pick, *Clips[Pick].ToString());
		Strand.Current = INDEX_NONE;
		return false;
	}

	const int32 Voice = Strand.Current == 0 ? 1 : 0;
	TObjectPtr<UAudioComponent>& Component = Strand.Voices[Voice];
	if (Component == nullptr)
	{
		// Kept, not destroyed after each clip: two voices a strand for the whole match. Null
		// without an audio device (-nosound); the strand keeps time all the same.
		Component = UGameplayStatics::CreateSound2D(GetWorld(), Sound, 1.0f, 1.0f, 0.0f, nullptr, /*bPersistAcrossLevelTransition*/ false, /*bAutoDestroy*/ false);
	}
	if (Component != nullptr)
	{
		Component->Stop();
		Component->SetSound(Sound);
		// Heard through a pause: it is the player's soundscape, not the board's.
		Component->bIsUISound = true;
		Component->SoundClassOverride = SoundClass;
		Component->SetVolumeMultiplier(Strand.Gain);
		Component->FadeIn(FMath::Max(Fade, 0.0f), 1.0f);
	}

	const float Length = Sound->GetDuration();
	Strand.Duration = Length > 0.0f && Length < INDEFINITELY_LOOPING_DURATION ? Length : FallbackDuration;
	Strand.Elapsed = 0.0f;
	Strand.Current = Voice;
	Strand.LastPick = Pick;
	Strand.CurrentName = Sound->GetName();
	++Strand.ClipsStarted;

	// Every track is worth a line; the ambience changes clip every second or two.
	UE_CLOG(Strand.Voices == Music.Voices, LogBDAudio, Log, TEXT("Music: %s (%.0fs) in over %.1fs, at %.2f%s."),
		*Strand.CurrentName, Strand.Duration, Fade, Strand.Gain, Component == nullptr ? TEXT(", no audio device") : TEXT(""));
	UE_CLOG(Strand.Voices != Music.Voices, LogBDAudio, Verbose, TEXT("%s: %s (%.1fs) in over %.2fs, at %.2f."),
		Label, *Strand.CurrentName, Strand.Duration, Fade, Strand.Gain);
	return true;
}

void UBDSoundscapeSubsystem::StopStrand(FBDSoundStrand& Strand, const float Fade)
{
	if (Strand.IsPlaying())
	{
		if (UAudioComponent* Voice = Strand.Voices[Strand.Current])
		{
			Voice->FadeOut(FMath::Max(Fade, 0.0f), 0.0f);
		}
	}
	Strand.Current = INDEX_NONE;
	Strand.CurrentName.Reset();
}

FString UBDSoundscapeSubsystem::Describe() const
{
	const auto Line = [](const TCHAR* Label, const FBDSoundStrand& Strand)
	{
		return Strand.IsPlaying()
			? FString::Printf(TEXT("%s %s %.0f/%.0fs at %.2f (%d clip(s))"), Label, *Strand.CurrentName, Strand.Elapsed, Strand.Duration, Strand.Gain, Strand.ClipsStarted)
			: FString::Printf(TEXT("%s silent (%d clip(s))"), Label, Strand.ClipsStarted);
	};
	return FString::Printf(TEXT("%s | %s | %s | %s | night %.2f, duck %.2f"), *Line(TEXT("music"), Music), *Line(TEXT("city"), City),
		*Line(TEXT("birds"), Birds), *Line(TEXT("night"), Night), GetNightAmount(), Duck);
}

namespace BDSoundscapeDebug
{
	static void ExecStatus(const TArray<FString>& Args, UWorld* World)
	{
		const UBDSoundscapeSubsystem* Soundscape = World != nullptr ? World->GetSubsystem<UBDSoundscapeSubsystem>() : nullptr;
		UE_LOG(LogBDAudio, Log, TEXT("BD.Audio.Status: %s"), Soundscape != nullptr ? *Soundscape->Describe() : TEXT("no soundscape in this world"));
	}

	static void ExecMusic(const TArray<FString>& Args, UWorld* World)
	{
		UBDSoundscapeSubsystem* Soundscape = World != nullptr ? World->GetSubsystem<UBDSoundscapeSubsystem>() : nullptr;
		if (Soundscape == nullptr)
		{
			return;
		}
		const int32 Mode = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : INDEX_NONE;
		Soundscape->DebugForceMusic(Mode < 0 ? INDEX_NONE : (Mode != 0 ? 1 : 0));
		UE_LOG(LogBDAudio, Log, TEXT("BD.Audio.Music: %s."), Mode < 0 ? TEXT("back to the phase") : (Mode != 0 ? TEXT("forced on") : TEXT("forced off")));
	}

	static FAutoConsoleCommandWithWorldAndArgs StatusCommand(TEXT("BD.Audio.Status"),
		TEXT("Logs what the music and each ambience layer play, at what level, and the night amount."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));

	static FAutoConsoleCommandWithWorldAndArgs MusicCommand(TEXT("BD.Audio.Music"),
		TEXT("BD.Audio.Music [1|0|-1]: forces the music on or off whatever the phase; -1 (or nothing) gives it back to the phase."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecMusic));
}

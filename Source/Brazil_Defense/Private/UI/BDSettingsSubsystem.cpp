// Brazil Defense. Loads the player's settings, applies them and keeps them saved.

#include "UI/BDSettingsSubsystem.h"

#include "BDLog.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UI/BDLocalization.h"
#include "UI/BDUISettings.h"

const TCHAR* UBDSettingsSave::SlotName = TEXT("BDSettings");

namespace BDSettingsPrivate
{
	static constexpr int32 LowestQuality = 0;
	static constexpr int32 HighestQuality = 3;
	static constexpr int32 VolumeMax = 100;

	static EWindowMode::Type ToEngineWindowMode(const EBDWindowMode Mode)
	{
		switch (Mode)
		{
		case EBDWindowMode::Fullscreen:
			return EWindowMode::Fullscreen;
		case EBDWindowMode::Borderless:
			return EWindowMode::WindowedFullscreen;
		default:
			return EWindowMode::Windowed;
		}
	}
}

void UBDSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Load();
	ApplyGraphics();
	ApplyLanguage();

	// Volumes need a world with an audio device; every game world that comes up gets them.
	WorldInitializedHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(this, &UBDSettingsSubsystem::HandleWorldInitialized);
	if (UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr)
	{
		ApplyAudio(World);
	}
}

void UBDSettingsSubsystem::Deinitialize()
{
	FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitializedHandle);
	Super::Deinitialize();
}

void UBDSettingsSubsystem::Load()
{
	if (UGameplayStatics::DoesSaveGameExist(UBDSettingsSave::SlotName, UBDSettingsSave::UserIndex))
	{
		Settings = Cast<UBDSettingsSave>(UGameplayStatics::LoadGameFromSlot(UBDSettingsSave::SlotName, UBDSettingsSave::UserIndex));
	}

	if (Settings != nullptr)
	{
		UE_LOG(LogBDUI, Log, TEXT("Settings loaded: quality %d, %dx%d %s, vsync %s, music %d, effects %d, muted %s, language %s."),
			Settings->QualityLevel, Settings->Resolution.X, Settings->Resolution.Y,
			*StaticEnum<EBDWindowMode>()->GetNameStringByValue(static_cast<int64>(Settings->WindowMode)),
			Settings->bVSync ? TEXT("on") : TEXT("off"), Settings->MusicVolume, Settings->EffectsVolume,
			Settings->bMuted ? TEXT("yes") : TEXT("no"),
			*StaticEnum<EBDLanguage>()->GetNameStringByValue(static_cast<int64>(Settings->Language)));
		return;
	}

	// First run, or a save that no longer reads: the defaults of the class, written out
	// so the next run finds them.
	Settings = Cast<UBDSettingsSave>(UGameplayStatics::CreateSaveGameObject(UBDSettingsSave::StaticClass()));
	Save();
	UE_LOG(LogBDUI, Log, TEXT("No settings save: defaults written to slot %s."), UBDSettingsSave::SlotName);
}

void UBDSettingsSubsystem::Save()
{
	if (Settings == nullptr)
	{
		return;
	}

	if (!UGameplayStatics::SaveGameToSlot(Settings, UBDSettingsSave::SlotName, UBDSettingsSave::UserIndex))
	{
		UE_LOG(LogBDUI, Error, TEXT("Settings could not be saved to slot %s."), UBDSettingsSave::SlotName);
	}
}

void UBDSettingsSubsystem::ApplyAll()
{
	ApplyGraphics();
	ApplyLanguage();
	if (UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr)
	{
		ApplyAudio(World);
	}
}

//~ Graphics ------------------------------------------------------------------------

void UBDSettingsSubsystem::ApplyGraphics()
{
	UGameUserSettings* UserSettings = GEngine != nullptr ? GEngine->GetGameUserSettings() : nullptr;
	if (UserSettings == nullptr || Settings == nullptr)
	{
		return;
	}

	// Auto leaves scalability where the engine put it; only an explicit pick sets a level.
	if (Settings->QualityLevel >= 0)
	{
		UserSettings->SetOverallScalabilityLevel(FMath::Clamp(Settings->QualityLevel, BDSettingsPrivate::LowestQuality, BDSettingsPrivate::HighestQuality));
	}

	// "Display resolution" means the desktop for a full screen of either kind; a window
	// keeps whatever size it has, since a window the size of the desktop does not fit on it.
	const bool bExplicit = Settings->Resolution.X > 0 && Settings->Resolution.Y > 0;
	const EWindowMode::Type WindowMode = BDSettingsPrivate::ToEngineWindowMode(Settings->WindowMode);
	FIntPoint Resolution = UserSettings->GetScreenResolution();
	if (bExplicit)
	{
		Resolution = Settings->Resolution;
	}
	else if (WindowMode != EWindowMode::Windowed)
	{
		Resolution = UserSettings->GetDesktopResolution();
	}

	// The window belongs to the editor while playing in it: resolution and window mode
	// are saved but only applied to a window of our own. A window with no explicit size
	// is left at whatever the engine or the command line gave it, so the resolution pass
	// is skipped outright: ApplySettings would push the stored size, which defaults to
	// the desktop, and a window that size does not fit on it. Leaving a full screen for
	// such a window is the one case that still needs the pass; it gets three quarters
	// of the desktop.
	const bool bModeChanged = WindowMode != UserSettings->GetLastConfirmedFullscreenMode();
	const bool bApplyResolution = !GIsEditor && (bExplicit || WindowMode != EWindowMode::Windowed || bModeChanged);
	if (bApplyResolution && !bExplicit && WindowMode == EWindowMode::Windowed)
	{
		Resolution = UserSettings->GetDesktopResolution() * 3 / 4;
	}
	if (bApplyResolution)
	{
		UserSettings->SetScreenResolution(Resolution);
		UserSettings->SetFullscreenMode(WindowMode);
	}
	UserSettings->SetVSyncEnabled(Settings->bVSync);

	// Not a check for a confirmation: the panel already showed the player what they picked.
	if (bApplyResolution)
	{
		UserSettings->ApplyResolutionSettings(/*bCheckForCommandLineOverrides*/ false);
	}
	UserSettings->ApplyNonResolutionSettings();
	UserSettings->SaveSettings();

	UE_LOG(LogBDUI, Verbose, TEXT("Graphics applied: quality %d, %dx%d, mode %d, vsync %s."),
		Settings->QualityLevel, Resolution.X, Resolution.Y, static_cast<int32>(Settings->WindowMode), Settings->bVSync ? TEXT("on") : TEXT("off"));
}

void UBDSettingsSubsystem::SetQualityLevel(const int32 Level)
{
	if (Settings == nullptr)
	{
		return;
	}

	Settings->QualityLevel = FMath::Clamp(Level, -1, BDSettingsPrivate::HighestQuality);
	ApplyGraphics();
	Save();
}

void UBDSettingsSubsystem::SetResolution(const FIntPoint Resolution)
{
	if (Settings == nullptr)
	{
		return;
	}

	Settings->Resolution = Resolution;
	ApplyGraphics();
	Save();
}

void UBDSettingsSubsystem::SetWindowMode(const EBDWindowMode Mode)
{
	if (Settings == nullptr)
	{
		return;
	}

	Settings->WindowMode = Mode;
	ApplyGraphics();
	Save();
}

void UBDSettingsSubsystem::SetVSync(const bool bEnabled)
{
	if (Settings == nullptr)
	{
		return;
	}

	Settings->bVSync = bEnabled;
	ApplyGraphics();
	Save();
}

//~ Audio ---------------------------------------------------------------------------

void UBDSettingsSubsystem::HandleWorldInitialized(UWorld* World, const UWorld::InitializationValues Values)
{
	if (World != nullptr && World->IsGameWorld())
	{
		ApplyAudio(World);
	}
}

void UBDSettingsSubsystem::ApplyAudio(UWorld* World)
{
	if (World == nullptr || Settings == nullptr)
	{
		return;
	}

	const UBDUISettings& UISettings = UBDUISettings::Get();
	USoundMix* Mix = UISettings.SettingsSoundMix.LoadSynchronous();
	USoundClass* Master = UISettings.MasterSoundClass.LoadSynchronous();
	USoundClass* Music = UISettings.MusicSoundClass.LoadSynchronous();
	USoundClass* Effects = UISettings.EffectsSoundClass.LoadSynchronous();

	if (Mix == nullptr)
	{
		UE_LOG(LogBDUI, Warning, TEXT("No settings sound mix configured (Project Settings > Brazil Defense - Interface): volumes are saved but drive nothing."));
		return;
	}

	// Mute is the master class at zero; the two sliders keep their values underneath it,
	// so unmuting brings back exactly what was set.
	const float MasterVolume = Settings->bMuted ? 0.0f : 1.0f;
	const float MusicVolume = static_cast<float>(FMath::Clamp(Settings->MusicVolume, 0, BDSettingsPrivate::VolumeMax)) / BDSettingsPrivate::VolumeMax;
	const float EffectsVolume = static_cast<float>(FMath::Clamp(Settings->EffectsVolume, 0, BDSettingsPrivate::VolumeMax)) / BDSettingsPrivate::VolumeMax;

	const auto Override = [World, Mix](USoundClass* Class, const float Volume)
	{
		if (Class != nullptr)
		{
			UGameplayStatics::SetSoundMixClassOverride(World, Mix, Class, Volume, /*Pitch*/ 1.0f, /*FadeIn*/ 0.0f, /*bApplyToChildren*/ true);
		}
	};

	Override(Master, MasterVolume);
	Override(Music, MusicVolume);
	Override(Effects, EffectsVolume);
	UGameplayStatics::PushSoundMixModifier(World, Mix);

	UE_LOG(LogBDUI, Verbose, TEXT("Audio applied on %s: master %.2f, music %.2f, effects %.2f."),
		*World->GetName(), MasterVolume, MusicVolume, EffectsVolume);
}

void UBDSettingsSubsystem::SetMusicVolume(const int32 Volume)
{
	if (Settings == nullptr)
	{
		return;
	}

	Settings->MusicVolume = FMath::Clamp(Volume, 0, BDSettingsPrivate::VolumeMax);
	ApplyAudio(GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr);
	Save();
}

void UBDSettingsSubsystem::SetEffectsVolume(const int32 Volume)
{
	if (Settings == nullptr)
	{
		return;
	}

	Settings->EffectsVolume = FMath::Clamp(Volume, 0, BDSettingsPrivate::VolumeMax);
	ApplyAudio(GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr);
	Save();
}

void UBDSettingsSubsystem::SetMuted(const bool bMuted)
{
	if (Settings == nullptr)
	{
		return;
	}

	Settings->bMuted = bMuted;
	ApplyAudio(GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr);
	Save();
}

//~ Language ------------------------------------------------------------------------

void UBDSettingsSubsystem::ApplyLanguage()
{
	if (Settings != nullptr)
	{
		BDLoc::SetLanguage(Settings->Language);
	}
}

void UBDSettingsSubsystem::SetLanguage(const EBDLanguage Language)
{
	if (Settings == nullptr)
	{
		return;
	}

	Settings->Language = Language;
	ApplyLanguage();
	Save();
}

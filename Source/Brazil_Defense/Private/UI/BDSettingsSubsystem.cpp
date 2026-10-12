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

int32 UBDSettingsSave::GetVolume(const EBDSoundCategory Category) const
{
	switch (Category)
	{
	case EBDSoundCategory::Voice:
		return VoiceVolume;
	case EBDSoundCategory::Music:
		return MusicVolume;
	case EBDSoundCategory::Environment:
		return EnvironmentVolume;
	default:
		return EffectsVolume;
	}
}

bool UBDSettingsSave::IsMuted(const EBDSoundCategory Category) const
{
	switch (Category)
	{
	case EBDSoundCategory::Voice:
		return bVoiceMuted;
	case EBDSoundCategory::Music:
		return bMusicMuted;
	case EBDSoundCategory::Environment:
		return bEnvironmentMuted;
	default:
		return bEffectsMuted;
	}
}

void UBDSettingsSave::SetVolume(const EBDSoundCategory Category, const int32 Volume)
{
	switch (Category)
	{
	case EBDSoundCategory::Voice:
		VoiceVolume = Volume;
		break;
	case EBDSoundCategory::Music:
		MusicVolume = Volume;
		break;
	case EBDSoundCategory::Environment:
		EnvironmentVolume = Volume;
		break;
	default:
		EffectsVolume = Volume;
		break;
	}
}

void UBDSettingsSave::SetMuted(const EBDSoundCategory Category, const bool bMute)
{
	switch (Category)
	{
	case EBDSoundCategory::Voice:
		bVoiceMuted = bMute;
		break;
	case EBDSoundCategory::Music:
		bMusicMuted = bMute;
		break;
	case EBDSoundCategory::Environment:
		bEnvironmentMuted = bMute;
		break;
	default:
		bEffectsMuted = bMute;
		break;
	}
}

void UBDSettingsSave::ResetAudio()
{
	const UBDSettingsSave* Defaults = GetDefault<UBDSettingsSave>();
	for (int32 Index = 0; Index < static_cast<int32>(EBDSoundCategory::Count); ++Index)
	{
		const EBDSoundCategory Category = static_cast<EBDSoundCategory>(Index);
		SetVolume(Category, Defaults->GetVolume(Category));
		SetMuted(Category, Defaults->IsMuted(Category));
	}
	bMuted = false;
	AudioLayout = CurrentAudioLayout;
}

namespace BDSettingsPrivate
{
	static constexpr int32 LowestQuality = 0;
	static constexpr int32 HighestQuality = 3;

	/**
	 * The most Auto may land on: High, 75% of the screen rebuilt by TSR. The engine puts the
	 * RTX 4070 Laptop on Epic, native, which ran a light match at 3440x1440 at 28 fps
	 * against 103 on High (2026-10-12). Epic stays a pick of the menu.
	 */
	static constexpr int32 AutoQualityCeiling = 2;
	static constexpr int32 VolumeMax = 100;

	/** "voices 80, music 20 (muted), ...", for the log. */
	static FString DescribeAudio(const UBDSettingsSave& Saved)
	{
		FString Text;
		for (int32 Index = 0; Index < static_cast<int32>(EBDSoundCategory::Count); ++Index)
		{
			const EBDSoundCategory Category = static_cast<EBDSoundCategory>(Index);
			Text += FString::Printf(TEXT("%s%s %d%s"), Text.IsEmpty() ? TEXT("") : TEXT(", "),
				*StaticEnum<EBDSoundCategory>()->GetNameStringByValue(Index).ToLower(),
				Saved.GetVolume(Category), Saved.IsMuted(Category) ? TEXT(" (muted)") : TEXT(""));
		}
		return Text;
	}

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
		UpgradeAudioLayout();
		UE_LOG(LogBDUI, Log, TEXT("Settings loaded: quality %d, %dx%d %s, vsync %s, %s, language %s."),
			Settings->QualityLevel, Settings->Resolution.X, Settings->Resolution.Y,
			*StaticEnum<EBDWindowMode>()->GetNameStringByValue(static_cast<int64>(Settings->WindowMode)),
			Settings->bVSync ? TEXT("on") : TEXT("off"), *BDSettingsPrivate::DescribeAudio(*Settings),
			*StaticEnum<EBDLanguage>()->GetNameStringByValue(static_cast<int64>(Settings->Language)));
		return;
	}

	// First run, or a save that no longer reads: the defaults of the class, written out
	// so the next run finds them.
	Settings = Cast<UBDSettingsSave>(UGameplayStatics::CreateSaveGameObject(UBDSettingsSave::StaticClass()));
	Settings->AudioLayout = UBDSettingsSave::CurrentAudioLayout;
	Save();
	UE_LOG(LogBDUI, Log, TEXT("No settings save: defaults written to slot %s."), UBDSettingsSave::SlotName);
}

void UBDSettingsSubsystem::UpgradeAudioLayout()
{
	if (Settings == nullptr || Settings->AudioLayout >= UBDSettingsSave::CurrentAudioLayout)
	{
		return;
	}

	// The old effects slider held the words and the ambience too: no number of it means
	// what one of the four sliders means now, so they start from the defaults. A save that
	// was muted stays silent, on all four.
	const bool bWasMuted = Settings->bMuted;
	Settings->ResetAudio();
	for (int32 Index = 0; Index < static_cast<int32>(EBDSoundCategory::Count); ++Index)
	{
		Settings->SetMuted(static_cast<EBDSoundCategory>(Index), bWasMuted);
	}
	Save();
	UE_LOG(LogBDUI, Log, TEXT("Settings: the two-slider audio of the save became the four categories at their defaults%s."),
		bWasMuted ? TEXT(", all muted as the save was") : TEXT(""));
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

	// Auto leaves scalability where the engine put it, up to High (outside the editor, whose
	// viewport keeps its own); an explicit pick sets its level.
	if (Settings->QualityLevel >= 0)
	{
		UserSettings->SetOverallScalabilityLevel(FMath::Clamp(Settings->QualityLevel, BDSettingsPrivate::LowestQuality, BDSettingsPrivate::HighestQuality));
	}
	else if (!GIsEditor)
	{
		const int32 EngineLevel = UserSettings->GetOverallScalabilityLevel();
		if (EngineLevel < 0 || EngineLevel > BDSettingsPrivate::AutoQualityCeiling)
		{
			UserSettings->SetOverallScalabilityLevel(BDSettingsPrivate::AutoQualityCeiling);
		}
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
	if (Mix == nullptr)
	{
		UE_LOG(LogBDUI, Warning, TEXT("No settings sound mix configured (Project Settings > Brazil Defense - Interface): volumes are saved but drive nothing."));
		return;
	}

	// The class of each category, in the order of EBDSoundCategory.
	USoundClass* const Classes[] = {
		UISettings.VoiceSoundClass.LoadSynchronous(), UISettings.MusicSoundClass.LoadSynchronous(),
		UISettings.EnvironmentSoundClass.LoadSynchronous(), UISettings.EffectsSoundClass.LoadSynchronous() };
	static_assert(UE_ARRAY_COUNT(Classes) == static_cast<int32>(EBDSoundCategory::Count), "one class per sound category");

	// Kept alive past every map load: see HeldClasses.
	HeldMix = Mix;
	if (USoundClass* Master = UISettings.MasterSoundClass.LoadSynchronous())
	{
		HeldClasses.AddUnique(Master);
	}

	// A mute is its class at zero; the slider keeps its value underneath, so unmuting brings
	// back exactly what was set. The four are siblings, so none carries another along.
	for (int32 Index = 0; Index < static_cast<int32>(EBDSoundCategory::Count); ++Index)
	{
		USoundClass* Class = Classes[Index];
		if (Class == nullptr)
		{
			UE_LOG(LogBDUI, Warning, TEXT("No sound class for %s (Project Settings > Brazil Defense - Interface): its slider drives nothing."),
				*StaticEnum<EBDSoundCategory>()->GetNameStringByValue(Index));
			continue;
		}
		HeldClasses.AddUnique(Class);
		const EBDSoundCategory Category = static_cast<EBDSoundCategory>(Index);
		const float Volume = Settings->IsMuted(Category) ? 0.0f
			: static_cast<float>(FMath::Clamp(Settings->GetVolume(Category), 0, BDSettingsPrivate::VolumeMax)) / BDSettingsPrivate::VolumeMax;
		UGameplayStatics::SetSoundMixClassOverride(World, Mix, Class, Volume, /*Pitch*/ 1.0f, /*FadeIn*/ 0.0f, /*bApplyToChildren*/ false);
	}
	UGameplayStatics::PushSoundMixModifier(World, Mix);

	UE_LOG(LogBDUI, Verbose, TEXT("Audio applied on %s: %s."), *World->GetName(), *BDSettingsPrivate::DescribeAudio(*Settings));
}

void UBDSettingsSubsystem::SetVolume(const EBDSoundCategory Category, const int32 Volume)
{
	if (Settings == nullptr)
	{
		return;
	}

	Settings->SetVolume(Category, FMath::Clamp(Volume, 0, BDSettingsPrivate::VolumeMax));
	ApplyAudio(GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr);
	Save();
}

void UBDSettingsSubsystem::SetMuted(const EBDSoundCategory Category, const bool bMuted)
{
	if (Settings == nullptr)
	{
		return;
	}

	Settings->SetMuted(Category, bMuted);
	ApplyAudio(GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr);
	Save();
}

//~ Controls ------------------------------------------------------------------------

void UBDSettingsSubsystem::SetInvertCameraSideways(const bool bInvert)
{
	if (Settings == nullptr)
	{
		return;
	}

	// Read by the controller every tick; nothing to apply here.
	Settings->bInvertCameraSideways = bInvert;
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

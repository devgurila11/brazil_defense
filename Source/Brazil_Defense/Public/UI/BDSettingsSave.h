// Brazil Defense. What the options panel remembers between sessions.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "UI/BDLocalization.h"
#include "BDSettingsSave.generated.h"

/** Window modes offered by the options panel, in the order of its list. */
UENUM(BlueprintType)
enum class EBDWindowMode : uint8
{
	Fullscreen UMETA(DisplayName = "Fullscreen"),
	Windowed UMETA(DisplayName = "Windowed"),
	Borderless UMETA(DisplayName = "Borderless"),

	Count UMETA(Hidden)
};

/** The four sound categories of the options, each with a slider and a mute of its own, in the order of the panel. */
UENUM(BlueprintType)
enum class EBDSoundCategory : uint8
{
	Voice UMETA(DisplayName = "Voices"),
	Music UMETA(DisplayName = "Music"),
	Environment UMETA(DisplayName = "Environment"),
	Effects UMETA(DisplayName = "Effects"),

	Count UMETA(Hidden)
};

/**
 * The player's settings, as one save slot. Graphics, audio and language; nothing about
 * a match. Written on every change in the options panel and read once at startup by
 * UBDSettingsSubsystem, which is the only thing that applies it.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:
	static const TCHAR* SlotName;
	static constexpr int32 UserIndex = 0;

	//~ Graphics --------------------------------------------------------------

	/** Scalability preset, 0 Low .. 3 Epic. -1 lets the engine pick from the hardware. */
	UPROPERTY()
	int32 QualityLevel = -1;

	/** Zero means "whatever the display has". */
	UPROPERTY()
	FIntPoint Resolution = FIntPoint::ZeroValue;

	UPROPERTY()
	EBDWindowMode WindowMode = EBDWindowMode::Windowed;

	UPROPERTY()
	bool bVSync = true;

	//~ Audio -----------------------------------------------------------------
	// One volume, 0..100, and one mute per category. The words lead: they are the charm
	// of the game, so they start loudest and the music sits well under them.

	UPROPERTY()
	int32 VoiceVolume = 80;

	UPROPERTY()
	int32 MusicVolume = 20;

	UPROPERTY()
	int32 EnvironmentVolume = 40;

	UPROPERTY()
	int32 EffectsVolume = 50;

	UPROPERTY()
	bool bVoiceMuted = false;

	UPROPERTY()
	bool bMusicMuted = false;

	UPROPERTY()
	bool bEnvironmentMuted = false;

	UPROPERTY()
	bool bEffectsMuted = false;

	/**
	 * The one mute of the two-slider panel, before the four categories. Read once, when an
	 * older save is brought up to them (see AudioLayout), and cleared.
	 */
	UPROPERTY()
	bool bMuted = false;

	/**
	 * Which audio layout the save was written with: 0 for the two sliders (music, and effects
	 * with the words and the ambience inside), CurrentAudioLayout for the four categories. A
	 * save of the old layout takes the four defaults once, since its effects number covered
	 * what three sliders own now.
	 */
	UPROPERTY()
	int32 AudioLayout = 0;

	static constexpr int32 CurrentAudioLayout = 1;

	int32 GetVolume(EBDSoundCategory Category) const;
	bool IsMuted(EBDSoundCategory Category) const;
	void SetVolume(EBDSoundCategory Category, int32 Volume);
	void SetMuted(EBDSoundCategory Category, bool bMute);

	/** The audio fields back to the class defaults, in the current layout. */
	void ResetAudio();

	//~ Controls -----------------------------------------------------------------

	/** A and D the other way round for the match camera. */
	UPROPERTY()
	bool bInvertCameraSideways = false;

	//~ Language ----------------------------------------------------------------

	UPROPERTY()
	EBDLanguage Language = EBDLanguage::English;
};

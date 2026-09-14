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

	/** 0..100. */
	UPROPERTY()
	int32 MusicVolume = 80;

	/** 0..100. */
	UPROPERTY()
	int32 EffectsVolume = 80;

	UPROPERTY()
	bool bMuted = false;

	//~ Controls -----------------------------------------------------------------

	/** A and D the other way round for the match camera. */
	UPROPERTY()
	bool bInvertCameraSideways = false;

	//~ Language ----------------------------------------------------------------

	UPROPERTY()
	EBDLanguage Language = EBDLanguage::English;
};

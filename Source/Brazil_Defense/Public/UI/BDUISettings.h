// Brazil Defense. Configuration of the interface layer: maps, timings and the audio it drives.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BDUISettings.generated.h"

class USoundClass;
class USoundMix;
class UTexture2D;
class UWorld;

/**
 * Everything about the screens that is not the screens themselves, kept out of the code
 * so it can be tuned without a recompile. Edited in Project Settings > Game > Brazil
 * Defense - Interface.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Interface"))
class BRAZIL_DEFENSE_API UBDUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDUISettings();

	static const UBDUISettings& Get();

	//~ Flow -----------------------------------------------------------------

	/** The level the front end (splash, loading, menu) runs in. */
	UPROPERTY(config, EditAnywhere, Category = "Flow", meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> MenuMap;

	/** The level Play opens. */
	UPROPERTY(config, EditAnywhere, Category = "Flow", meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> GameMap;

	/** The studio logo on the splash, above the name. Optional. */
	UPROPERTY(config, EditAnywhere, Category = "Flow", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> SplashLogo;

	//~ HUD proportions -------------------------------------------------------------
	// Fractions of the viewport, applied whenever it changes size, on top of the DPI
	// curve: an icon is the same share of the screen at 1080p, 1440p and 4K, and on an
	// ultrawide it does not grow with the width.

	/** Height of an item bar icon, as a fraction of the viewport height. */
	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (ClampMin = "0.02", ClampMax = "0.25", UIMin = "0.02", UIMax = "0.25"))
	float ItemIconHeightFraction = 0.075f;

	/** Width of the side panels (defender, piece in hand), as a fraction of the viewport width. */
	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (ClampMin = "0.1", ClampMax = "0.4", UIMin = "0.1", UIMax = "0.4"))
	float SidePanelWidthFraction = 0.18f;

	/** Width of the candidate's health bar, as a fraction of the viewport width. */
	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (ClampMin = "0.1", ClampMax = "0.8", UIMin = "0.1", UIMax = "0.8"))
	float CandidateBarWidthFraction = 0.28f;

	/** Height the logo is drawn at, in Slate units; the width follows the texture. */
	UPROPERTY(config, EditAnywhere, Category = "Flow", meta = (ClampMin = "16.0", UIMin = "16.0"))
	float SplashLogoHeight = 240.0f;

	/** Seconds the splash stays up when nobody skips it. */
	UPROPERTY(config, EditAnywhere, Category = "Flow", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float SplashSeconds = 2.0f;

	/** Least time the loading screen stays up, so it does not flash. */
	UPROPERTY(config, EditAnywhere, Category = "Flow", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float LoadingMinSeconds = 0.5f;

	/** Seconds of a fade to or from black between screens, and between the menu and the game. */
	UPROPERTY(config, EditAnywhere, Category = "Flow", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float FadeSeconds = 0.5f;

	/** Seconds the "candidate is out" notice stays on the HUD. */
	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float CandidateNoticeSeconds = 4.0f;

	//~ Audio -----------------------------------------------------------------
	// The option sliders drive these through a sound mix override, so any sound put on
	// one of the classes follows the sliders from day one.

	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (AllowedClasses = "/Script/Engine.SoundClass"))
	TSoftObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (AllowedClasses = "/Script/Engine.SoundClass"))
	TSoftObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (AllowedClasses = "/Script/Engine.SoundClass"))
	TSoftObjectPtr<USoundClass> EffectsSoundClass;

	/** The mix the volume overrides are pushed through. */
	UPROPERTY(config, EditAnywhere, Category = "Audio", meta = (AllowedClasses = "/Script/Engine.SoundMix"))
	TSoftObjectPtr<USoundMix> SettingsSoundMix;

	//~ Text -------------------------------------------------------------------
	// One string table per language, loaded from a CSV under Content so the texts are
	// edited as a spreadsheet and never typed into code. Key,SourceString per line.

	/** CSV of the English table, relative to the Content folder. */
	UPROPERTY(config, EditAnywhere, Category = "Text")
	FString EnglishTableFile = TEXT("BD/Text/BD_en.csv");

	/** CSV of the Portuguese table, relative to the Content folder. */
	UPROPERTY(config, EditAnywhere, Category = "Text")
	FString PortugueseTableFile = TEXT("BD/Text/BD_pt.csv");
};

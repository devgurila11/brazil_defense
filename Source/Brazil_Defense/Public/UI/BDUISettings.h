// Brazil Defense. Configuration of the interface layer: maps, timings and the audio it drives.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Audio/BDSpeechMarks.h"
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

	//~ Scoreboard pictures -----------------------------------------------------
	// Fixed pictures of the HUD, not pieces: the blue ballot beside the blue count, the
	// urn between the two counts, the red ballot beside the red count.

	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> ScoreBlueIcon;

	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> ScoreUrnIcon;

	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> ScoreRedIcon;

	/** The null ballot, beside the null count under the bar. Optional until the art lands. */
	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> ScoreNullIcon;

	//~ The money counters -------------------------------------------------------
	// Under the count bar, side by side: the thief on the left holding the bribe just
	// recovered, the mint on the right holding the public money the player spends. The
	// value crosses from one to the other when the till rings.

	/** The thief, beside the bribe recovered and not yet minted. */
	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> BribeIcon;

	/** The mint, beside the public money: the spendable balance. */
	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> MintIcon;

	/** Height of the two money pictures, as a fraction of the viewport height. */
	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (ClampMin = "0.02", ClampMax = "0.2", UIMin = "0.02", UIMax = "0.2"))
	float MoneyIconHeightFraction = 0.05f;

	/** Width of the count bar, as a fraction of the viewport width. */
	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (ClampMin = "0.1", ClampMax = "0.8", UIMin = "0.1", UIMax = "0.8"))
	float ScoreBarWidthFraction = 0.26f;

	//~ Creep health bars ---------------------------------------------------------
	// Drawn by ABDMatchHUD over every damaged creep: a fixed width in the world, so the
	// zoom sizes them, and dropped once thinner than CreepBarMinPixels.

	UPROPERTY(config, EditAnywhere, Category = "Creep Bars", meta = (ClampMin = "10.0", UIMin = "10.0", ForceUnits = "cm"))
	float CreepBarWorldWidth = 260.0f;

	/** How high above the creep's location the bar floats. */
	UPROPERTY(config, EditAnywhere, Category = "Creep Bars", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float CreepBarWorldHeight = 220.0f;

	/** Bar height as a fraction of its on-screen width. */
	UPROPERTY(config, EditAnywhere, Category = "Creep Bars", meta = (ClampMin = "0.05", ClampMax = "0.5", UIMin = "0.05", UIMax = "0.5"))
	float CreepBarAspect = 0.14f;

	/** Bars narrower than this many pixels are not drawn: the overview stays clean. */
	UPROPERTY(config, EditAnywhere, Category = "Creep Bars", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float CreepBarMinPixels = 14.0f;

	//~ Speech marks --------------------------------------------------------------
	// Drawn by ABDMatchHUD over whoever is saying a sentence (UBDSpeechMarkSubsystem): an
	// exclamation in the colour of his side, from the first word to the last. Sized in the
	// world like the creep bars, so the zoom sizes it and the overview drops it.

	/** Height of the exclamation in the world. */
	UPROPERTY(config, EditAnywhere, Category = "Speech Marks", meta = (ClampMin = "10.0", UIMin = "10.0", ForceUnits = "cm"))
	float SpeechMarkWorldHeight = 160.0f;

	/** Gap between the top of the speaker (or his health bar) and the foot of the exclamation. */
	UPROPERTY(config, EditAnywhere, Category = "Speech Marks", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float SpeechMarkLift = 40.0f;

	/** Exclamations shorter than this many pixels are not drawn: the overview stays clean. */
	UPROPERTY(config, EditAnywhere, Category = "Speech Marks", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float SpeechMarkMinPixels = 9.0f;

	/** The pop when the words start: this long, growing by PulseScale at its peak. */
	UPROPERTY(config, EditAnywhere, Category = "Speech Marks", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float SpeechMarkPulseSeconds = 0.22f;

	UPROPERTY(config, EditAnywhere, Category = "Speech Marks", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	float SpeechMarkPulseScale = 0.45f;

	/** Strength of the soft halo round it: a little light in the action and at night, not a beacon. */
	UPROPERTY(config, EditAnywhere, Category = "Speech Marks", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float SpeechMarkGlow = 0.35f;

	UPROPERTY(config, EditAnywhere, Category = "Speech Marks")
	FLinearColor SpeechMarkOpponentColor = FLinearColor(0.95f, 0.12f, 0.10f, 1.0f);

	UPROPERTY(config, EditAnywhere, Category = "Speech Marks")
	FLinearColor SpeechMarkPlayerColor = FLinearColor(0.15f, 0.55f, 1.0f, 1.0f);

	/** The ministers' robes. Its rim and halo go light, or black would vanish at night. */
	UPROPERTY(config, EditAnywhere, Category = "Speech Marks")
	FLinearColor SpeechMarkMinisterColor = FLinearColor(0.03f, 0.03f, 0.03f, 1.0f);

	FLinearColor GetSpeechMarkColor(EBDSpeakerSide Side) const;

	//~ Agent bar -----------------------------------------------------------------
	// Drawn by ABDMatchHUD over the palace's Agent, as the creep bars, a fixed width in the
	// world over his head: his patrol time emptying while awake, his rest filling while he
	// sleeps, in a colour of its own.

	UPROPERTY(config, EditAnywhere, Category = "Agent Bar", meta = (ClampMin = "10.0", UIMin = "10.0", ForceUnits = "cm"))
	float AgentBarWorldWidth = 360.0f;

	UPROPERTY(config, EditAnywhere, Category = "Agent Bar")
	FLinearColor AgentBarColor = FLinearColor(0.15f, 0.55f, 1.0f, 1.0f);

	UPROPERTY(config, EditAnywhere, Category = "Agent Bar")
	FLinearColor AgentRestBarColor = FLinearColor(0.62f, 0.55f, 0.85f, 1.0f);

	//~ Palace stars --------------------------------------------------------------
	// Drawn by ABDMatchHUD over every palace: five stars facing the camera, as many filled
	// as its level, the rest an outline. Sized by the screen, not the world, so they read
	// the same at every zoom.

	/** Height of one star, as a fraction of the viewport height. */
	UPROPERTY(config, EditAnywhere, Category = "Palace Stars", meta = (ClampMin = "0.005", ClampMax = "0.1", UIMin = "0.005", UIMax = "0.1"))
	float PalaceStarHeightFraction = 0.032f;

	/** Gap between two stars, as a fraction of a star's height. */
	UPROPERTY(config, EditAnywhere, Category = "Palace Stars", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float PalaceStarGap = 0.15f;

	UPROPERTY(config, EditAnywhere, Category = "Palace Stars")
	FLinearColor PalaceStarColor = FLinearColor(1.0f, 0.78f, 0.1f, 1.0f);

	/**
	 * The stars fade with the camera's distance to the palace: whole up to FadeStart,
	 * gone from FadeEnd, a smooth fade between. Close enough to decide on that palace,
	 * they read; in the overview they are not there. The overview sits at some 33000 cm.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Palace Stars", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float PalaceStarFadeStart = 10000.0f;

	UPROPERTY(config, EditAnywhere, Category = "Palace Stars", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float PalaceStarFadeEnd = 18000.0f;

	/** Opacity of the stars for a camera this far from the palace: 1 up to the fade start, 0 from its end. */
	float GetPalaceStarOpacity(float CameraDistance) const;

	/**
	 * The same row over every piece that evolves: ground towers by their level, platforms by
	 * the level of their block (0 until every slot is manned). Smaller than the palace's, so
	 * the palace still reads first. Colour and fade are the palace's.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Palace Stars", meta = (ClampMin = "0.1", ClampMax = "1.0", UIMin = "0.1", UIMax = "1.0"))
	float PieceStarScale = 0.7f;

	/** Centimetres over the top of the piece the row floats at. */
	UPROPERTY(config, EditAnywhere, Category = "Palace Stars", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float PieceStarLift = 60.0f;

	/**
	 * Room left over a platform's deck for the crew, under its row. The row hangs off the
	 * construction alone: a shooter's bounds move with every pose and kick, and a row that
	 * followed them climbed and shook whenever someone was on board.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Palace Stars", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float PlatformCrewClearance = 220.0f;

	/**
	 * Opacity of the item bar's background at the bottom, 0 to 1. Low on purpose: the
	 * board behind it is where the piece is going, so the bar should not hide it.
	 */
	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float ItemBarOpacity = 0.3f;

	/** Opacity of an item of the bar not in hand. The one in hand stays solid. */
	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float ItemIdleOpacity = 0.55f;

	/** Height of the ballot pictures, as a fraction of the viewport height; the urn is drawn a little larger. */
	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (ClampMin = "0.02", ClampMax = "0.2", UIMin = "0.02", UIMax = "0.2"))
	float ScoreIconHeightFraction = 0.045f;

	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (ClampMin = "1.0", ClampMax = "3.0", UIMin = "1.0", UIMax = "3.0"))
	float ScoreUrnIconScale = 1.5f;

	/** Width of the candidate's health bar, as a fraction of the viewport width. */
	UPROPERTY(config, EditAnywhere, Category = "HUD", meta = (ClampMin = "0.1", ClampMax = "0.8", UIMin = "0.1", UIMax = "0.8"))
	float CandidateBarWidthFraction = 0.28f;

	//~ Kill boards ---------------------------------------------------------------
	// Down the sides: the kinds of creep killed on the left, the candidates brought down on
	// the right. Each icon turns up with the first kill of its kind. Only a read-out.

	/** Height of a kill board icon, as a fraction of the viewport height. */
	UPROPERTY(config, EditAnywhere, Category = "Kill Boards", meta = (ClampMin = "0.02", ClampMax = "0.2", UIMin = "0.02", UIMax = "0.2"))
	float KillIconHeightFraction = 0.055f;

	/** The candidates' icon on the right board, until each has a face of his own. */
	UPROPERTY(config, EditAnywhere, Category = "Kill Boards", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> CandidateKillIcon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/BD/UI/T_UI_Candidate_Placeholder.T_UI_Candidate_Placeholder")));

	/**
	 * One face per scheduled candidate, by his number (first is index 0), for when they get
	 * faces. Not read yet: the board shows one counter with CandidateKillIcon for all of them.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Kill Boards", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TArray<TSoftObjectPtr<UTexture2D>> CandidateFaces;

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

// Brazil Defense. Which screen is up, and how one gives way to the next.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BDUISubsystem.generated.h"

class APlayerController;
class UBDFadeWidget;
class UBDHUDWidget;
class UBDOptionsWidget;
class UBDPauseWidget;
class UBDWidgetBase;
class UUserWidget;
class UWorld;

/** The screens of the interface. One is up at a time; the options panel sits over it. */
UENUM(BlueprintType)
enum class EBDScreen : uint8
{
	None,
	Splash,
	Loading,
	MainMenu,
	HUD
};

/**
 * The flow of the interface, as one state machine that outlives the levels:
 *
 *   Splash -> Loading -> Main menu -> [Play] -> fade -> the game, with its HUD
 *
 * The front end runs in the menu level (ABDMenuGameMode starts it); Play fades to
 * black, opens the game level, and ABDGameMode puts the HUD up when the match is
 * ready, after which the fade lifts. Every change of screen goes through a fade, whose
 * length is one setting. The options panel is not a screen: it is put over whatever is
 * up and taken away again.
 *
 * Screens are created here from their C++ classes. The classes are settable so the
 * visual pass can hand over Blueprint children without touching the flow.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDUISubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	//~ Flow ---------------------------------------------------------------------

	/** Splash, then loading, then the menu. Called by the menu game mode. */
	void StartFrontEnd();

	/** Skips the rest of the splash. */
	void FinishSplash();

	/** Fades out, opens the game level; the HUD follows once the level is up. */
	void PlayGame();

	/** Fades out and returns to the menu level. */
	void ReturnToMenu();

	void QuitGame();

	/** Puts the HUD over the game. Called by the game mode once the match exists. Fades in from black. */
	void ShowHUD();

	/** Whether the splash and the menu have run in this session. Play in Editor skips them on purpose. */
	bool HasSeenFrontEnd() const { return bFrontEndSeen; }

	/** Opens the menu level outright, no fade: for a game level that came up before the front end did. */
	void RedirectToFrontEnd();

	//~ Options ------------------------------------------------------------------

	void OpenOptions();
	void CloseOptions();
	bool IsOptionsOpen() const { return Options != nullptr; }

	//~ In-game menu ---------------------------------------------------------------
	// Over the HUD only. The match is paused while it is up.

	void OpenPauseMenu();
	void ClosePauseMenu();
	void TogglePauseMenu();
	bool IsPauseMenuOpen() const { return PauseMenu != nullptr; }

	//~ State --------------------------------------------------------------------

	EBDScreen GetCurrentScreen() const { return CurrentScreen; }
	UBDHUDWidget* GetHUD() const;

private:
	APlayerController* GetLocalController() const;

	/** Replaces the current screen with a new one, no fade. */
	void SetScreen(EBDScreen Screen);

	/** Fades to black, swaps the screen, fades back. */
	void TransitionTo(EBDScreen Screen);

	/** The widget class behind a screen, and a fresh instance of it. */
	TSubclassOf<UUserWidget> ClassForScreen(EBDScreen Screen) const;
	UUserWidget* CreateScreen(EBDScreen Screen);

	/** The fade overlay of the current world, created on demand. */
	UBDFadeWidget* GetOrCreateFade(bool bStartBlack);

	void HandlePreLoadMap(const FString& MapName);
	void HandleLoadingDone();

	/** Cursor and input mode fit for a screen: a menu wants the cursor and only the UI, the game wants both. */
	void ApplyInputMode(EBDScreen Screen);

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> CurrentWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBDOptionsWidget> Options;

	UPROPERTY(Transient)
	TObjectPtr<UBDPauseWidget> PauseMenu;

	UPROPERTY(Transient)
	TObjectPtr<UBDFadeWidget> Fade;

	EBDScreen CurrentScreen = EBDScreen::None;

	/** Set when Play was pressed: the next level to come up is the game and gets the HUD. */
	bool bGamePending = false;

	/** Set once StartFrontEnd ran in this session. */
	bool bFrontEndSeen = false;

	FDelegateHandle PreLoadMapHandle;
	FTimerHandle SplashTimer;
	FTimerHandle LoadingTimer;
};

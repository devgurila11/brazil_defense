// Brazil Defense. Which screen is up, and how one gives way to the next.

#include "UI/BDUISubsystem.h"

#include "BDLog.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Match/BDMatchManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UI/BDFadeWidget.h"
#include "UI/BDFrontEndWidgets.h"
#include "UI/BDHUDWidget.h"
#include "UI/BDLocalization.h"
#include "UI/BDOptionsWidget.h"
#include "UI/BDPauseWidget.h"
#include "UI/BDUISettings.h"

namespace BDUIPrivate
{
	/** Z order of the layers: screens under the options panel, everything under the fade. */
	static constexpr int32 ScreenZ = 0;
	static constexpr int32 PauseZ = 5;
	static constexpr int32 OptionsZ = 10;
	static constexpr int32 FadeZ = 100;
}

void UBDUISubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	BDLoc::Initialize();
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UBDUISubsystem::HandlePreLoadMap);

	// A headless check launched straight into a game level has no use for the menu:
	// the switch counts the front end as seen so the game mode keeps the level.
	if (FParse::Param(FCommandLine::Get(), TEXT("BDSkipFrontEnd")))
	{
		bFrontEndSeen = true;
		UE_LOG(LogBDUI, Log, TEXT("Front end skipped by -BDSkipFrontEnd."));
	}
}

void UBDUISubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	Super::Deinitialize();
}

APlayerController* UBDUISubsystem::GetLocalController() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetFirstLocalPlayerController() : nullptr;
}

UBDHUDWidget* UBDUISubsystem::GetHUD() const
{
	return CurrentScreen == EBDScreen::HUD ? Cast<UBDHUDWidget>(CurrentWidget) : nullptr;
}

//~ Screens -----------------------------------------------------------------------

TSubclassOf<UUserWidget> UBDUISubsystem::ClassForScreen(const EBDScreen Screen) const
{
	switch (Screen)
	{
	case EBDScreen::Splash:
		return UBDSplashWidget::StaticClass();
	case EBDScreen::Loading:
		return UBDLoadingWidget::StaticClass();
	case EBDScreen::MainMenu:
		return UBDMainMenuWidget::StaticClass();
	case EBDScreen::DifficultySelect:
		return UBDDifficultySelectWidget::StaticClass();
	case EBDScreen::HUD:
		return UBDHUDWidget::StaticClass();
	default:
		return nullptr;
	}
}

UUserWidget* UBDUISubsystem::CreateScreen(const EBDScreen Screen)
{
	APlayerController* Controller = GetLocalController();
	const TSubclassOf<UUserWidget> Class = ClassForScreen(Screen);
	if (Controller == nullptr || Class == nullptr)
	{
		return nullptr;
	}

	return CreateWidget<UUserWidget>(Controller, Class);
}

void UBDUISubsystem::SetScreen(const EBDScreen Screen)
{
	using namespace BDUIPrivate;

	if (CurrentWidget != nullptr)
	{
		CurrentWidget->RemoveFromParent();
		CurrentWidget = nullptr;
	}

	CurrentScreen = Screen;
	CurrentWidget = CreateScreen(Screen);
	if (CurrentWidget != nullptr)
	{
		CurrentWidget->AddToViewport(ScreenZ);
	}
	else if (Screen != EBDScreen::None)
	{
		UE_LOG(LogBDUI, Error, TEXT("Screen %s could not be created: no local player controller yet."),
			*StaticEnum<EBDScreen>()->GetNameStringByValue(static_cast<int64>(Screen)));
	}

	ApplyInputMode(Screen);

	UE_LOG(LogBDUI, Log, TEXT("Screen: %s."), *StaticEnum<EBDScreen>()->GetNameStringByValue(static_cast<int64>(Screen)));
}

void UBDUISubsystem::ApplyInputMode(const EBDScreen Screen)
{
	APlayerController* Controller = GetLocalController();
	if (Controller == nullptr)
	{
		return;
	}

	if (Screen == EBDScreen::HUD)
	{
		// The board is played with the mouse under the HUD: both get the clicks.
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Controller->SetInputMode(Mode);
	}
	else
	{
		FInputModeUIOnly Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		if (CurrentWidget != nullptr)
		{
			Mode.SetWidgetToFocus(CurrentWidget->TakeWidget());
		}
		Controller->SetInputMode(Mode);
	}

	Controller->bShowMouseCursor = true;
}

UBDFadeWidget* UBDUISubsystem::GetOrCreateFade(const bool bStartBlack)
{
	using namespace BDUIPrivate;

	if (Fade != nullptr && Fade->IsInViewport())
	{
		return Fade;
	}

	APlayerController* Controller = GetLocalController();
	if (Controller == nullptr)
	{
		return nullptr;
	}

	Fade = CreateWidget<UBDFadeWidget>(Controller, UBDFadeWidget::StaticClass());
	if (Fade != nullptr)
	{
		Fade->AddToViewport(FadeZ);
		Fade->SetBlack(bStartBlack ? 1.0f : 0.0f);
	}
	return Fade;
}

void UBDUISubsystem::TransitionTo(const EBDScreen Screen)
{
	UBDFadeWidget* Overlay = GetOrCreateFade(false);
	if (Overlay == nullptr)
	{
		SetScreen(Screen);
		return;
	}

	const float Seconds = UBDUISettings::Get().FadeSeconds;
	TWeakObjectPtr<UBDUISubsystem> WeakThis(this);
	Overlay->FadeTo(1.0f, Seconds, [WeakThis, Screen, Seconds]()
	{
		if (!WeakThis.IsValid())
		{
			return;
		}

		WeakThis->SetScreen(Screen);
		if (UBDFadeWidget* Lift = WeakThis->GetOrCreateFade(true))
		{
			Lift->FadeTo(0.0f, Seconds, nullptr);
		}
	});
}

//~ Flow --------------------------------------------------------------------------

void UBDUISubsystem::StartFrontEnd()
{
	bGamePending = false;
	bFrontEndSeen = true;

	// The splash comes up under a lifted black, like every screen after it, and gives
	// way on its own after its seconds unless a key ends it first.
	SetScreen(EBDScreen::Splash);
	if (UBDFadeWidget* Overlay = GetOrCreateFade(true))
	{
		Overlay->FadeTo(0.0f, UBDUISettings::Get().FadeSeconds, nullptr);
	}

	GetGameInstance()->GetTimerManager().SetTimer(SplashTimer, this, &UBDUISubsystem::FinishSplash,
		FMath::Max(0.01f, UBDUISettings::Get().SplashSeconds), false);
}

void UBDUISubsystem::FinishSplash()
{
	if (CurrentScreen != EBDScreen::Splash)
	{
		return;
	}

	GetGameInstance()->GetTimerManager().ClearTimer(SplashTimer);
	TransitionTo(EBDScreen::Loading);

	// The loading screen is a placeholder: it stays its minimum and gives way to the menu.
	// Real loading would end it from a callback instead of a timer.
	const float Seconds = UBDUISettings::Get().LoadingMinSeconds + 2.0f * UBDUISettings::Get().FadeSeconds;
	GetGameInstance()->GetTimerManager().SetTimer(LoadingTimer, this, &UBDUISubsystem::HandleLoadingDone, FMath::Max(0.01f, Seconds), false);
}

void UBDUISubsystem::HandleLoadingDone()
{
	if (CurrentScreen == EBDScreen::Loading)
	{
		TransitionTo(EBDScreen::MainMenu);
	}
}

void UBDUISubsystem::OpenDifficultySelect()
{
	if (CurrentScreen == EBDScreen::MainMenu)
	{
		TransitionTo(EBDScreen::DifficultySelect);
	}
}

void UBDUISubsystem::CloseDifficultySelect()
{
	if (CurrentScreen == EBDScreen::DifficultySelect)
	{
		TransitionTo(EBDScreen::MainMenu);
	}
}

void UBDUISubsystem::PlayGame(const EBDDifficulty Difficulty)
{
	ChosenDifficulty = Difficulty;
	bLoadPending = false;
	UE_LOG(LogBDUI, Log, TEXT("Play on %s."), *StaticEnum<EBDDifficulty>()->GetNameStringByValue(static_cast<int64>(Difficulty)));
	OpenGameLevel();
}

void UBDUISubsystem::ContinueGame()
{
	ChosenDifficulty.Reset();
	bLoadPending = true;
	OpenGameLevel();
}

TOptional<EBDDifficulty> UBDUISubsystem::TakeChosenDifficulty()
{
	const TOptional<EBDDifficulty> Taken = ChosenDifficulty;
	ChosenDifficulty.Reset();
	return Taken;
}

void UBDUISubsystem::OpenGameLevel()
{
	const UBDUISettings& Settings = UBDUISettings::Get();
	if (Settings.GameMap.IsNull())
	{
		UE_LOG(LogBDUI, Error, TEXT("Play: no game map set in Project Settings > Brazil Defense - Interface."));
		return;
	}

	CloseOptions();
	ClosePauseMenu();
	bGamePending = true;

	UBDFadeWidget* Overlay = GetOrCreateFade(false);
	TWeakObjectPtr<UBDUISubsystem> WeakThis(this);
	const auto Open = [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			UE_LOG(LogBDUI, Log, TEXT("Play: opening %s."), *UBDUISettings::Get().GameMap.GetAssetName());
			UGameplayStatics::OpenLevelBySoftObjectPtr(WeakThis.Get(), UBDUISettings::Get().GameMap);
		}
	};

	if (Overlay != nullptr)
	{
		Overlay->FadeTo(1.0f, Settings.FadeSeconds, Open);
	}
	else
	{
		Open();
	}
}

void UBDUISubsystem::ReturnToMenu()
{
	const UBDUISettings& Settings = UBDUISettings::Get();
	if (Settings.MenuMap.IsNull())
	{
		UE_LOG(LogBDUI, Error, TEXT("No menu map set in Project Settings > Brazil Defense - Interface."));
		return;
	}

	CloseOptions();
	ClosePauseMenu();
	bGamePending = false;

	UBDFadeWidget* Overlay = GetOrCreateFade(false);
	TWeakObjectPtr<UBDUISubsystem> WeakThis(this);
	const auto Open = [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			UGameplayStatics::OpenLevelBySoftObjectPtr(WeakThis.Get(), UBDUISettings::Get().MenuMap);
		}
	};

	if (Overlay != nullptr)
	{
		Overlay->FadeTo(1.0f, Settings.FadeSeconds, Open);
	}
	else
	{
		Open();
	}
}

void UBDUISubsystem::RedirectToFrontEnd()
{
	const UBDUISettings& Settings = UBDUISettings::Get();
	if (Settings.MenuMap.IsNull())
	{
		UE_LOG(LogBDUI, Error, TEXT("No menu map set in Project Settings > Brazil Defense - Interface; the game level stays."));
		return;
	}

	UE_LOG(LogBDUI, Log, TEXT("Game level came up before the front end: opening %s first."), *Settings.MenuMap.GetAssetName());
	UGameplayStatics::OpenLevelBySoftObjectPtr(this, Settings.MenuMap);
}

void UBDUISubsystem::HandlePreLoadMap(const FString& MapName)
{
	// The widgets of this world go with it. The next world starts under black; its game
	// mode decides what comes up, and lifts it.
	GetGameInstance()->GetTimerManager().ClearTimer(SplashTimer);
	GetGameInstance()->GetTimerManager().ClearTimer(LoadingTimer);
	CurrentWidget = nullptr;
	Options = nullptr;
	PauseMenu = nullptr;
	Fade = nullptr;
	CurrentScreen = EBDScreen::None;
}

//~ In-game menu ----------------------------------------------------------------

void UBDUISubsystem::OpenPauseMenu()
{
	using namespace BDUIPrivate;

	if (PauseMenu != nullptr || CurrentScreen != EBDScreen::HUD)
	{
		return;
	}

	APlayerController* Controller = GetLocalController();
	if (Controller == nullptr)
	{
		return;
	}

	PauseMenu = CreateWidget<UBDPauseWidget>(Controller, UBDPauseWidget::StaticClass());
	if (PauseMenu == nullptr)
	{
		return;
	}

	PauseMenu->AddToViewport(PauseZ);
	bResumeOnMenuClose = !UGameplayStatics::IsGamePaused(Controller);
	UGameplayStatics::SetGamePaused(Controller, true);

	FInputModeUIOnly Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetWidgetToFocus(PauseMenu->TakeWidget());
	Controller->SetInputMode(Mode);
	Controller->bShowMouseCursor = true;

	UE_LOG(LogBDUI, Log, TEXT("Game menu opened, match paused."));
}

void UBDUISubsystem::ClosePauseMenu()
{
	if (PauseMenu == nullptr)
	{
		return;
	}

	CloseOptions();
	PauseMenu->RemoveFromParent();
	PauseMenu = nullptr;

	// Opened over a gameplay pause, it closes back onto the frozen board.
	APlayerController* Controller = GetLocalController();
	if (Controller != nullptr && bResumeOnMenuClose)
	{
		UGameplayStatics::SetGamePaused(Controller, false);
	}
	ApplyInputMode(CurrentScreen);

	UE_LOG(LogBDUI, Log, TEXT("Game menu closed, match %s."), bResumeOnMenuClose ? TEXT("resumed") : TEXT("still paused"));
	bResumeOnMenuClose = true;
}

//~ Gameplay pause --------------------------------------------------------------

void UBDUISubsystem::SetGameplayPaused(const bool bPaused)
{
	// Only over the bare HUD: the menu owns the pause while it is up.
	APlayerController* Controller = GetLocalController();
	if (Controller == nullptr || PauseMenu != nullptr || CurrentScreen != EBDScreen::HUD)
	{
		return;
	}
	if (UGameplayStatics::IsGamePaused(Controller) == bPaused)
	{
		return;
	}

	UGameplayStatics::SetGamePaused(Controller, bPaused);
	UE_LOG(LogBDUI, Log, TEXT("Gameplay %s."), bPaused ? TEXT("paused: the board is frozen, the HUD and the gesture stay live") : TEXT("resumed"));
}

void UBDUISubsystem::ToggleGameplayPause()
{
	SetGameplayPaused(!IsGameplayPaused());
}

bool UBDUISubsystem::IsGameplayPaused() const
{
	const APlayerController* Controller = GetLocalController();
	return Controller != nullptr && PauseMenu == nullptr && UGameplayStatics::IsGamePaused(Controller);
}

void UBDUISubsystem::TogglePauseMenu()
{
	if (PauseMenu != nullptr)
	{
		ClosePauseMenu();
	}
	else
	{
		OpenPauseMenu();
	}
}

void UBDUISubsystem::ShowHUD()
{
	bGamePending = false;
	SetScreen(EBDScreen::HUD);

	// The match exists by now: the game mode calls this after spawning it.
	if (bLoadPending)
	{
		bLoadPending = false;
		if (ABDMatchManager* Match = ABDMatchManager::Get(GetLocalController()))
		{
			Match->LoadMatch();
		}
	}

	if (UBDFadeWidget* Overlay = GetOrCreateFade(true))
	{
		Overlay->FadeTo(0.0f, UBDUISettings::Get().FadeSeconds, nullptr);
	}
}

void UBDUISubsystem::QuitGame()
{
	UE_LOG(LogBDUI, Log, TEXT("Quit from the menu."));
	UKismetSystemLibrary::QuitGame(GetGameInstance(), GetLocalController(), EQuitPreference::Quit, /*bIgnorePlatformRestrictions*/ false);
}

//~ Options -----------------------------------------------------------------------

void UBDUISubsystem::OpenOptions()
{
	using namespace BDUIPrivate;

	if (Options != nullptr)
	{
		return;
	}

	APlayerController* Controller = GetLocalController();
	if (Controller == nullptr)
	{
		return;
	}

	Options = CreateWidget<UBDOptionsWidget>(Controller, UBDOptionsWidget::StaticClass());
	if (Options != nullptr)
	{
		Options->AddToViewport(OptionsZ);
		UE_LOG(LogBDUI, Log, TEXT("Options opened."));
	}
}

void UBDUISubsystem::CloseOptions()
{
	if (Options == nullptr)
	{
		return;
	}

	Options->RemoveFromParent();
	Options = nullptr;
	if (PauseMenu == nullptr)
	{
		ApplyInputMode(CurrentScreen);
	}
	UE_LOG(LogBDUI, Log, TEXT("Options closed."));
}

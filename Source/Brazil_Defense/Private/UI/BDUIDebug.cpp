// Brazil Defense. Console access to the interface, for driving it without a mouse.

#include "BDLog.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "UI/BDLocalization.h"
#include "UI/BDSettingsSubsystem.h"
#include "UI/BDUISubsystem.h"
#include "TimerManager.h"
#include "UnrealClient.h"

namespace BDUIDebug
{
	template <typename TSubsystem>
	static TSubsystem* Find(const UWorld* World)
	{
		const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
		TSubsystem* Subsystem = GameInstance != nullptr ? GameInstance->GetSubsystem<TSubsystem>() : nullptr;
		if (Subsystem == nullptr)
		{
			UE_LOG(LogBDUI, Error, TEXT("This command needs a running game instance."));
		}
		return Subsystem;
	}

	static void ExecLanguage(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != 1)
		{
			UE_LOG(LogBDUI, Error, TEXT("Usage: BD.UI.Language <en|pt>"));
			return;
		}

		UBDSettingsSubsystem* Settings = Find<UBDSettingsSubsystem>(World);
		if (Settings == nullptr)
		{
			return;
		}

		const EBDLanguage Language = Args[0].StartsWith(TEXT("pt"), ESearchCase::IgnoreCase) ? EBDLanguage::Portuguese : EBDLanguage::English;
		Settings->SetLanguage(Language);
		UE_LOG(LogBDUI, Log, TEXT("BD.UI.Language: %s. Sample: Menu.Play = \"%s\", HUD.Score.Blue = \"%s\"."),
			BDLoc::GetCultureCode(Language), *BDLoc::Text(TEXT("Menu.Play")).ToString(), *BDLoc::Text(TEXT("HUD.Score.Blue")).ToString());
	}

	static void ExecOptions(const TArray<FString>& Args, UWorld* World)
	{
		if (UBDUISubsystem* UI = Find<UBDUISubsystem>(World))
		{
			if (UI->IsOptionsOpen())
			{
				UI->CloseOptions();
			}
			else
			{
				UI->OpenOptions();
			}
		}
	}

	static void ExecPause(const TArray<FString>& Args, UWorld* World)
	{
		if (UBDUISubsystem* UI = Find<UBDUISubsystem>(World))
		{
			UI->TogglePauseMenu();
		}
	}

	static void ExecGamePause(const TArray<FString>& Args, UWorld* World)
	{
		if (UBDUISubsystem* UI = Find<UBDUISubsystem>(World))
		{
			if (Args.Num() > 0)
			{
				UI->SetGameplayPaused(FCString::Atoi(*Args[0]) != 0);
			}
			else
			{
				UI->ToggleGameplayPause();
			}
			UE_LOG(LogBDUI, Log, TEXT("BD.UI.GamePause: the board is %s."), UI->IsGameplayPaused() ? TEXT("paused") : TEXT("running"));
		}
	}

	static void ExecPlay(const TArray<FString>& Args, UWorld* World)
	{
		UBDUISubsystem* UI = Find<UBDUISubsystem>(World);
		if (UI == nullptr)
		{
			return;
		}

		// With a difficulty named it plays straight away, as the difficulty screen would;
		// without one it does what the Play button does and opens that screen.
		const int64 Value = Args.Num() == 1 ? StaticEnum<EBDDifficulty>()->GetValueByNameString(Args[0]) : INDEX_NONE;
		if (Value != INDEX_NONE && Value < static_cast<int64>(EBDDifficulty::Count))
		{
			UI->PlayGame(static_cast<EBDDifficulty>(Value));
		}
		else
		{
			UI->OpenDifficultySelect();
		}
	}

	/** A screenshot after a delay, so a headless launch can capture the HUD once it is up. */
	static void ExecShot(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			return;
		}
		const float Seconds = Args.Num() >= 1 ? FCString::Atof(*Args[0]) : 2.0f;
		const FString Name = Args.Num() >= 2 ? Args[1] : TEXT("BDHUD");
		// 0 shoots this frame; a negative delay counts real seconds, which run on a paused
		// board where no game timer does.
		if (Seconds <= 0.0f)
		{
			if (Seconds == 0.0f)
			{
				FScreenshotRequest::RequestScreenshot(Name, /*bShowUI*/ true, /*bAddFilenameSuffix*/ true);
				return;
			}
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Name](float)
			{
				FScreenshotRequest::RequestScreenshot(Name, /*bShowUI*/ true, /*bAddFilenameSuffix*/ true);
				return false;
			}), -Seconds);
			return;
		}
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, [Name]()
		{
			FScreenshotRequest::RequestScreenshot(Name, /*bShowUI*/ true, /*bAddFilenameSuffix*/ true);
		}, FMath::Max(0.01f, Seconds), false);
	}

	static void ExecMenu(const TArray<FString>& Args, UWorld* World)
	{
		if (UBDUISubsystem* UI = Find<UBDUISubsystem>(World))
		{
			UI->ReturnToMenu();
		}
	}

	static void ExecStatus(const TArray<FString>& Args, UWorld* World)
	{
		const UBDUISubsystem* UI = Find<UBDUISubsystem>(World);
		const UBDSettingsSubsystem* Settings = Find<UBDSettingsSubsystem>(World);
		if (UI == nullptr || Settings == nullptr)
		{
			return;
		}

		const UBDSettingsSave& Saved = Settings->Get();
		UE_LOG(LogBDUI, Log, TEXT("BD.UI.Status: screen %s, options %s, game menu %s | quality %d, %dx%d, window %d, vsync %s | music %d, effects %d, muted %s | language %s."),
			*StaticEnum<EBDScreen>()->GetNameStringByValue(static_cast<int64>(UI->GetCurrentScreen())),
			UI->IsOptionsOpen() ? TEXT("open") : TEXT("closed"),
			UI->IsPauseMenuOpen() ? TEXT("open") : TEXT("closed"),
			Saved.QualityLevel, Saved.Resolution.X, Saved.Resolution.Y, static_cast<int32>(Saved.WindowMode), Saved.bVSync ? TEXT("on") : TEXT("off"),
			Saved.MusicVolume, Saved.EffectsVolume, Saved.bMuted ? TEXT("yes") : TEXT("no"),
			BDLoc::GetCultureCode(Saved.Language));
	}

	static void ExecVolume(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != 2)
		{
			UE_LOG(LogBDUI, Error, TEXT("Usage: BD.UI.Volume <music|effects|mute> <0..100 | 0|1>"));
			return;
		}

		UBDSettingsSubsystem* Settings = Find<UBDSettingsSubsystem>(World);
		if (Settings == nullptr)
		{
			return;
		}

		const int32 Value = FCString::Atoi(*Args[1]);
		if (Args[0].Equals(TEXT("music"), ESearchCase::IgnoreCase))
		{
			Settings->SetMusicVolume(Value);
		}
		else if (Args[0].Equals(TEXT("effects"), ESearchCase::IgnoreCase))
		{
			Settings->SetEffectsVolume(Value);
		}
		else
		{
			Settings->SetMuted(Value != 0);
		}
		ExecStatus(Args, World);
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdLanguage(
		TEXT("BD.UI.Language"),
		TEXT("BD.UI.Language <en|pt>: switches the interface language, saved like the options panel does."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecLanguage));

	static FAutoConsoleCommandWithWorldAndArgs CmdOptions(
		TEXT("BD.UI.Options"),
		TEXT("BD.UI.Options: opens the options panel, or closes it when open."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecOptions));

	static FAutoConsoleCommandWithWorldAndArgs CmdPause(
		TEXT("BD.UI.Pause"),
		TEXT("BD.UI.Pause: opens the in-game menu (match paused), or closes it when open. Escape with nothing in hand does the same."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecPause));

	/** Runs a console command after a delay: the one way a launch line can act on a running match. */
	static void ExecDelay(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || Args.Num() < 2)
		{
			UE_LOG(LogBDUI, Error, TEXT("Usage: BD.Delay <seconds> <console command...>"));
			return;
		}
		const float Seconds = FCString::Atof(*Args[0]);
		FString Command;
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			Command += (Index > 1 ? TEXT(" ") : TEXT("")) + Args[Index];
		}
		FTimerHandle Handle;
		TWeakObjectPtr<UWorld> WeakWorld(World);
		World->GetTimerManager().SetTimer(Handle, [WeakWorld, Command]()
		{
			if (UWorld* Target = WeakWorld.Get())
			{
				UE_LOG(LogBDUI, Log, TEXT("BD.Delay runs: %s"), *Command);
				GEngine->Exec(Target, *Command);
			}
		}, FMath::Max(0.01f, Seconds), false);
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdGamePause(
		TEXT("BD.UI.GamePause"),
		TEXT("BD.UI.GamePause [0|1]: the gameplay pause of the HUD button and P, toggled or set. The HUD stays up, the board freezes."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecGamePause));

	static FAutoConsoleCommandWithWorldAndArgs CmdDelay(
		TEXT("BD.Delay"),
		TEXT("BD.Delay <seconds> <command...>: runs a console command after a delay, in game seconds."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecDelay));

	static FAutoConsoleCommandWithWorldAndArgs CmdShot(
		TEXT("BD.UI.Shot"),
		TEXT("BD.UI.Shot [seconds] [name]: takes a screenshot with the UI after a delay."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecShot));

	static FAutoConsoleCommandWithWorldAndArgs CmdPlay(
		TEXT("BD.UI.Play"),
		TEXT("BD.UI.Play [Easy|Normal|Hard]: what the Play button does (the difficulty screen), or straight into the game on a difficulty."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecPlay));

	static FAutoConsoleCommandWithWorldAndArgs CmdMenu(
		TEXT("BD.UI.Menu"),
		TEXT("BD.UI.Menu: fades out and returns to the menu level."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecMenu));

	static FAutoConsoleCommandWithWorldAndArgs CmdVolume(
		TEXT("BD.UI.Volume"),
		TEXT("BD.UI.Volume <music|effects|mute> <value>: sets a volume or the mute, saved like the options panel does."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecVolume));

	static FAutoConsoleCommandWithWorldAndArgs CmdStatus(
		TEXT("BD.UI.Status"),
		TEXT("BD.UI.Status: logs the current screen and the saved settings."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));
}

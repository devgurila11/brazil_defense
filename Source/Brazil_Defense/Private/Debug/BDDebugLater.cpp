// Brazil Defense. Tooling: a console command run some seconds from now.

#include "BDLog.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace BDDebugLater
{
	/**
	 * -ExecCmds runs everything on the first frame, before the world has streamed in or a
	 * frame has been drawn. A screenshot taken there shows nothing of what the commands
	 * just built; this waits for the world to look like itself:
	 *
	 *   -ExecCmds="BD.Place.At 22 14, BD.Debug.Later 8 Shot, BD.Debug.Later 10 quit"
	 */
	static void ExecLater(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || Args.Num() < 2)
		{
			UE_LOG(LogBDDebug, Error, TEXT("Usage: BD.Debug.Later <seconds> <command...>."));
			return;
		}

		const float Seconds = FMath::Max(0.0f, FCString::Atof(*Args[0]));
		const FString Command = FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" "));
		const TWeakObjectPtr<UWorld> WeakWorld = World;

		// Real time on the core ticker, so a frozen or dilated game clock does not hold it.
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld, Command](float)
		{
			UWorld* Target = WeakWorld.Get();
			APlayerController* Controller = Target != nullptr ? Target->GetFirstPlayerController() : nullptr;
			if (Controller != nullptr)
			{
				UE_LOG(LogBDDebug, Log, TEXT("BD.Debug.Later: running '%s'."), *Command);
				Controller->ConsoleCommand(Command);
			}
			return false;
		}), Seconds);

		UE_LOG(LogBDDebug, Log, TEXT("BD.Debug.Later: '%s' in %.1f s."), *Command, Seconds);
	}

	static FAutoConsoleCommandWithWorldAndArgs LaterCommand(
		TEXT("BD.Debug.Later"),
		TEXT("BD.Debug.Later <seconds> <command...>: runs a console command that many real seconds from now."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecLater));
}

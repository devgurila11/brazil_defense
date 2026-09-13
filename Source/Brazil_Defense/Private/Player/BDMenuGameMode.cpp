// Brazil Defense. The game mode of the menu level: no match, only the front end.

#include "Player/BDMenuGameMode.h"

#include "BDLog.h"
#include "Engine/GameInstance.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/SpectatorPawn.h"
#include "UI/BDUISubsystem.h"

ABDMenuPlayerController::ABDMenuPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
}

ABDMenuGameMode::ABDMenuGameMode()
{
	PlayerControllerClass = ABDMenuPlayerController::StaticClass();
	// Nothing to walk around in: no pawn at all.
	DefaultPawnClass = nullptr;
}

void ABDMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	UBDUISubsystem* UI = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UBDUISubsystem>() : nullptr;
	if (UI == nullptr)
	{
		UE_LOG(LogBDUI, Error, TEXT("Menu level has no UI subsystem to start the front end."));
		return;
	}

	UI->StartFrontEnd();
}

// Brazil Defense. Game mode wiring the player controller and the match in.

#include "Player/BDGameMode.h"

#include "BDLog.h"
#include "Engine/World.h"
#include "Match/BDMatchManager.h"
#include "Player/BDPlayerController.h"

ABDGameMode::ABDGameMode()
{
	PlayerControllerClass = ABDPlayerController::StaticClass();
	MatchManagerClass = ABDMatchManager::StaticClass();
}

void ABDGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (ABDMatchManager::Get(this) != nullptr)
	{
		return;
	}

	UClass* SpawnClass = MatchManagerClass != nullptr ? MatchManagerClass.Get() : ABDMatchManager::StaticClass();
	const ABDMatchManager* Spawned = GetWorld()->SpawnActor<ABDMatchManager>(SpawnClass);

	UE_LOG(LogBDMatch, Log, TEXT("No match manager in the level, spawned %s."), *GetNameSafe(Spawned));
}

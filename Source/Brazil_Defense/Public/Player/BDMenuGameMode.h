// Brazil Defense. The game mode of the menu level: no match, only the front end.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "BDMenuGameMode.generated.h"

/**
 * A controller that does nothing but hold the cursor for the screens. The menu level
 * has no board and no gesture, so it does not carry the placement component.
 */
UCLASS()
class BRAZIL_DEFENSE_API ABDMenuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABDMenuPlayerController();
};

/**
 * Runs the front end: splash, loading, main menu. Nothing of the match is spawned
 * here; Play opens the game level, whose ABDGameMode does that.
 */
UCLASS()
class BRAZIL_DEFENSE_API ABDMenuGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABDMenuGameMode();

	virtual void BeginPlay() override;
};

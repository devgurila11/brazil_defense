// Brazil Defense. Game mode wiring the player controller and the match in.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Templates/SubclassOf.h"
#include "BDGameMode.generated.h"

class ABDMatchManager;

/**
 * Hands out ABDPlayerController and makes sure the world has a match to play.
 * Rounds and economy live on ABDMatchManager, not here.
 */
UCLASS()
class BRAZIL_DEFENSE_API ABDGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABDGameMode();

	virtual void BeginPlay() override;

	/**
	 * Spawned when the level does not already carry a match manager.
	 * Spawning rather than requiring one in the level is deliberate: a map that forgot to
	 * place it would otherwise start a match with no clock, no budget and no phase.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Brazil Defense")
	TSubclassOf<ABDMatchManager> MatchManagerClass;
};

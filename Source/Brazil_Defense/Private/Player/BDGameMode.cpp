// Brazil Defense. Game mode wiring the player controller and the match in.

#include "Player/BDGameMode.h"

#include "BDLog.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Match/BDMatchManager.h"
#include "Objective/BDObjectiveSettings.h"
#include "Objective/BDObjectiveSubsystem.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementComponent.h"
#include "Player/BDPlayerController.h"
#include "UI/BDMatchHUD.h"
#include "UI/BDUISubsystem.h"

ABDGameMode::ABDGameMode()
{
	PlayerControllerClass = ABDPlayerController::StaticClass();
	HUDClass = ABDMatchHUD::StaticClass();
	MatchManagerClass = ABDMatchManager::StaticClass();
	// The match is looked at through a fixed camera the controller sets up; nothing to fly.
	DefaultPawnClass = nullptr;
}

void ABDGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Outside the editor the game is entered through the menu, whatever level was
	// launched: a standalone or packaged run that lands here first goes back to the
	// front end. Play in Editor stays, that is what it is for.
	UBDUISubsystem* UI = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UBDUISubsystem>() : nullptr;
	if (UI != nullptr && !GIsEditor && !UI->HasSeenFrontEnd())
	{
		UI->RedirectToFrontEnd();
		return;
	}

	if (ABDMatchManager::Get(this) == nullptr)
	{
		UClass* SpawnClass = MatchManagerClass != nullptr ? MatchManagerClass.Get() : ABDMatchManager::StaticClass();
		const ABDMatchManager* Spawned = GetWorld()->SpawnActor<ABDMatchManager>(SpawnClass);

		UE_LOG(LogBDMatch, Log, TEXT("No match manager in the level, spawned %s."), *GetNameSafe(Spawned));
	}

	// The HUD goes up over the match, whether the level came from the menu or from Play in Editor.
	if (UI != nullptr)
	{
		UI->ShowHUD();
	}

	// A board with no urn starts with the urn in the player's hand: it is the first thing
	// to put down, and nothing else is accepted before it.
	const UBDObjectiveSubsystem* Objectives = UBDObjectiveSubsystem::Get(this);
	const ABDPlayerController* Controller = Cast<ABDPlayerController>(GetWorld()->GetFirstPlayerController());
	UBDPlacementComponent* Placement = Controller != nullptr ? Controller->GetPlacementComponent() : nullptr;
	if (Objectives != nullptr && !Objectives->IsPlaced() && Placement != nullptr)
	{
		if (UBDPlaceableData* Urn = UBDObjectiveSettings::Get().ObjectivePlaceable.LoadSynchronous())
		{
			Placement->SelectPlaceable(Urn);
			UE_LOG(LogBDMatch, Log, TEXT("No urn on the board: %s put in the player's hand."), *Urn->GetName());
		}
		else
		{
			UE_LOG(LogBDMatch, Warning, TEXT("No urn on the board and no Objective Placeable set in Project Settings > Brazil Defense - Objective."));
		}
	}
}

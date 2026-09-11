// Brazil Defense. Player controller that owns the placement gesture.

#include "Player/BDPlayerController.h"

#include "Placement/BDPlacementComponent.h"

ABDPlayerController::ABDPlayerController()
{
	// The whole game is played by pointing at the board.
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	PlacementComponent = CreateDefaultSubobject<UBDPlacementComponent>(TEXT("Placement"));
}

void ABDPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Bound here rather than in the component: this is the first moment InputComponent
	// is guaranteed to exist.
	if (PlacementComponent != nullptr)
	{
		PlacementComponent->BindInput(InputComponent);
	}
}

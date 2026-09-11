// Brazil Defense. Player controller that owns the placement gesture.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BDPlayerController.generated.h"

class UBDPlacementComponent;

/**
 * Hosts UBDPlacementComponent and puts the cursor on screen for it.
 * Deliberately thin: it exists so the placement component has a controller to live on.
 */
UCLASS()
class BRAZIL_DEFENSE_API ABDPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABDPlayerController();

	virtual void SetupInputComponent() override;

	UFUNCTION(BlueprintPure, Category = "Brazil Defense")
	UBDPlacementComponent* GetPlacementComponent() const { return PlacementComponent; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense")
	TObjectPtr<UBDPlacementComponent> PlacementComponent;
};

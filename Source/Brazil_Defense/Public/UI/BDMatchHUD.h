// Brazil Defense. What is drawn straight on the screen over the board: the creeps' health.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BDMatchHUD.generated.h"

/**
 * Health bars over the creeps, drawn on the canvas rather than as widgets: there can be
 * hundreds, and a bar is two rectangles. A creep shows one only once it has taken
 * damage; the bar is a fixed width in the world, so the camera's zoom sizes it, and it
 * is skipped once it would be thinner than a few pixels: the overview stays clean, the
 * close look reads every creep. The candidate keeps the bar of his own.
 */
UCLASS()
class BRAZIL_DEFENSE_API ABDMatchHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;
};

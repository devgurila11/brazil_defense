// Brazil Defense. What is drawn straight on the screen over the board: the creeps' health, the palaces' stars.

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
 *
 * Over every palace, five stars facing the camera: as many filled as its level, the
 * rest only an outline. These are sized by the screen rather than the world, so the
 * evolution reads the same at every zoom they are shown at. They fade out as the
 * camera pulls back, and are gone in the overview (UBDUISettings::PalaceStarFade*).
 *
 * Over every Agent, the creep bars' way: his patrol time emptying in blue while awake,
 * his rest filling in violet while he sleeps.
 */
UCLASS()
class BRAZIL_DEFENSE_API ABDMatchHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

private:
	void DrawCreepBars();
	void DrawPalaceStars();

	/** Over every Agent: the patrol emptying in blue while awake, the rest filling in violet while asleep. */
	void DrawAgentBars();

	/** One five-pointed star centered on a screen point, Radius from the center to a tip: filled, or only its outline. */
	void DrawStar(const FVector2D& Center, float Radius, bool bFilled, const FLinearColor& Color);
};

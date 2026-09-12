// Brazil Defense. The numbers of the match on screen, for testing, until there is a HUD.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDDebugScoreboard.generated.h"

class APlayerController;
class IConsoleVariable;
class UCanvas;
class UFont;

/**
 * Draws the scoreboard of the match in the top left corner of the viewport, every frame,
 * while BD.HUD.Debug is on: the two vote counters in their colors, the wave and the
 * countdown, the mouths of the wave and how many creeps are out. The same numbers
 * BD.Votes.Status and BD.Wave.Status log, read from the same places, so what is on
 * screen and what is in the log can be checked against each other.
 *
 * A test overlay, not the HUD: no localisation, no art, no widgets. Drawn on the canvas
 * of UDebugDrawService the way UBDGridDebugDrawer draws its labels, so it lives in every
 * game world without anything placed in the level and cannot be streamed out.
 *
 * The automatic test setup switches it on with the other debug switches.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDDebugScoreboard : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	//~ Begin UWorldSubsystem interface
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	//~ End UWorldSubsystem interface

private:
	/** Canvas pass. Draws nothing when the switch is off or the canvas shows another world. */
	void Draw(UCanvas* Canvas, APlayerController* PlayerController);

	/** One line of the overlay, shadowed so it reads over any board. @return the height taken. */
	float DrawLine(UCanvas& Canvas, const UFont& Font, float X, float Y, const FString& Text, const FLinearColor& Color) const;

	/** Handles of the canvas draw delegate, one per observed engine show flag. */
	TArray<FDelegateHandle> CanvasDrawHandles;

	/** BD.Match.FreezeTimer, looked up once: a frozen countdown is labelled as such rather than looking stuck. */
	IConsoleVariable* FreezeTimerVariable = nullptr;
};

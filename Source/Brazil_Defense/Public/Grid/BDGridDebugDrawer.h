// Brazil Defense. Grid debug drawing for the worlds that are actually played.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDGridDebugDrawer.generated.h"

class APlayerController;
class UCanvas;

/**
 * Draws the grid debug while the game is running, in PIE and in a packaged build.
 *
 * A world subsystem rather than a level actor on purpose: the maps are World Partition,
 * so an actor placed next to the world origin is simply never streamed in when the board
 * sits hundreds of metres away, and the debug silently disappears the moment you press
 * Play. A subsystem exists in every game world, needs nothing placed in the level and
 * cannot be streamed out.
 *
 * The editor viewport is drawn by ABDGridVisualizer instead, so the two never overlap.
 * All the drawing itself lives in BDGridDebug and is shared between them.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDGridDebugDrawer : public UTickableWorldSubsystem
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

	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject interface

private:
	/** Canvas pass: forwards to the shared drawing, which filters by world. */
	void DrawCoordLabels(UCanvas* Canvas, APlayerController* PlayerController);

	/** Handles of the canvas draw delegate, one per observed engine show flag. */
	TArray<FDelegateHandle> CanvasDrawHandles;
};

// Brazil Defense. Debug visualization of the logical gameplay grid, in the editor.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BDGridVisualizer.generated.h"

class APlayerController;
class UCanvas;

/**
 * Draws the logical grid owned by UBDGridSubsystem so the layout can be checked
 * against the level art. Purely a debug view: it holds no grid data and changes
 * no state. Drop one instance in the level and toggle it from Project Settings.
 *
 * Editor worlds only. A level actor cannot be trusted to draw a running game: the
 * maps are World Partition, so this one is streamed out as soon as the camera moves
 * away from it. UBDGridDebugDrawer, a world subsystem, covers the game worlds, and
 * both share the drawing in BDGridDebug.
 *
 * Lines and cell fills are drawn with the debug line batcher, coordinate labels
 * are drawn on the viewport canvas so they stay readable at any distance.
 * Editor drawing requires the viewport to be in realtime mode.
 */
UCLASS(meta = (DisplayName = "BD Grid Visualizer"))
class BRAZIL_DEFENSE_API ABDGridVisualizer : public AActor
{
	GENERATED_BODY()

public:
	ABDGridVisualizer();

	//~ Begin AActor interface
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }
	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;
	//~ End AActor interface

	/** Per instance switch, on top of the global toggles in the grid settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Grid")
	bool bDrawGrid = true;

private:
	/** True when this actor is the one responsible for drawing its world. */
	bool ShouldDraw() const;

	/** Canvas pass: forwards to the shared drawing, which filters by world. */
	void DrawCoordLabels(UCanvas* Canvas, APlayerController* PlayerController);

	void RegisterCanvasDraw();
	void UnregisterCanvasDraw();

	/** Handle of the canvas draw delegate on the editor show flag. */
	FDelegateHandle CanvasDrawHandle;
};

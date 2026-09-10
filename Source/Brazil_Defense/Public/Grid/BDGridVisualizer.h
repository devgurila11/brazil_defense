// Brazil Defense. Debug visualization of the logical gameplay grid.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BDGridVisualizer.generated.h"

class APlayerController;
class UBDGridSettings;
class UBDGridSubsystem;
class UCanvas;

/**
 * Draws the logical grid owned by UBDGridSubsystem so the layout can be checked
 * against the level art. Purely a debug view: it holds no grid data and changes
 * no state. Drop one instance in the level and toggle it from Project Settings.
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
	/** True when the current world and the settings both allow drawing. */
	bool ShouldDraw() const;

	/** Returns the grid of this actor world, or null when it is not available. */
	const UBDGridSubsystem* GetGrid() const;

	void DrawGridLines(const UBDGridSubsystem& Grid, const UBDGridSettings& Settings) const;
	void DrawCellStates(const UBDGridSubsystem& Grid, const UBDGridSettings& Settings) const;
	void DrawOriginMarker(const UBDGridSubsystem& Grid, const UBDGridSettings& Settings) const;

	/** Canvas pass: draws the coordinate label of the cells around the camera. */
	void DrawCoordLabels(UCanvas* Canvas, APlayerController* PlayerController);

	void RegisterCanvasDraw();
	void UnregisterCanvasDraw();

	/** Handles of the canvas draw delegate, one per observed engine show flag. */
	TArray<FDelegateHandle> CanvasDrawHandles;
};

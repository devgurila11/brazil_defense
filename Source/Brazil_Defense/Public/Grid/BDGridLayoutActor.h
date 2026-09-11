// Brazil Defense. The authored cell layout of a map, saved with the level.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/BDGridTypes.h"
#include "BDGridLayoutActor.generated.h"

class UBDGridSubsystem;

/**
 * Carries the layout authored for a map: where the creeps come in, where they are
 * headed, which cells are permanent scenery and which edges are permanently fenced.
 *
 * UBDGridSubsystem starts every world with all cells Free, so without something like
 * this the layout would only ever exist in the editor session that painted it and a
 * game world would come up empty. This actor is that something: it lives in the level,
 * is saved with it, and applies itself to the grid of whatever world it is in.
 *
 * Never spatially loaded. Under World Partition a streamed actor is one that may simply
 * not be there when the match starts, and a layout that arrives late, or never, is worse
 * than no layout at all.
 */
UCLASS(meta = (DisplayName = "BD Grid Layout"))
class BRAZIL_DEFENSE_API ABDGridLayoutActor : public AActor
{
	GENERATED_BODY()

public:
	ABDGridLayoutActor();

	//~ Begin AActor interface
	virtual void PostRegisterAllComponents() override;
	virtual void PostUnregisterAllComponents() override;
#if WITH_EDITOR
	/** The whole point of this actor is to always be loaded, so the flag is not editable. */
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
#endif
	//~ End AActor interface

	/** Writes the authored cells and edges onto the grid of this actor world. @return cells plus edges changed. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Grid")
	int32 ApplyToGrid();

	const TMap<FBDCellCoord, EBDCellState>& GetAuthoredCells() const { return AuthoredCells; }
	const TArray<FBDEdgeCoord>& GetAuthoredEdges() const { return AuthoredEdges; }

#if WITH_EDITOR
	/**
	 * Replaces the authored layout with every non Free cell of a grid, except the cells
	 * the obstacle generator currently owns: those belong to a seed, not to the map.
	 * Blocked edges are captured alongside.
	 * Editor only: this is the save half of the paint in the console, save, ship it loop.
	 * @return number of cells plus edges captured.
	 */
	int32 CaptureFromGrid(const UBDGridSubsystem& Grid);
#endif

private:
	UBDGridSubsystem* GetGrid() const;

	/** Re-applies the layout after the grid was resized or reset under us. */
	void HandleGridRebuilt();

	/**
	 * The authored cells, by coordinate. Only cells that are not Free are stored: Free is
	 * the state every cell already starts in, so listing them would be noise in the details
	 * panel and in the diff of the level.
	 */
	UPROPERTY(EditAnywhere, Category = "Brazil Defense|Grid")
	TMap<FBDCellCoord, EBDCellState> AuthoredCells;

	/** Edges fenced off as part of the map. Only blocked edges are listed, for the same reason as the cells. */
	UPROPERTY(EditAnywhere, Category = "Brazil Defense|Grid")
	TArray<FBDEdgeCoord> AuthoredEdges;

	FDelegateHandle GridRebuiltHandle;
};

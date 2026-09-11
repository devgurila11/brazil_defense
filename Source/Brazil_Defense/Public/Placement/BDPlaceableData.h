// Brazil Defense. Definition of something the player can put on the board.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Grid/BDGridTypes.h"
#include "BDPlaceableData.generated.h"

class UStaticMesh;

/** Local axis of a mesh. */
UENUM(BlueprintType)
enum class EBDPlaceableAxis : uint8
{
	X UMETA(DisplayName = "X (forward)"),
	Y UMETA(DisplayName = "Y (right)")
};

/**
 * One placeable piece: a divider, a platform or a tower. Describes what it costs,
 * how much of the board it takes and what it leaves behind, so the placement flow
 * never needs to know which kind of piece it is handling.
 *
 * A piece takes either cells or edges. A platform or a tower covers a rectangle of
 * cells and writes OccupiesAs on them. A divider is a fence: it sits on the boundary
 * between two cells, blocks the crossing and never writes a cell at all.
 */
UCLASS(BlueprintType)
class BRAZIL_DEFENSE_API UBDPlaceableData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Asset type used to discover every placeable through the asset manager. */
	static const FPrimaryAssetType PlaceableAssetType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placeable")
	FText DisplayName;

	/** Actor spawned once the piece is placed. Loaded on demand, not on startup. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placeable")
	TSoftClassPtr<AActor> ActorClass;

	/**
	 * Mesh shown by the ghost preview while the piece is being positioned.
	 * Optional: without it the preview falls back to the footprint outline alone.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placeable")
	TSoftObjectPtr<UStaticMesh> PreviewMesh;

	/** Price of the piece. Stored only: nothing spends it yet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placeable", meta = (ClampMin = "0"))
	int32 Cost = 0;

	//~ Cell pieces ----------------------------------------------------------

	/**
	 * The piece is a fence on the boundary between cells rather than a thing standing
	 * on them. True only for the divider. Everything under "Cell" is then ignored and
	 * everything under "Edge" applies.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Placeable")
	bool bOccupiesEdge = false;

	/** Grid rectangle the piece takes, growing from its origin cell towards +X and +Y. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cell", meta = (ClampMin = "1", EditCondition = "!bOccupiesEdge"))
	FIntPoint Footprint = FIntPoint(1, 1);

	/** State written on the covered cells. Only Platform and Tower make sense here. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cell", meta = (EditCondition = "!bOccupiesEdge"))
	EBDCellState OccupiesAs = EBDCellState::Platform;

	//~ Edge pieces ----------------------------------------------------------

	/** How many edges in a straight line one click places. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Edge", meta = (ClampMin = "1", EditCondition = "bOccupiesEdge"))
	int32 SegmentLength = 1;

	/**
	 * Copies of ActorClass spawned on each edge, spread evenly along it so that together
	 * they span the cell side. The divider mesh is half a cell long, so it asks for 2 and
	 * gets one copy at -1/4 and one at +1/4 of the edge.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Edge", meta = (ClampMin = "1", EditCondition = "bOccupiesEdge"))
	int32 InstancesPerEdge = 1;

	/** Local axis of the mesh that runs along the fence. The actor is turned so this axis lies on the edge. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Edge", meta = (EditCondition = "bOccupiesEdge"))
	EBDPlaceableAxis EdgeAxis = EBDPlaceableAxis::X;

	//~ Begin UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UPrimaryDataAsset interface

	/** What the match budgets and removal rules see this piece as. */
	UFUNCTION(BlueprintPure, Category = "Placeable")
	EBDPieceKind GetPieceKind() const;

	/** Whether the piece obstructs movement once placed, which is what makes a blocking check worth running. */
	UFUNCTION(BlueprintPure, Category = "Placeable")
	bool BlocksMovement() const;

	/** Whether the piece is set up in a way the placement flow can act on. Logs what is wrong when not. */
	bool IsValidSetup(FString& OutError) const;

	/**
	 * Yaw that lays EdgeAxis along an edge of the given direction. A +X edge is a line
	 * along world Y, a +Y edge a line along world X.
	 */
	float GetYawForEdge(uint8 EdgeDirection) const;

	/**
	 * Where every copy of an edge piece goes on one edge, relative to the middle of that
	 * edge and already turned to lie along it. One entry when InstancesPerEdge is 1.
	 */
	void GetEdgeInstanceTransforms(uint8 EdgeDirection, float CellSize, TArray<FTransform>& OutTransforms) const;
};

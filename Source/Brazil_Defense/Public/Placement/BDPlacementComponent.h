// Brazil Defense. The gesture of putting a piece on the board and taking it back.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Grid/BDGridTypes.h"
#include "BDPlacementComponent.generated.h"

class ABDMatchManager;
class ABDPlacementPreview;
class APlayerController;
class UBDGridSubsystem;
class UBDPathfinder;
class UBDPlaceableData;
class UInputComponent;
struct FInputActionValue;

/** A piece standing on the board, remembered so it can be taken back. */
USTRUCT()
struct FBDPlacedPiece
{
	GENERATED_BODY()

	/** Everything spawned for the piece: one actor, or one per copy of a fence. */
	UPROPERTY()
	TArray<TObjectPtr<AActor>> Actors;

	UPROPERTY()
	TObjectPtr<const UBDPlaceableData> Data = nullptr;

	/** Cell pieces: origin and footprint as placed, axes already swapped for a 90 or 270 turn. */
	UPROPERTY()
	FBDCellCoord Origin;

	UPROPERTY()
	FIntPoint Footprint = FIntPoint(1, 1);

	/** Edge pieces: every edge of the segment, which is what removal frees. */
	UPROPERTY()
	TArray<FBDEdgeCoord> Edges;

	/** Yaw the actors were spawned with, so a saved or rebuilt board comes back facing the same way. */
	UPROPERTY()
	float Yaw = 0.0f;
};

/**
 * Why the hovered spot is refused, so a red ghost can be told apart from a hover that
 * never found the board. Logged whenever it changes.
 */
UENUM(BlueprintType)
enum class EBDPlacementRefusal : uint8
{
	/** The spot is legal. */
	None,
	/** Nothing is selected. */
	NoSelection,
	/** The cursor is not over the grid plane, or the board has no grid. */
	NotHoveringGrid,
	/** The match will not take this kind of piece now: no budget left, or the phase forbids it. */
	MatchRefused,
	/** Part of the footprint or the segment lies outside the grid. */
	OffGrid,
	/** A cell of the footprint is not Free. */
	CellTaken,
	/** An edge of the segment is on the outer border. */
	EdgeOnBorder,
	/** An edge of the segment already carries a fence. */
	EdgeTaken,
	/** Some spawn would lose every way to the goal. */
	WouldBlockPath
};

/** Broadcast whenever the hovered cell or its validity changes. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBDOnHoverChanged, const FBDCellCoord& /*Cell*/, bool /*bValid*/);

/**
 * Lives on the player controller and turns mouse movement into a grid coordinate,
 * a legality answer and, on click, a piece on the board.
 *
 * Two kinds of piece go through here. A cell piece (platform, tower) hovers a cell
 * and covers a footprint of cells. An edge piece (divider) hovers the boundary
 * nearest the cursor and fences off a run of edges. The grid tells them apart; the
 * gesture is the same.
 *
 * Nothing here spends money or counts waves: this is only the gesture.
 */
UCLASS(ClassGroup = (BrazilDefense), meta = (BlueprintSpawnableComponent, DisplayName = "BD Placement"))
class BRAZIL_DEFENSE_API UBDPlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBDPlacementComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent interface

	/** Binds left click to place and right click to remove. Called by the owning controller. */
	void BindInput(UInputComponent* InputComponent);

	//~ Selection ------------------------------------------------------------

	/** Enters placement mode with a piece. Passing null is the same as CancelSelection. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Placement")
	void SelectPlaceable(UBDPlaceableData* Placeable);

	/** Leaves placement mode and hides the ghost. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Placement")
	void CancelSelection();

	/** Places the selected piece on the hovered cell or edge. @return false when the spot is refused. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Placement")
	bool TryPlaceAtHovered();

	/** Takes back whatever the player put under the cursor. @return false when there is nothing of theirs there. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Placement")
	bool TryRemoveAtHovered();

	/**
	 * Turns the held piece a quarter turn, forwards or backwards, wrapping around.
	 * A cell piece has four facings: the footprint swaps its axes at 90 and 270, so a
	 * 2x1 truck becomes 1x2; at 180 it takes the same cells as at 0 but faces the other
	 * way. A fence has two: it runs along X or along Y, and has no front or back.
	 */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Placement")
	void RotateSelection(bool bClockwise = true);

	/** Footprint actually being validated, with the current rotation applied. Cell pieces only. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Placement")
	FIntPoint GetEffectiveFootprint() const;

	/** Yaw a cell piece placed right now would be spawned with: 0, 90, 180 or 270. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Placement")
	float GetPlacementYaw() const;

	/** Whether the current turn swaps the axes: a cell piece at 90 or 270, or a fence running along X. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Placement")
	bool IsSelectionRotated() const { return (RotationSteps % 2) != 0; }

	/** Direction of the edges a fence placed right now would take: +X edges line up along Y, +Y edges along X. */
	uint8 GetSegmentDirection() const;

	//~ State ----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Placement")
	UBDPlaceableData* GetCurrentSelection() const { return CurrentSelection; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Placement")
	FBDCellCoord GetHoveredCell() const { return HoveredCell; }

	/** The boundary nearest the cursor inside the hovered cell. Meaningful only while hovering the grid. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Placement")
	FBDEdgeCoord GetHoveredEdge() const { return HoveredEdge; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Placement")
	bool IsCurrentPlacementValid() const { return bCurrentPlacementValid; }

	/** Why the current placement is refused. None while it is valid. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Placement")
	EBDPlacementRefusal GetCurrentRefusal() const { return CurrentRefusal; }

	/** The refusal as text, with what the match had to say when it is the one refusing. */
	FString DescribeCurrentRefusal() const;

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Placement")
	bool IsHoveringGrid() const { return bHoveringGrid; }

	/** The edges the selected fence would take at the hovered edge. Empty for cell pieces. */
	void GetSelectionSegmentEdges(TArray<FBDEdgeCoord>& OutEdges) const;

	/**
	 * Points the hover at a cell without a mouse, as if the cursor were on its center.
	 * The console placement commands use this; the real gesture goes through the cursor.
	 */
	void SetHoveredCellDirect(FBDCellCoord Coord);

	/** Points the hover at an edge without a mouse, as if the cursor were on its middle. */
	void SetHoveredEdgeDirect(FBDEdgeCoord Edge);

	FBDOnHoverChanged OnHoverChanged;

private:
	APlayerController* GetOwningController() const;
	UBDGridSubsystem* GetGrid() const;

	/**
	 * The match that owns the budgets, or null when there is none.
	 * Without a match every placement is free: the console tools and any test map that
	 * never starts one still have to work.
	 */
	ABDMatchManager* GetMatch() const;

	const UBDPathfinder* GetPathfinder() const;

	bool IsEdgeSelection() const;

	/** Projects the mouse onto the grid plane. @return false when it misses the board. */
	bool TraceGridPlane(FVector& OutHitPoint) const;

	/** Turns a point on the grid plane into the hovered cell and the hovered edge. */
	void ResolveHover(const FVector& PlanePoint);

	/**
	 * The edge of Cell whose middle is nearest to Point. With a fence in hand only the
	 * two edges of the fence's direction compete, so the wheel decides the axis and the
	 * cursor decides the side; otherwise all four do.
	 */
	FBDEdgeCoord PickNearestEdge(const UBDGridSubsystem& Grid, const FBDCellCoord& Cell, const FVector& Point) const;

	/** Recomputes bCurrentPlacementValid and CurrentRefusal for the current selection and hovered cell or edge. */
	void EvaluatePlacement();
	EBDPlacementRefusal EvaluateCellPlacement(const UBDGridSubsystem& Grid) const;
	EBDPlacementRefusal EvaluateEdgePlacement(const UBDGridSubsystem& Grid) const;

	/** Logs the refusal when it differs from the last one logged, so a hover does not spam. */
	void ReportRefusalChange();

	/** Fills OutCells with the footprint of the current selection anchored at Origin. */
	void GetFootprintCells(FBDCellCoord Origin, TArray<FBDCellCoord>& OutCells) const;

	/**
	 * World transform of every actor the selection would spawn where it hovers, each
	 * standing on the floor under it. The ghost and the spawn both take these, so what
	 * is shown is what gets placed.
	 */
	void BuildInstanceTransforms(TArray<FTransform>& OutTransforms) const;

	/** The box the debug outline draws around the hovered spot: a footprint of cells or a strip along a fence. */
	void ComputeOutline(FVector& OutCenter, FVector& OutExtent) const;

	/** Z of the floor under a point, traced downwards. Falls back to the grid plane. */
	float ResolveGroundZ(const FVector& Point) const;

	bool PlaceCellPiece(UBDGridSubsystem& Grid, FBDPlacedPiece& Piece);
	bool PlaceEdgePiece(UBDGridSubsystem& Grid, FBDPlacedPiece& Piece);
	void SpawnPieceActors(const TArray<FTransform>& Transforms, FBDPlacedPiece& Piece) const;

	/** The piece under the cursor, preferring the fence when the cursor is nearer to it than to the cell center. */
	const FBDPlacedPiece* FindPieceUnderHover() const;
	void ForgetPiece(UBDGridSubsystem& Grid, const FBDPlacedPiece& Piece);

	void EnsurePreview();

	void HandlePlaceInput();
	void HandleRemoveInput();
	void HandleCancelInput();
	void HandleRotateInput(const FInputActionValue& Value);

	/** Pushes the gameplay mapping context onto the local player. */
	void AddMappingContext();

	UPROPERTY(Transient)
	TObjectPtr<UBDPlaceableData> CurrentSelection;

	UPROPERTY(Transient)
	TObjectPtr<ABDPlacementPreview> Preview;

	/** Resolved on first use: finding it walks the actor list. */
	mutable TWeakObjectPtr<ABDMatchManager> CachedMatch;

	/**
	 * Every cell a piece covers points at that piece, so a right click anywhere on a
	 * wide platform takes the whole thing back rather than punching a hole in it.
	 * Fences are the same by edge.
	 *
	 * Owned here for now because there is one local player and no save yet. It belongs
	 * in a world subsystem once pieces have to outlive the controller.
	 */
	UPROPERTY(Transient)
	TMap<FBDCellCoord, FBDPlacedPiece> PlacedByCell;

	UPROPERTY(Transient)
	TMap<FBDEdgeCoord, FBDPlacedPiece> PlacedByEdge;

	FBDCellCoord HoveredCell;
	FBDEdgeCoord HoveredEdge;

	/** Where the cursor ray met the grid plane, kept to choose between a fence and the cell it borders. */
	FVector HoverPoint = FVector::ZeroVector;

	bool bHoveringGrid = false;
	bool bCurrentPlacementValid = false;

	EBDPlacementRefusal CurrentRefusal = EBDPlacementRefusal::NoSelection;
	EBDPlacementRefusal LastReportedRefusal = EBDPlacementRefusal::NoSelection;

	/**
	 * Quarter turns from the piece's authored facing. Kept as steps rather than degrees
	 * so the footprint swap (odd steps) and the wrap-around stay exact. A fence only
	 * has two steps, since an edge has no front or back.
	 */
	int32 RotationSteps = 0;

	static constexpr int32 CellRotationStepCount = 4;
	static constexpr int32 EdgeRotationStepCount = 2;
	static constexpr float DegreesPerRotationStep = 90.0f;
};

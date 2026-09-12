// Brazil Defense. The gesture of putting a piece on the board and taking it back.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Grid/BDGridTypes.h"
#include "Match/BDMatchTypes.h"
#include "BDPlacementComponent.generated.h"

class ABDMatchManager;
class ABDPlacementPreview;
class UBDPlatformComponent;
class APlayerController;
class UBDGridSubsystem;
class UBDObjectiveSubsystem;
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

	/** Slot pieces: the platform and the slot the tower stands on. No cell is written for these. */
	UPROPERTY()
	TWeakObjectPtr<UBDPlatformComponent> Platform;

	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	bool IsOnSlot() const { return SlotIndex != INDEX_NONE; }
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
	/** Some spawn would lose every way to the goal; for the urn itself, some spawn cannot reach it. */
	WouldBlockPath,
	/** The urn is not down yet, and nothing is built before the urn. */
	ObjectiveMissing,
	/** The urn is being put outside the zone it may stand in. */
	ObjectiveOutOfZone,
	/** The platform slot under the cursor already holds a tower. */
	SlotTaken,
	/** The held defender is ground equipment and the cursor is over a platform slot. */
	TowerCannotGoOnSlot,
	/** The held defender is a character and the cursor is over a ground cell: characters stand on platforms. */
	CharacterNeedsPlatform,
	/** Moving the lifted piece here would cost more blue votes than the player has. */
	CannotAffordMove
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
 * The urn is a cell piece with its own rules: it comes first, it goes only in its zone,
 * and putting it down is UBDObjectiveSubsystem's job rather than a cell write here.
 *
 * A defender over a platform is the other special case: the hover snaps to the platform
 * slot nearest the cursor, the defender is mounted on it and no cell is written. On the
 * ground a defender takes its cell as Tower, which the creeps walk through. Which of the
 * two a defender accepts is on its UBDTowerData: towers take cells, characters take slots.
 *
 * Between waves a placed piece can be picked up and put elsewhere. The piece is lifted
 * off the board and held exactly like a fresh selection, so every rule above applies to
 * the destination; dropping it charges the match a share of its build cost in blue
 * votes, and an invalid or cancelled drop puts it back where it was for nothing.
 *
 * Nothing else here spends money or counts waves: this is only the gesture.
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

	/**
	 * Leaves placement mode and destroys the ghost. Also done on its own when the building
	 * phase ends or the budget of the held piece runs out: a piece stuck to the cursor
	 * says "you can still build", and at those moments that is not true.
	 */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Placement")
	void CancelSelection();

	/** Places the selected piece on the hovered cell or edge. @return false when the spot is refused. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Placement")
	bool TryPlaceAtHovered();

	/** Takes back whatever the player put under the cursor. @return false when there is nothing of theirs there. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Placement")
	bool TryRemoveAtHovered();

	//~ Moving ---------------------------------------------------------------

	/**
	 * Lifts the piece under the cursor off the board and holds it as the selection, so
	 * it can be dropped elsewhere with TryPlaceAtHovered or put back with CancelMove.
	 * @return false when nothing of the player's is there, or the match does not allow moving now.
	 */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Placement")
	bool TryBeginMoveAtHovered();

	/** Puts the lifted piece back where it was, for nothing, and drops the selection. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Placement")
	void CancelMove();

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Placement")
	bool IsMoving() const { return bMoving; }

	/** Blue votes dropping the lifted piece would charge. 0 when nothing is lifted. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Placement")
	int32 GetMoveCost() const;

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

	/** The platform under the cursor while a tower is held over one, or null. */
	UBDPlatformComponent* GetHoveredPlatform() const { return HoveredPlatform.Get(); }

	/** The slot the held tower would take, or INDEX_NONE when the hover is not over a platform. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Placement")
	int32 GetHoveredSlotIndex() const { return HoveredSlotIndex; }

	/** Whether the held tower is aimed at a platform slot rather than a cell. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Placement")
	bool IsHoveringSlot() const { return HoveredSlotIndex != INDEX_NONE && HoveredPlatform.IsValid(); }

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
	UBDObjectiveSubsystem* GetObjectives() const;

	bool IsEdgeSelection() const;
	bool IsObjectiveSelection() const;
	bool IsTowerSelection() const;

	/** The platform standing on a cell inside the battle area, or null. */
	UBDPlatformComponent* FindPlatformAt(const FBDCellCoord& Coord) const;

	/** Index of the slot of a platform nearest to a point on the board plane, or INDEX_NONE when it has no slots. */
	static int32 FindNearestSlot(const UBDPlatformComponent& Platform, const FVector& Point);

	/** Recomputes HoveredPlatform and HoveredSlotIndex for the current selection and hover. */
	void ResolveSlotHover();

	/** The class actually spawned for the selection: ActorClass, or the tower class of its tower data. */
	UClass* ResolveActorClass() const;

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
	EBDPlacementRefusal EvaluateObjectivePlacement() const;
	EBDPlacementRefusal EvaluateSlotPlacement() const;

	/** Whether the held defender is allowed on a grid cell / a platform slot, per its tower data. */
	bool CanSelectionStandOnGround() const;
	bool CanSelectionStandOnSlot() const;

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
	/** Hands the urn to UBDObjectiveSubsystem. Not remembered as a piece: it cannot be taken back. */
	bool PlaceObjectivePiece();
	/** Mounts the held tower on the hovered slot. No cell is written. */
	bool PlaceSlotPiece(FBDPlacedPiece& Piece);

	/** Takes back every tower mounted on the platforms of a piece, refunding each. Called before the platform itself goes. */
	void ForgetSlotPiecesOn(const FBDPlacedPiece& PlatformPiece);

	/** Lifts the pieces mounted on the platforms of a piece into MovingMounted, hidden, slots released. */
	void LiftSlotPiecesOn(const FBDPlacedPiece& PlatformPiece);

	/** Mounts MovingMounted back on the same slots of the platform just placed. */
	void RemountLiftedPieces(const FBDPlacedPiece& PlatformPiece);

	/** Places the lifted piece at the current hover, reusing its actors. @return false when the placement failed. */
	bool DropMovingPiece();

	/** Points the hover back at where the lifted piece came from, rotation included. */
	void HoverMoveOrigin();

	/** Whether the current hover is the very spot the lifted piece came from. */
	bool IsHoveringMoveOrigin() const;

	static void SetActorsHidden(const TArray<TObjectPtr<AActor>>& Actors, bool bHidden);
	void SpawnPieceActors(const TArray<FTransform>& Transforms, FBDPlacedPiece& Piece) const;

	/** The piece under the cursor, preferring the fence when the cursor is nearer to it than to the cell center. */
	const FBDPlacedPiece* FindPieceUnderHover() const;
	void ForgetPiece(UBDGridSubsystem& Grid, const FBDPlacedPiece& Piece);

	void EnsurePreview();

	void HandlePlaceInput();
	void HandlePlaceReleased();
	void HandleRemoveInput();
	void HandleCancelInput();
	void HandleRotateInput(const FInputActionValue& Value);

	/** Pushes the gameplay mapping context onto the local player. */
	void AddMappingContext();

	/**
	 * Listens to the match once there is one. The match manager is spawned by the game
	 * mode, possibly after this component began play, so the binding is retried from the
	 * tick until it takes.
	 */
	void EnsureMatchBinding();
	void HandleMatchPhaseChanged(EBDMatchPhase NewPhase);

	/** Drops the selection when the match has nothing of that kind left to place. */
	void CancelSelectionIfBudgetExhausted();

	UPROPERTY(Transient)
	TObjectPtr<UBDPlaceableData> CurrentSelection;

	UPROPERTY(Transient)
	TObjectPtr<ABDPlacementPreview> Preview;

	/** Resolved on first use: finding it walks the actor list. */
	mutable TWeakObjectPtr<ABDMatchManager> CachedMatch;

	/** The match whose phase changes this component is bound to, so the binding is dropped cleanly. */
	TWeakObjectPtr<ABDMatchManager> BoundMatch;
	FDelegateHandle PhaseChangedHandle;

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

	/** Towers mounted on platform slots. Few enough to search; keyed by nothing on purpose. */
	UPROPERTY(Transient)
	TArray<FBDPlacedPiece> PlacedOnSlots;

	/** The platform and slot a held tower is aimed at. Reset whenever the hover is not over a platform. */
	TWeakObjectPtr<UBDPlatformComponent> HoveredPlatform;
	int32 HoveredSlotIndex = INDEX_NONE;

	//~ The lifted piece, while one is being moved ---------------------------

	bool bMoving = false;

	/** The piece as it stood, actors and all, hidden while it travels. */
	UPROPERTY(Transient)
	FBDPlacedPiece MovingPiece;

	/** Pieces that were mounted on the lifted platform, slot indices kept. */
	UPROPERTY(Transient)
	TArray<FBDPlacedPiece> MovingMounted;

	/** Where it came from, so a cancelled or refused drop puts it back. */
	FBDCellCoord MoveOriginCell;
	FBDEdgeCoord MoveOriginEdge;
	TWeakObjectPtr<UBDPlatformComponent> MoveOriginPlatform;
	int32 MoveOriginSlot = INDEX_NONE;
	int32 MoveOriginRotationSteps = 0;

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

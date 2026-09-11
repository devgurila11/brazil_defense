// Brazil Defense. Ghost of the piece being positioned.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/BDGridTypes.h"
#include "BDPlacementPreview.generated.h"

class UBDGridSubsystem;
class UBDPlaceableData;
class UInstancedStaticMeshComponent;

/**
 * Translucent stand-in for the piece the player is holding, snapped to the center of
 * the hovered cell. Turns red the moment the spot would be refused, so the answer is
 * on screen before the click rather than after it.
 *
 * The mesh is instanced because a tiled piece is several copies per cell, and the
 * ghost has to show exactly the copies that will be spawned.
 *
 * Spawned and driven by UBDPlacementComponent. It carries no rules of its own: it is
 * told where to sit and whether the spot is legal.
 */
UCLASS(NotPlaceable)
class BRAZIL_DEFENSE_API ABDPlacementPreview : public AActor
{
	GENERATED_BODY()

public:
	ABDPlacementPreview();

	virtual void Tick(float DeltaSeconds) override;

	/** Switches the ghost to a piece, or hides it when Placeable is null. */
	void SetPlaceable(const UBDPlaceableData* Placeable);

	/**
	 * Moves the ghost onto a spot and colors it by whether the spot is legal.
	 * The outline box and the copies come in per update rather than from the placeable,
	 * because rotation, the hovered edge and the floor all change them, and the ghost
	 * must show what is actually about to be spawned. Everything is in world space.
	 */
	void UpdatePlacement(const FVector& OutlineCenter, const FVector& OutlineExtent,
		const TArray<FTransform>& WorldInstanceTransforms, bool bValid);

	void HidePreview();

private:
	void ApplyMaterialForState();

	/** Lays the copies of the piece out again when they moved. */
	void RebuildInstances(const TArray<FTransform>& WorldInstanceTransforms);

	UPROPERTY(VisibleAnywhere, Category = "Preview")
	TObjectPtr<UInstancedStaticMeshComponent> MeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<const UBDPlaceableData> CurrentPlaceable;

	/** Debug outline of the spot being taken: a footprint of cells or a strip along a fence. */
	FVector OutlineCenter = FVector::ZeroVector;
	FVector OutlineExtent = FVector::ZeroVector;

	bool bPlacementValid = false;
	bool bPreviewVisible = false;

	/** Layout the instances were last built for, so a frame without movement does not rebuild them. */
	TArray<FTransform> BuiltTransforms;
	bool bInstancesDirty = true;

	/** Avoids reassigning the same material every frame of a hover. */
	bool bLastAppliedValidState = false;
	bool bHasAppliedMaterial = false;
};

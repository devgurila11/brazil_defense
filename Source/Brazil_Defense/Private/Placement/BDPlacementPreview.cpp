// Brazil Defense. Ghost of the piece being positioned.

#include "Placement/BDPlacementPreview.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Grid/BDGridDebug.h"
#include "Grid/BDGridSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementSettings.h"

ABDPlacementPreview::ABDPlacementPreview()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// The root stays unrotated on the footprint center; the yaw lives in the instances,
	// which is how the spawn applies it too.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	SetRootComponent(Root);

	MeshComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PreviewMesh"));
	MeshComponent->SetupAttachment(Root);
	MeshComponent->SetMobility(EComponentMobility::Movable);

	// A ghost is a picture, not a thing in the world: it must never be hit by a trace,
	// cast a shadow or take part in anything.
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);
	MeshComponent->SetGenerateOverlapEvents(false);

	SetActorEnableCollision(false);
	SetHidden(true);
}

void ABDPlacementPreview::SetPlaceable(const UBDPlaceableData* Placeable)
{
	CurrentPlaceable = Placeable;
	bHasAppliedMaterial = false;

	if (Placeable == nullptr)
	{
		HidePreview();
		return;
	}

	// Synchronous load on purpose: the player just picked this piece and the ghost has
	// to appear on the very next frame.
	UStaticMesh* Mesh = Placeable->PreviewMesh.LoadSynchronous();
	MeshComponent->SetStaticMesh(Mesh);
	MeshComponent->SetVisibility(Mesh != nullptr);
	bInstancesDirty = true;
}

void ABDPlacementPreview::RebuildInstances(const TArray<FTransform>& WorldInstanceTransforms)
{
	if (!bInstancesDirty && BuiltTransforms.Num() == WorldInstanceTransforms.Num())
	{
		bool bUnchanged = true;
		for (int32 Index = 0; Index < BuiltTransforms.Num() && bUnchanged; ++Index)
		{
			bUnchanged = BuiltTransforms[Index].Equals(WorldInstanceTransforms[Index]);
		}

		if (bUnchanged)
		{
			return;
		}
	}

	bInstancesDirty = false;
	BuiltTransforms = WorldInstanceTransforms;

	MeshComponent->ClearInstances();
	if (CurrentPlaceable == nullptr || MeshComponent->GetStaticMesh() == nullptr)
	{
		return;
	}

	// Lifted a little off the floor so the ghost does not z-fight with the pavement.
	const float HeightOffset = UBDPlacementSettings::Get().PreviewHeightOffset;
	TArray<FTransform> Lifted = WorldInstanceTransforms;
	for (FTransform& Transform : Lifted)
	{
		Transform.AddToTranslation(FVector(0.0f, 0.0f, HeightOffset));
	}

	MeshComponent->AddInstances(Lifted, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ true);
}

void ABDPlacementPreview::UpdatePlacement(const FVector& InOutlineCenter, const FVector& InOutlineExtent,
	const TArray<FTransform>& WorldInstanceTransforms, const bool bValid)
{
	if (CurrentPlaceable == nullptr)
	{
		HidePreview();
		return;
	}

	OutlineCenter = InOutlineCenter;
	OutlineExtent = InOutlineExtent;
	bPlacementValid = bValid;
	bPreviewVisible = true;

	// The root only anchors the actor over the spot; the copies are placed in world
	// space, so the outline center is as good a home as any.
	SetActorLocation(OutlineCenter);
	RebuildInstances(WorldInstanceTransforms);
	SetHidden(false);

	ApplyMaterialForState();
}

void ABDPlacementPreview::HidePreview()
{
	bPreviewVisible = false;
	SetHidden(true);
}

void ABDPlacementPreview::ApplyMaterialForState()
{
	if (bHasAppliedMaterial && bLastAppliedValidState == bPlacementValid)
	{
		return;
	}

	bHasAppliedMaterial = true;
	bLastAppliedValidState = bPlacementValid;

	const UBDPlacementSettings& Settings = UBDPlacementSettings::Get();
	const TSoftObjectPtr<UMaterialInterface>& Wanted = bPlacementValid
		? Settings.PreviewValidMaterial
		: Settings.PreviewInvalidMaterial;

	// Unset materials are fine: the footprint outline already carries the answer.
	// Every slot is overridden, or a mesh with two materials would ghost only half.
	if (UMaterialInterface* Material = Wanted.LoadSynchronous())
	{
		for (int32 Slot = 0; Slot < MeshComponent->GetNumMaterials(); ++Slot)
		{
			MeshComponent->SetMaterial(Slot, Material);
		}
	}
}

void ABDPlacementPreview::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bPreviewVisible || OutlineExtent.IsNearlyZero() || !BDGridDebug::IsEnabled())
	{
		return;
	}

	// The outline is what actually tells the player which cells or which boundary are
	// being taken, and it works before any preview mesh or material has been authored.
	const UBDPlacementSettings& Settings = UBDPlacementSettings::Get();

	DrawDebugBox(GetWorld(), OutlineCenter, OutlineExtent,
		bPlacementValid ? Settings.ValidOutlineColor : Settings.InvalidOutlineColor,
		BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime,
		BDGridDebug::DepthPriority, Settings.OutlineThickness);
}

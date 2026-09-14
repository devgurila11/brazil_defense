// Brazil Defense. Configuration of the placement gesture and its preview.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BDPlacementSettings.generated.h"

class UBDPlaceableData;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;

/**
 * Everything the placement flow needs that is not a grid dimension.
 * Kept apart from UBDGridSettings so the grid settings stay about the grid.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Placement"))
class BRAZIL_DEFENSE_API UBDPlacementSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDPlacementSettings();

	static const UBDPlacementSettings& Get();

	//~ Palette ------------------------------------------------------------
	// What the player can pick up, in the order the HUD shows it and the number keys
	// select it (1 is the first). The urn is not listed: it comes from the objective
	// settings and has its own button.

	UPROPERTY(config, EditAnywhere, Category = "Palette", meta = (AllowedClasses = "/Script/Brazil_Defense.BDPlaceableData"))
	TArray<TSoftObjectPtr<UBDPlaceableData>> Palette;

	//~ Input ----------------------------------------------------------------

	/** Mapping context pushed while the player is on the board. */
	UPROPERTY(config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputMappingContext> GameplayMappingContext;

	/** Priority the gameplay context is pushed with. Higher wins over lower contexts. */
	UPROPERTY(config, EditAnywhere, Category = "Input")
	int32 GameplayMappingPriority = 0;

	/** Puts the held piece down on the hovered cell. */
	UPROPERTY(config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputAction> PlaceAction;

	/** Takes back the piece under the cursor. */
	UPROPERTY(config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputAction> RemoveAction;

	/** Swaps the footprint axes of the held piece. */
	UPROPERTY(config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputAction> RotateAction;

	/** Drops the current selection and leaves placement mode. */
	UPROPERTY(config, EditAnywhere, Category = "Input")
	TSoftObjectPtr<UInputAction> CancelAction;

	//~ Preview ghost --------------------------------------------------------

	/** Material of the ghost while the spot is legal. Optional: the outline is drawn either way. */
	UPROPERTY(config, EditAnywhere, Category = "Preview")
	TSoftObjectPtr<UMaterialInterface> PreviewValidMaterial;

	/** Material of the ghost while the spot is refused. */
	UPROPERTY(config, EditAnywhere, Category = "Preview")
	TSoftObjectPtr<UMaterialInterface> PreviewInvalidMaterial;

	/** Outline color of a legal spot. */
	UPROPERTY(config, EditAnywhere, Category = "Preview")
	FColor ValidOutlineColor = FColor(60, 255, 120, 255);

	/** Outline color of a refused spot. */
	UPROPERTY(config, EditAnywhere, Category = "Preview")
	FColor InvalidOutlineColor = FColor(255, 40, 40, 255);

	/** Height of the footprint outline box. */
	UPROPERTY(config, EditAnywhere, Category = "Preview", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float OutlineHeight = 200.0f;

	/** Thickness of the footprint outline. */
	UPROPERTY(config, EditAnywhere, Category = "Preview", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float OutlineThickness = 6.0f;

	/** Width of the outline strip drawn along a fence, across the boundary. */
	UPROPERTY(config, EditAnywhere, Category = "Preview", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float EdgeOutlineWidth = 40.0f;

	/** Height the preview sits at, above the floor it was snapped to. */
	UPROPERTY(config, EditAnywhere, Category = "Preview", meta = (ForceUnits = "cm"))
	float PreviewHeightOffset = 10.0f;

	//~ Ground ---------------------------------------------------------------

	/**
	 * Channel traced downwards to find the floor a piece stands on. The grid is a flat
	 * plane at GridOrigin.Z, but the Esplanada is not: a piece spawned on the plane
	 * would sit buried wherever the pavement is higher. The terrain has to block it.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Ground")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_WorldStatic;

	/**
	 * How far above and below the grid plane the floor is looked for.
	 * When nothing is hit within it, the piece falls back to the grid plane.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Ground", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float GroundTraceDistance = 3000.0f;

	//~ Hover ----------------------------------------------------------------

	/**
	 * How far along the mouse ray the grid plane is still considered.
	 * Guards against a nearly horizontal camera projecting a hit at the horizon.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Hover", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float MaxHoverDistance = 200000.0f;
};

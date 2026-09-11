// Brazil Defense. Project wide configuration of the gameplay grid.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Grid/BDGridTypes.h"
#include "BDGridSettings.generated.h"

/**
 * Single source of truth for the gameplay grid layout and for its debug visualization.
 * Edited in Project Settings > Game > Brazil Defense - Grid and saved to DefaultGame.ini.
 * No system is allowed to hardcode grid dimensions, cell size or debug values.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Grid"))
class BRAZIL_DEFENSE_API UBDGridSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDGridSettings();

	/** Read only access to the configured values. */
	static const UBDGridSettings& Get();

	//~ Layout ---------------------------------------------------------------

	/** Number of cells along the world +X axis. */
	UPROPERTY(config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1", UIMin = "1"))
	int32 GridSizeX = 54;

	/** Number of cells along the world +Y axis. */
	UPROPERTY(config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1", UIMin = "1"))
	int32 GridSizeY = 22;

	/** Size of one square cell in world units. */
	UPROPERTY(config, EditAnywhere, Category = "Layout", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float CellSize = 200.0f;

	/** World location of the bottom-left corner of cell (0,0). */
	UPROPERTY(config, EditAnywhere, Category = "Layout")
	FVector GridOrigin = FVector::ZeroVector;

	//~ Debug drawing --------------------------------------------------------

	/** Draw the grid in the level editor viewport (requires a BDGridVisualizer actor in the level). */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Visibility")
	bool bDrawInEditor = true;

	/** Draw the grid while playing. Needs nothing placed in the level: see UBDGridDebugDrawer. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Visibility")
	bool bDrawInGame = true;

	/** Draw the coordinate label of the cells that are close enough to the camera. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Visibility")
	bool bDrawCellCoords = true;

	/** Draw an arrow pair at the grid origin showing the +X and +Y directions. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Visibility")
	bool bDrawOriginMarker = true;

	/** Vertical offset applied to every debug primitive, to avoid z-fighting with the floor. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Geometry", meta = (ForceUnits = "cm"))
	float DrawHeightOffset = 5.0f;

	/** Thickness of the internal cell lines. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Geometry", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LineThickness = 1.0f;

	/** Thickness of the outer border of the grid. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Geometry", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BorderThickness = 6.0f;

	/** Fraction of the cell covered by the state fill. 1.0 fills the whole cell. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Geometry", meta = (ClampMin = "0.05", ClampMax = "1.0", UIMin = "0.05", UIMax = "1.0"))
	float CellFillRatio = 0.9f;

	/** Height of the state fill boxes. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Geometry", meta = (ClampMin = "0.1", UIMin = "0.1", ForceUnits = "cm"))
	float CellFillHeight = 4.0f;

	/** Length of the origin marker arrows. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Geometry", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float OriginMarkerLength = 300.0f;

	/** Head size of the origin marker arrows. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Geometry", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float OriginMarkerArrowSize = 40.0f;

	/** Color of the internal cell lines. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Color")
	FColor LineColor = FColor(60, 60, 60, 255);

	/** Color of the outer border of the grid. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Color")
	FColor BorderColor = FColor(255, 200, 0, 255);

	/** Color of the origin marker arrows. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Color")
	FColor OriginMarkerColor = FColor(255, 0, 255, 255);

	/** Fill color used for each cell state. States missing from the map are not filled. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Color")
	TMap<EBDCellState, FColor> CellStateColors;

	/** Color of a blocked edge, drawn as a line on the boundary. Kept apart from every cell fill so a fence reads as a fence. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Color")
	FColor BlockedEdgeColor = FColor(0, 200, 255, 255);

	/** Thickness of a blocked edge line. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Geometry", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float BlockedEdgeThickness = 12.0f;

	//~ Debug coordinate labels ----------------------------------------------

	/** Cells farther than this from the camera do not show their coordinate label. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Coords", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float CoordDrawDistance = 3000.0f;

	/** Screen scale of the coordinate label text. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Coords", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float CoordTextScale = 1.0f;

	/** Color of the coordinate label text. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Coords")
	FColor CoordTextColor = FColor(200, 200, 200, 255);

	/** Upper bound of labels drawn per view, as a safety net on very large grids. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Coords", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxCoordLabelsPerView = 400;

	//~ Debug path -----------------------------------------------------------

	/** Color of the line joining the cells of the last path found. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Path")
	FColor PathLineColor = FColor(255, 255, 0, 255);

	/** Color of the start and goal markers of the last path found. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Path")
	FColor PathEndpointColor = FColor(255, 0, 255, 255);

	/** Thickness of the path line. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Path", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float PathLineThickness = 8.0f;

	/** Radius of the start and goal markers. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Path", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float PathEndpointRadius = 60.0f;

	/** Segment count of the start and goal marker spheres. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Path", meta = (ClampMin = "3", UIMin = "3"))
	int32 PathEndpointSegments = 12;

	/** Height the path is drawn at, above the grid plane. Sits over the cell fills. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Path", meta = (ForceUnits = "cm"))
	float PathDrawHeightOffset = 30.0f;

	//~ Debug platforms ------------------------------------------------------

	/** Draw a marker on every platform slot, colored by free or occupied. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Platform")
	bool bDrawPlatformSlots = true;

	/** Draw the grid rectangle covered by every platform. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Platform")
	bool bDrawPlatformFootprint = true;

	/** Radius of the slot marker sphere. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Platform", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float SlotMarkerRadius = 40.0f;

	/** Segment count of the slot marker sphere. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Platform", meta = (ClampMin = "3", UIMin = "3"))
	int32 SlotMarkerSegments = 12;

	/** Length of the arrow showing which way a tower on the slot would face. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Platform", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float SlotForwardLength = 120.0f;

	/** Height of the platform footprint box. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Platform", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float PlatformFootprintHeight = 150.0f;

	/** Color of a slot with no tower on it. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Platform")
	FColor SlotFreeColor = FColor(60, 255, 120, 255);

	/** Color of a slot already holding a tower. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Platform")
	FColor SlotOccupiedColor = FColor(255, 140, 0, 255);

	/** Color of the footprint of a platform standing inside the battle area. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Platform")
	FColor PlatformFootprintColor = FColor(0, 160, 255, 255);

	/** Error color: a platform marked as outside the battle area that still overlaps the grid. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Platform")
	FColor PlatformOutsideFootprintColor = FColor(255, 0, 0, 255);

	/**
	 * Returns the configured fill color of a state.
	 * @return false when the state has no configured color and must not be filled.
	 */
	bool TryGetCellStateColor(EBDCellState State, FColor& OutColor) const;
};

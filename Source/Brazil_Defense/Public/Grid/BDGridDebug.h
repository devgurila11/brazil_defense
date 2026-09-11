// Brazil Defense. Runtime switch, shared constants and shared drawing of the grid debug.

#pragma once

#include "CoreMinimal.h"

class UCanvas;
class UWorld;

namespace BDGridDebug
{
	/**
	 * True while the BD.Grid.Debug console variable is enabled.
	 * Master switch on top of the per feature toggles in UBDGridSettings: every grid
	 * related debug drawing checks it, so a single console command silences all of it.
	 */
	BRAZIL_DEFENSE_API bool IsEnabled();

	/**
	 * Whether the grid debug may draw into this world: the console variable above plus
	 * the bDrawInEditor / bDrawInGame setting matching the world type.
	 */
	BRAZIL_DEFENSE_API bool ShouldDrawInWorld(const UWorld& World);

	/**
	 * Draws the grid lines, the cell state fills and the origin marker of the grid owned
	 * by World. A silent no-op when the debug is off or the world has no populated grid,
	 * so a caller only has to decide whether it is the one responsible for this world.
	 */
	BRAZIL_DEFENSE_API void DrawGrid(const UWorld& World);

	/**
	 * Canvas pass: coordinate labels of the cells around the view.
	 * Draws nothing when the canvas belongs to a viewport showing another world, so the
	 * editor world and a running PIE world can both register it without crossing over.
	 */
	BRAZIL_DEFENSE_API void DrawCoordLabels(UCanvas& Canvas, const UWorld& World);

	/** Every grid debug primitive is redrawn each tick, so it only needs to live one frame. */
	inline constexpr bool bPersistentLines = false;
	inline constexpr float SingleFrameLifeTime = -1.0f;
	inline constexpr uint8 DepthPriority = 0;
}

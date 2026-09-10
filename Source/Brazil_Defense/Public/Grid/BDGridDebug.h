// Brazil Defense. Runtime switch and shared constants for the grid debug drawing.

#pragma once

#include "CoreMinimal.h"

namespace BDGridDebug
{
	/**
	 * True while the BD.Grid.Debug console variable is enabled.
	 * Master switch on top of the per feature toggles in UBDGridSettings: every grid
	 * related debug drawing checks it, so a single console command silences all of it.
	 */
	BRAZIL_DEFENSE_API bool IsEnabled();

	/** Every grid debug primitive is redrawn each tick, so it only needs to live one frame. */
	inline constexpr bool bPersistentLines = false;
	inline constexpr float SingleFrameLifeTime = -1.0f;
	inline constexpr uint8 DepthPriority = 0;
}

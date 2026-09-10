// Brazil Defense. Runtime switch for the grid debug drawing.

#include "Grid/BDGridDebug.h"

#include "HAL/IConsoleManager.h"

namespace BDGridDebug
{
	// Defaults to on so the grid stays visible while the layout is being authored.
	static int32 GGridDebugEnabled = 1;

	static FAutoConsoleVariableRef CVarGridDebug(
		TEXT("BD.Grid.Debug"),
		GGridDebugEnabled,
		TEXT("Brazil Defense grid debug drawing: grid lines, cell states, platform footprints and slots. 0 to disable."),
		ECVF_Cheat);

	bool IsEnabled()
	{
		return GGridDebugEnabled != 0;
	}
}

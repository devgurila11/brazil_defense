// Brazil Defense. Which build is running, said the same way everywhere.

#pragma once

#include "CoreMinimal.h"

/**
 * A label for the binary that is running - when it was built, and in which
 * configuration - so a tester always knows which version they had in front of them.
 *
 * Read off the binary itself (the game module in the editor, the executable in a
 * packaged game) rather than stamped by a build step: every compile moves it on its own,
 * nothing has to remember to bump a counter, and a stale binary cannot claim a new date.
 */
namespace BDBuildInfo
{
	/** "build 2026.09.23-2241 Development": when the running binary was linked, in local time. */
	BRAZIL_DEFENSE_API const FString& GetLabel();
}

// Brazil Defense. Log categories of the game module.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

/** Grid, platforms and everything placed on the battle area. */
DECLARE_LOG_CATEGORY_EXTERN(LogBDGrid, Log, All);

/** Pathfinding and blocking validation. */
DECLARE_LOG_CATEGORY_EXTERN(LogBDPath, Log, All);

/** Obstacle generation and the layouts it rejects. */
DECLARE_LOG_CATEGORY_EXTERN(LogBDObstacle, Log, All);

/** Match phases, waves, budgets and game speed. */
DECLARE_LOG_CATEGORY_EXTERN(LogBDMatch, Log, All);

/** Enemies, spawn points and the routes they walk. */
DECLARE_LOG_CATEGORY_EXTERN(LogBDWave, Log, All);

/** Towers, targeting and projectiles. */
DECLARE_LOG_CATEGORY_EXTERN(LogBDTower, Log, All);

/** Debug tooling: automatic test setups and the like. */
DECLARE_LOG_CATEGORY_EXTERN(LogBDDebug, Log, All);

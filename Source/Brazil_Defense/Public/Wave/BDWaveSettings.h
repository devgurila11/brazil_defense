// Brazil Defense. Configuration of the creeps: how they stand on the floor, how they
// follow their route and how the routes are drawn.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "BDWaveSettings.generated.h"

class UBDEnemyData;

/**
 * Everything about the creeps that is not the creep itself, kept out of the code so it
 * can be tuned without a recompile. Edited in Project Settings > Game > Brazil Defense - Waves.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Waves"))
class BRAZIL_DEFENSE_API UBDWaveSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDWaveSettings();

	static const UBDWaveSettings& Get();

	/** Enemy the BD.Wave.* console commands spawn when no asset is named. Console tooling until waves have content. */
	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (AllowedClasses = "/Script/Brazil_Defense.BDEnemyData"))
	TSoftObjectPtr<UBDEnemyData> DebugEnemy;

	//~ Waves ----------------------------------------------------------------

	/** Enemy a wave sends out, until waves have a composition of their own. Falls back to DebugEnemy when unset. The pacing lives in the balance settings. */
	UPROPERTY(config, EditAnywhere, Category = "Waves", meta = (AllowedClasses = "/Script/Brazil_Defense.BDEnemyData"))
	TSoftObjectPtr<UBDEnemyData> WaveEnemy;

	//~ Ground ---------------------------------------------------------------

	/**
	 * Channel traced downwards under every cell of a route to find the floor the creep
	 * walks on. The grid is a flat plane at GridOrigin.Z, but the Esplanada is not.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Ground")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_WorldStatic;

	/** How far above and below the grid plane the floor is looked for. Falls back to the grid plane when nothing is hit. */
	UPROPERTY(config, EditAnywhere, Category = "Ground", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float GroundTraceDistance = 3000.0f;

	//~ Movement -------------------------------------------------------------

	/**
	 * How close to the objective actor a creep has to get to count as arrived. The last
	 * leg of a route goes from the final Goal cell to the urn itself, and the urn has a
	 * body: a creep is not expected to reach its exact origin.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float ArrivalDistance = 150.0f;

	/**
	 * Degrees per second a creep turns towards where it is walking. 0 snaps instantly.
	 * Cosmetic only: the position always follows the route exactly, whatever the facing.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TurnRate = 540.0f;

	//~ Debug routes ---------------------------------------------------------

	/** Colors of the routes drawn by BD.Path.ShowRoutes, one per spawn point, cycling when there are more points than colors. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Routes")
	TArray<FColor> RouteColors = {
		FColor(255, 80, 80, 255), FColor(80, 255, 80, 255), FColor(80, 160, 255, 255),
		FColor(255, 220, 60, 255), FColor(255, 100, 255, 255), FColor(80, 255, 255, 255) };

	UPROPERTY(config, EditAnywhere, Category = "Debug|Routes", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RouteLineThickness = 6.0f;

	/** Height the routes are drawn at, above the grid plane. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Routes", meta = (ForceUnits = "cm"))
	float RouteDrawHeightOffset = 40.0f;

	/** Color of a route for a spawn point that has no way to the urn. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Routes")
	FColor RouteBlockedColor = FColor(255, 0, 0, 255);

	/** Radius of the marker drawn on a spawn point with no route. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Routes", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "cm"))
	float RouteBlockedMarkerRadius = 120.0f;

	/** The route color of a spawn point. */
	FColor GetRouteColor(int32 SpawnPointIndex) const;
};

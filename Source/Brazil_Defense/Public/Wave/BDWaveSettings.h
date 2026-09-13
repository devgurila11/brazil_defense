// Brazil Defense. Configuration of the creeps: how they stand on the floor, how they
// follow their route and how the routes are drawn.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/EngineTypes.h"
#include "BDWaveSettings.generated.h"

class UBDEnemyData;

/** Who draws a route cost map: see UBDWaveSettings::RouteVarianceMode. */
UENUM(BlueprintType)
enum class EBDRouteVarianceMode : uint8
{
	/** Every creep draws its own map. The creeps of one mouth split over the corridors of the same wave. */
	PerCreep UMETA(DisplayName = "Per Creep"),

	/** One map per mouth per wave. The creeps of a mouth walk together; the next wave takes another way. */
	PerSpawnPointPerWave UMETA(DisplayName = "Per Spawn Point Per Wave")
};

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

	/** The enemy a wave sends out: WaveEnemy, or DebugEnemy while that is unset. Loads it. Null when neither is set. */
	const UBDEnemyData* ResolveWaveEnemy() const;

	/**
	 * The red candidate. Its enemy class is expected to be ABDCandidate or a child; the
	 * health on the asset is ignored, the candidate is as tough as the balance says.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Waves", meta = (AllowedClasses = "/Script/Brazil_Defense.BDEnemyData"))
	TSoftObjectPtr<UBDEnemyData> CandidateEnemy;

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

	//~ Organic movement -----------------------------------------------------
	// A route is a line of cell centers, and a horde walking it exactly is a queue
	// turning ninety degrees on the spot. These three take it apart: the corners are cut
	// where there is room, and every creep draws a lane and a pace of its own at spawn.
	// All three are fractions of a cell, so they hold whatever the grid is scaled to.

	/**
	 * How far before a corner a creep starts turning, as a fraction of a cell. 0 keeps
	 * the square turns.
	 *
	 * Only corners in open ground are cut: a turn forced by a fence, a platform or
	 * scenery in the cells around it is walked into and taken square, flush with the
	 * barrier. Half a cell is the hard ceiling whatever is set here, so the curve can
	 * never leave the cell the route turns on.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Movement|Organic", meta = (ClampMin = "0.0", ClampMax = "0.5", UIMin = "0.0", UIMax = "0.5"))
	float CornerSmoothRadius = 0.35f;

	/**
	 * Widest lane a creep may take beside the middle of its route, as a fraction of a
	 * cell, drawn per creep and kept for the whole trip. 0.4 spreads a horde over most of
	 * a corridor; 0 puts every creep back on the same line.
	 *
	 * Cut back on the sides where a barrier stands, down to whatever room is left for the
	 * body of the creep: a one cell corridor files the horde into a line, which is the
	 * point. Where the horde spreads there is space, where it queues the player closed
	 * something.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Movement|Organic", meta = (ClampMin = "0.0", ClampMax = "0.45", UIMin = "0.0", UIMax = "0.45"))
	float LateralOffsetMax = 0.4f;

	/**
	 * How much of its lane a creep drifts across on the way, as a fraction of
	 * LateralOffsetMax. 0 holds the lane it drew and the horde walks parallel lines, like
	 * ants; 0.6 has each one wandering across most of the corridor on its own wavelength
	 * and its own phase, so the file keeps rearranging itself instead of holding formation.
	 *
	 * The drift is bounded by LateralOffsetMax and by the same barrier check as the lane
	 * itself, so wandering can never walk a creep into a fence.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Movement|Organic", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float LaneWander = 0.6f;

	/**
	 * How much creeps differ in how tightly they take a corner, as a fraction of
	 * CornerSmoothRadius. 0.4 has some hugging the inside and others swinging wide; 0 cuts
	 * every corner identically, which reads as one creep repeated.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Movement|Organic", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float CornerRadiusVariance = 0.4f;

	/**
	 * Spread of the creep speeds around the one on the data asset, as a fraction of it.
	 * 0.1 is plus or minus ten percent. It works twice: once as the pace a creep draws at
	 * spawn, and again, at half the amplitude, as a slow breath around that pace, so a
	 * queue keeps opening and closing gaps instead of holding its spacing forever.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Movement|Organic", meta = (ClampMin = "0.0", ClampMax = "0.5", UIMin = "0.0", UIMax = "0.5"))
	float SpeedVariance = 0.1f;

	//~ Route variance ---------------------------------------------------------
	// The A* answers the shortest path, and the shortest path is the same for every creep
	// until the maze changes: the whole horde on one line, wave after wave, and a player
	// who got it right once never has to touch the board again. So the route is per creep,
	// searched over a cost map of its own where every walkable cell weighs between 1 and
	// RouteCostVariance. Corridors nearly as short as the shortest split the horde; a
	// corridor the player closed is closed in every map. See FBDRouteCost.
	//
	// The blocking validation keeps the uniform cost: it asks whether a path exists, not
	// which, and a seeded answer would accept a fence in one match and refuse it in another.

	/**
	 * Highest cost a cell may draw, as a multiple of the uniform step. 1 puts every creep
	 * back on the shortest path; 1.3 lets a corridor up to thirty percent longer win some
	 * of the creeps. Drawn from the seed of the match, so a seed reproduces the spread.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Movement|Route Variance", meta = (ClampMin = "1.0", UIMin = "1.0", UIMax = "3.0"))
	float RouteCostVariance = 1.3f;

	/** Whether every creep draws a map of its own or the creeps of a mouth share one per wave. */
	UPROPERTY(config, EditAnywhere, Category = "Movement|Route Variance")
	EBDRouteVarianceMode RouteVarianceMode = EBDRouteVarianceMode::PerCreep;

	//~ Debug routes ---------------------------------------------------------

	/** Colors of the routes drawn by BD.Path.ShowRoutes, one per spawn point, cycling when there are more points than colors. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Routes")
	TArray<FColor> RouteColors = {
		FColor(255, 80, 80, 255), FColor(80, 255, 80, 255), FColor(80, 160, 255, 255),
		FColor(255, 220, 60, 255), FColor(255, 100, 255, 255), FColor(80, 255, 255, 255) };

	/** Thickness of the uniform cost route of a mouth, the thin line under the creeps' own. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Routes", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RouteLineThickness = 6.0f;

	/** Thickness of the route each living creep is actually walking, drawn over the uniform one in the color of its mouth. */
	UPROPERTY(config, EditAnywhere, Category = "Debug|Routes", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CreepRouteLineThickness = 14.0f;

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

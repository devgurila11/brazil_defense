// Brazil Defense. Configuration of the towers: how a hit is judged and how combat is drawn.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BDTowerSettings.generated.h"

/**
 * Everything about the towers that is not the tower itself, kept out of the code so it
 * can be tuned without a recompile. Edited in Project Settings > Game > Brazil Defense - Towers.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Towers"))
class BRAZIL_DEFENSE_API UBDTowerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDTowerSettings();

	static const UBDTowerSettings& Get();

	//~ Projectiles ----------------------------------------------------------

	/**
	 * How close to its aim point a projectile has to get to count as a hit. A projectile
	 * is not a physics body: it is interpolated towards the creep and judged by distance,
	 * because at 4x game speed a physical one crosses the creep between two frames.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Projectile", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float HitDistance = 30.0f;

	/** Projectiles alive longer than this are dropped: a target that vanished the wrong way must not leave one flying forever. */
	UPROPERTY(config, EditAnywhere, Category = "Projectile", meta = (ClampMin = "0.1", UIMin = "0.1", ForceUnits = "s"))
	float MaxLifetime = 10.0f;

	//~ Debug ----------------------------------------------------------------

	/** Color of the range sphere and circle drawn by BD.Tower.ShowRange. */
	UPROPERTY(config, EditAnywhere, Category = "Debug")
	FColor RangeColor = FColor(80, 200, 255, 255);

	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "3", UIMin = "3"))
	int32 RangeSegments = 48;

	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RangeThickness = 4.0f;

	/**
	 * Segments of the range sphere. Low on purpose: a sphere is segments squared in lines,
	 * drawn every frame for every defender.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "4", UIMin = "4"))
	int32 RangeSphereSegments = 16;

	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RangeSphereThickness = 1.0f;

	/** Color of the circle of the next level's range, drawn only when the next level reaches further. */
	UPROPERTY(config, EditAnywhere, Category = "Debug")
	FColor NextLevelRangeColor = FColor(255, 210, 60, 255);

	/** Color of the level and range written above each defender. */
	UPROPERTY(config, EditAnywhere, Category = "Debug")
	FColor RangeLabelColor = FColor(255, 255, 255, 255);

	/** Color of the line from a tower to its target drawn by BD.Tower.ShowTarget. */
	UPROPERTY(config, EditAnywhere, Category = "Debug")
	FColor TargetLineColor = FColor(255, 60, 60, 255);

	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TargetLineThickness = 4.0f;

	/** Height above the grid plane the range circle is drawn at. */
	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ForceUnits = "cm"))
	float RangeDrawHeightOffset = 20.0f;

	//~ Aim ------------------------------------------------------------------------
	// Every defender today fires shot by shot, and a shot-by-shot weapon looks again
	// before each shot: the nearest creep in range then, switching from the one it held
	// only for one clearly nearer. A continuous weapon (a laser, later) will hold its
	// creep instead, since keeping it is what pays.

	/** Re-picks the nearest creep before every shot. Off, a defender keeps its creep while it is in range, and picks the one furthest along. */
	UPROPERTY(config, EditAnywhere, Category = "Aim")
	bool bDynamicAim = true;

	/** How much nearer another creep must be to be switched to: 0.15 is 15% nearer than the one held. */
	UPROPERTY(config, EditAnywhere, Category = "Aim", meta = (ClampMin = "0.0", ClampMax = "0.9", EditCondition = "bDynamicAim"))
	float AimSwitchMargin = 0.15f;
};

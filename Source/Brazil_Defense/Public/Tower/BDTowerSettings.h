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

	/** Color of the range circle drawn by BD.Tower.ShowRange. */
	UPROPERTY(config, EditAnywhere, Category = "Debug")
	FColor RangeColor = FColor(80, 200, 255, 255);

	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "3", UIMin = "3"))
	int32 RangeSegments = 48;

	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RangeThickness = 4.0f;

	/** Color of the line from a tower to its target drawn by BD.Tower.ShowTarget. */
	UPROPERTY(config, EditAnywhere, Category = "Debug")
	FColor TargetLineColor = FColor(255, 60, 60, 255);

	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TargetLineThickness = 4.0f;

	/** Height above the grid plane the range circle is drawn at. */
	UPROPERTY(config, EditAnywhere, Category = "Debug", meta = (ForceUnits = "cm"))
	float RangeDrawHeightOffset = 20.0f;
};

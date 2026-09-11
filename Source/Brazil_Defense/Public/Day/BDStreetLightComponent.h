// Brazil Defense. A street light: scenery that lights up at dusk and stands in the way.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Grid/BDGridTypes.h"
#include "BDStreetLightComponent.generated.h"

class UBDGridSubsystem;

/**
 * Put this on a lamp post in the level and it becomes two things at once: a light the
 * day cycle turns up as the sun goes down, and a Blocked cell the creeps have to walk
 * around. Scenery that lies about its footprint is the fastest way to a path that looks
 * wrong, so occupying the cell is the default rather than an option someone remembers.
 *
 * The light itself is whatever ULightComponent the owning actor already carries, so the
 * art side stays an ordinary point or spot light.
 */
UCLASS(ClassGroup = (BrazilDefense), meta = (BlueprintSpawnableComponent, DisplayName = "BD Street Light"))
class BRAZIL_DEFENSE_API UBDStreetLightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBDStreetLightComponent();

	//~ Begin UActorComponent interface
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent interface

	/** Scales the light by what the day cycle asked for, 0 dark to 1 fully lit. */
	void ApplyCycleIntensity(float Multiplier);

	/** Intensity of the light when the cycle has it fully on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Day", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LitIntensity = 5000.0f;

	/** Whether the cell under this light is marked Blocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Day")
	bool bOccupiesCell = true;

private:
	/** Marks the cell under the owner as permanent scenery. */
	void MarkCellBlocked();

	UBDGridSubsystem* GetGrid() const;

	FBDCellCoord OccupiedCell;
	bool bHasOccupiedCell = false;
};

// Brazil Defense. A street light: scenery that lights up at dusk and stands in the way.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Grid/BDGridTypes.h"
#include "BDStreetLightComponent.generated.h"

class UBDGridSubsystem;
class ULightComponent;
class UMeshComponent;

/**
 * Put this on a lamp post in the level and it becomes two things at once: a light the
 * day cycle turns up as the sun goes down, and a Blocked cell the creeps have to walk
 * around. Scenery that lies about its footprint is the fastest way to a path that looks
 * wrong, so occupying the cell is the default rather than an option someone remembers.
 *
 * The lights are whatever ULightComponents the owning actor already carries, so the art
 * side stays ordinary point or spot lights. Each keeps the intensity it was given in the
 * editor as its fully lit value, and the cycle scales it from there. ABDStreetLamp is the
 * ready-made post that carries one.
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

	/** Scales the lights by what the day cycle asked for, 0 dark to 1 fully lit. */
	void ApplyCycleIntensity(float Multiplier);

	/** Last multiplier the cycle applied, -1 before the first. */
	float GetAppliedMultiplier() const { return AppliedMultiplier; }

	/**
	 * Whether the lights cast dynamic shadows. Off by default: a street of shadowed spots
	 * is the most expensive light in the level, and the board reads fine without them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Day")
	bool bLightsCastShadows = false;

	/**
	 * Scalar parameter of the post's materials scaled with the lights, so the lamp heads
	 * glow at night and go dark by day. Materials without it are left alone. None skips it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Day")
	FName GlowParameter = TEXT("Light Force");

	/** Whether the cell under this light is marked Blocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brazil Defense|Day")
	bool bOccupiesCell = true;

private:
	/** Marks the cell under the owner as permanent scenery. */
	void MarkCellBlocked();

	/** Takes what the editor gave the lights and the glow as their fully lit values. */
	void CaptureLitValues();

	UBDGridSubsystem* GetGrid() const;

	struct FLitLight
	{
		TWeakObjectPtr<ULightComponent> Light;
		float LitIntensity = 0.0f;
	};

	struct FLitGlow
	{
		TWeakObjectPtr<UMeshComponent> Mesh;
		float LitValue = 0.0f;
	};

	TArray<FLitLight> LitLights;
	TArray<FLitGlow> LitGlows;
	bool bCapturedLitValues = false;
	float AppliedMultiplier = -1.0f;

	FBDCellCoord OccupiedCell;
	bool bHasOccupiedCell = false;
};

// Brazil Defense. The sky, driven by how far the match has got.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BDDayCycleComponent.generated.h"

class UBDStreetLightComponent;
class UDirectionalLightComponent;
class UCurveFloat;
class UCurveLinearColor;

/**
 * Moves the sun across the match.
 *
 * The position in the day comes from the wave number, never from the clock. Two reasons,
 * both of them the same reason: with time dilation at 4x a real time cycle would race,
 * and two players on wave 12 would be looking at different skies. Wave 12 should look
 * like wave 12. Only the blend between one wave and the next follows the clock, so the
 * sun slides instead of snapping.
 *
 * Everything the cycle does is read off curves, so the look is authored in the curve
 * editor rather than compiled in. BD.Day.SetAlpha sweeps the cycle by hand for that.
 */
/** The part of the day the sun is in, for the HUD. */
UENUM(BlueprintType)
enum class EBDDayPhase : uint8
{
	Sunrise,
	Day,
	Sunset,
	Dusk,
	Night
};

UCLASS(ClassGroup = (BrazilDefense), meta = (BlueprintSpawnableComponent, DisplayName = "BD Day Cycle"))
class BRAZIL_DEFENSE_API UBDDayCycleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBDDayCycleComponent();

	//~ Begin UActorComponent interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent interface

	/** The phase an hour of the day falls in. */
	static EBDDayPhase PhaseForHour(float Hour);

	/** Where in the day the cycle currently stands, 0 to 1. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Day")
	float GetCycleAlpha() const { return CycleAlpha; }

	/** Whether the sun is on its way to the position of the current wave. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Day")
	bool IsBlending() const { return !FMath::IsNearlyEqual(CycleAlpha, TargetAlpha); }

	/** Real seconds since the sun last stopped moving; large while it moves. */
	float GetSecondsSinceBlendEnded() const { return SecondsSinceBlendEnded; }

	/** Hour of the day, 0 to 24, where the cycle stands. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Day")
	float GetHour() const;

	/** The phase the hour falls in, by the day settings. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Day")
	EBDDayPhase GetPhase() const;

	/** "HH:MM" for the current hour. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Day")
	FText GetClockText() const;

	/** Aims the cycle at the position a wave implies. Blended, not snapped. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Day")
	void SetWave(int32 Wave);

	/** Puts the cycle straight at an alpha, no blend. Authoring the curves without playing. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Day")
	void SetAlphaImmediate(float Alpha);

	/** Street lights announce themselves so the cycle can light them. */
	void RegisterStreetLight(UBDStreetLightComponent* Light);
	void UnregisterStreetLight(UBDStreetLightComponent* Light);

	/** Waves it takes to come back round to dawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cycle", meta = (ClampMin = "1", UIMin = "1"))
	int32 WavesPerCycle = 16;

	/** How fast the cycle slides towards the position of a new wave, in alpha per real second: the game speed does not hurry the sun. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cycle", meta = (ClampMin = "0.001", UIMin = "0.001"))
	float BlendSpeed = 0.03f;

	//~ Curves ----------------------------------------------------------------

	/** Sun pitch in degrees, by alpha. Around -90 is overhead, 0 is the horizon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curves")
	TObjectPtr<UCurveFloat> SunPitchByAlpha;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curves")
	TObjectPtr<UCurveFloat> SunIntensityByAlpha;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curves")
	TObjectPtr<UCurveLinearColor> SunColorByAlpha;

	/** Multiplier applied to every registered street light, by alpha. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Curves")
	TObjectPtr<UCurveFloat> StreetLightIntensityByAlpha;

	//~ Scene ------------------------------------------------------------------

	/**
	 * Actor carrying the sun. Left unset, the first actor in the world with a directional
	 * light component is used, which covers a plain directional light, a Sun and Sky rig
	 * and any Blueprint that wraps one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene")
	TSoftObjectPtr<AActor> SunActor;

	/** Compass direction the sun travels along. Not curve driven: it is a level decision. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scene", meta = (ClampMin = "-360.0", ClampMax = "360.0"))
	float SunYaw = -45.0f;

private:
	/** Fills any curve left unset from the project wide defaults. */
	void ResolveDefaultCurves();

	/** Alpha a wave maps to. */
	float ComputeAlphaForWave(int32 Wave) const;

	/** Pushes the current alpha onto the sun and the street lights. */
	void ApplyCycle();

	/** Resolves SunActor, falling back to the first directional light component in the world. */
	UDirectionalLightComponent* ResolveSunLight();

	UPROPERTY(Transient)
	TWeakObjectPtr<UDirectionalLightComponent> ResolvedSun;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UBDStreetLightComponent>> StreetLights;

	/** Where the cycle is now, and where the current wave says it should end up. */
	float CycleAlpha = 0.0f;
	float TargetAlpha = 0.0f;

	/** See GetSecondsSinceBlendEnded. */
	float SecondsSinceBlendEnded = 1.0e6f;

	/** Last wave handed in, kept so BD.Day.Freeze 0 resumes at the right sky. */
	int32 LastWave = 0;
	bool bWasFrozen = false;
};

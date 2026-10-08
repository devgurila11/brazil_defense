// Brazil Defense. The green stain a creep leaves on the ground where it falls.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDBloodDecals.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;

/**
 * How the stains look and how long they stay. The picture is BloodTexture, painted
 * through DecalMaterial (M_BloodDecal: the texture's colour times Tint, its alpha as the
 * opacity, faded out by the decal's own lifetime). Empty BloodTexture leaves no stain at
 * all: the slot waits for the art.
 *
 * Edited in Project Settings > Game > Brazil Defense - Blood.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Blood"))
class BRAZIL_DEFENSE_API UBDBloodSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDBloodSettings();

	static const UBDBloodSettings& Get();

	/** The stain. Its alpha cuts the shape. Empty: no stain is left. */
	UPROPERTY(config, EditAnywhere, Category = "Blood")
	TSoftObjectPtr<UTexture2D> BloodTexture;

	/** A deferred decal material with a BloodTexture parameter, a Tint parameter and the decal lifetime opacity. */
	UPROPERTY(config, EditAnywhere, Category = "Blood")
	TSoftObjectPtr<UMaterialInterface> DecalMaterial;

	/** Multiplies the texture's colour: green, the militants' blood. */
	UPROPERTY(config, EditAnywhere, Category = "Blood")
	FLinearColor Tint = FLinearColor(0.12f, 0.55f, 0.08f, 1.0f);

	/** Width of a stain on the ground. */
	UPROPERTY(config, EditAnywhere, Category = "Blood", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float Size = 180.0f;

	/** Each stain is scaled by a random factor within this share either way, so no two match. */
	UPROPERTY(config, EditAnywhere, Category = "Blood", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float SizeJitter = 0.25f;

	/** How far up and down from the ground the stain reaches: enough for a kerb, not a wall. */
	UPROPERTY(config, EditAnywhere, Category = "Blood", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float Depth = 60.0f;

	/** Seconds the stain stays whole, game time. */
	UPROPERTY(config, EditAnywhere, Category = "Blood", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float HoldTime = 3.0f;

	/** Seconds it takes to fade away once the hold is over: a fade, never a cut. */
	UPROPERTY(config, EditAnywhere, Category = "Blood", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float DecalFadeTime = 2.0f;

	/**
	 * Most stains on the ground at once. Past it the oldest fades out fast (OverflowFadeTime)
	 * for the new one: a dense wave leaves a trail, not a carpet.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Blood", meta = (ClampMin = "0", UIMax = "128"))
	int32 MaxDecals = 24;

	UPROPERTY(config, EditAnywhere, Category = "Blood", meta = (ClampMin = "0.05", ForceUnits = "s"))
	float OverflowFadeTime = 0.3f;
};

/**
 * Leaves the stains and keeps them few. One decal per creep brought down, lying on the
 * ground traced under it, at a random turn; it holds, fades and is gone on its own, and
 * past MaxDecals the oldest is sent fading early.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDBloodDecalSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBDBloodDecalSubsystem* Get(const UObject* WorldContext);

	/**
	 * A stain on the ground under a point. Does nothing without a texture or a material.
	 * @return the decal left, or null.
	 */
	UDecalComponent* SpawnAt(const FVector& Where);

	/** Stains on the ground now, the fading ones included. */
	int32 GetLiveCount();

	/** Stains asked for, left, and sent fading early to make room. */
	int32 GetRequested() const { return Requested; }
	int32 GetSpawned() const { return Spawned; }
	int32 GetEvicted() const { return Evicted; }

	/** For the regression: a texture to paint with whatever the settings say. Null goes back to the settings. */
	void SetTextureOverride(UTexture2D* Texture) { TextureOverride = Texture; }

private:
	/** Drops the decals already gone from the list. */
	void Prune();

	/** Oldest first. Weak: each decal destroys itself at the end of its fade. */
	TArray<TWeakObjectPtr<UDecalComponent>> Live;

	/** Decals already sent fading early, so the next overflow takes the next one. */
	TSet<TWeakObjectPtr<UDecalComponent>> Evicting;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TextureOverride;

	/** The material every stain wears. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Painted;

	int32 Requested = 0;
	int32 Spawned = 0;
	int32 Evicted = 0;
};

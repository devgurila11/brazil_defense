// Brazil Defense. The show the bribe puts on: the bag, the coins and the till.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BDBribeSettings.generated.h"

class USoundBase;
class UStaticMesh;

/**
 * Everything the conversion of a bribe into public money looks and sounds like, kept out
 * of the code so it can be tuned without a recompile. The economy itself is not here: the
 * rate, the refund and the ladder live in UBDGameBalanceSettings, where the rest of the
 * balance is. Edited in Project Settings > Game > Brazil Defense - Bribe.
 *
 * The asset slots start empty on purpose. Without them the sequence still runs, silently
 * and invisibly, and the counters still move: a headless check of the economy never waits
 * on art.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Bribe"))
class BRAZIL_DEFENSE_API UBDBribeSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDBribeSettings();

	static const UBDBribeSettings& Get();

	//~ The bag ----------------------------------------------------------------
	// It falls out of the dead candidate and bounces like a ball: each touch of the
	// ground takes a share of the speed and rings the coins inside, until what is left
	// is too little to lift it and it rests. Then it fades.

	/** Money bag dropped over the body. Optional: without it the sequence runs unseen. */
	UPROPERTY(config, EditAnywhere, Category = "Money Bag", meta = (AllowedClasses = "/Script/Engine.StaticMesh"))
	TSoftObjectPtr<UStaticMesh> MoneyBagMesh;

	UPROPERTY(config, EditAnywhere, Category = "Money Bag")
	FVector MoneyBagScale = FVector(1.0f, 1.0f, 1.0f);

	/** How high above the body the bag appears before it falls. */
	UPROPERTY(config, EditAnywhere, Category = "Money Bag", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float DropHeight = 450.0f;

	/** Pull on the bag. The board's own gravity is not used: this one is tuned for the bounce, not for physics. */
	UPROPERTY(config, EditAnywhere, Category = "Money Bag", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float Gravity = 2400.0f;

	/** Share of the speed a bounce keeps. 0.55 gives four or five audible touches. */
	UPROPERTY(config, EditAnywhere, Category = "Money Bag", meta = (ClampMin = "0.0", ClampMax = "0.95", UIMin = "0.0", UIMax = "0.95"))
	float Restitution = 0.55f;

	/** Below this landing speed the bag stops instead of bouncing again. */
	UPROPERTY(config, EditAnywhere, Category = "Money Bag", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MinBounceSpeed = 80.0f;

	/** Turns per second while it falls, so it reads as thrown rather than dropped. */
	UPROPERTY(config, EditAnywhere, Category = "Money Bag", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpinRate = 120.0f;

	/** Seconds from the drop to the bag being gone, fade included. */
	UPROPERTY(config, EditAnywhere, Category = "Money Bag", meta = (ClampMin = "0.1", UIMin = "0.1", ForceUnits = "s"))
	float BagSeconds = 3.0f;

	//~ The counters -----------------------------------------------------------

	/** Seconds the thief's counter takes to climb the whole bribe. */
	UPROPERTY(config, EditAnywhere, Category = "Counters", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float CountSeconds = 1.2f;

	/** Seconds the value takes to come off the thief's counter and land on the mint's. */
	UPROPERTY(config, EditAnywhere, Category = "Counters", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float TransferSeconds = 0.9f;

	/** Pause between the count finishing and the transfer starting: the beat the till rings in. */
	UPROPERTY(config, EditAnywhere, Category = "Counters", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float RegisterSeconds = 0.35f;

	//~ Sound ------------------------------------------------------------------
	// Both are 3D, at the candidate's own position, routed to the effects class so the
	// options slider owns them, and attenuated over the board like the urn's beep: the
	// listener is the match camera, so the radii are wide.

	/** Coins colliding, once per touch of the ground. */
	UPROPERTY(config, EditAnywhere, Category = "Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	TSoftObjectPtr<USoundBase> CoinDropSound;

	/** The till, when the count finishes and the money turns public. */
	UPROPERTY(config, EditAnywhere, Category = "Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	TSoftObjectPtr<USoundBase> CashRegisterSound;

	UPROPERTY(config, EditAnywhere, Category = "Sound", meta = (ClampMin = "0.0", ClampMax = "4.0", UIMin = "0.0", UIMax = "4.0"))
	float CoinDropVolume = 1.0f;

	UPROPERTY(config, EditAnywhere, Category = "Sound", meta = (ClampMin = "0.0", ClampMax = "4.0", UIMin = "0.0", UIMax = "4.0"))
	float CashRegisterVolume = 1.0f;

	/** Each touch is quieter than the last, down to this share at the final one. */
	UPROPERTY(config, EditAnywhere, Category = "Sound", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float CoinDropFinalVolume = 0.35f;

	/** Inside this radius the sound plays at full volume. */
	UPROPERTY(config, EditAnywhere, Category = "Sound", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float SoundInnerRadius = 6000.0f;

	/** Past the inner radius it fades over this distance, down to SoundAttenuationAtMax. */
	UPROPERTY(config, EditAnywhere, Category = "Sound", meta = (ClampMin = "100.0", UIMin = "100.0", ForceUnits = "cm"))
	float SoundFalloffDistance = 28000.0f;

	UPROPERTY(config, EditAnywhere, Category = "Sound", meta = (ClampMin = "-90.0", ClampMax = "0.0", UIMin = "-90.0", UIMax = "0.0"))
	float SoundAttenuationAtMax = -40.0f;

	/** Most coin drops sounding at once before the oldest gives way. */
	UPROPERTY(config, EditAnywhere, Category = "Sound", meta = (ClampMin = "1", ClampMax = "16", UIMin = "1", UIMax = "16"))
	int32 SoundMaxConcurrent = 6;
};

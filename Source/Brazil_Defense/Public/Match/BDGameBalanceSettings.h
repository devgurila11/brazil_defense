// Brazil Defense. Pacing of a match: countdowns, refunds and game speed.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Match/BDMatchTypes.h"
#include "BDGameBalanceSettings.generated.h"

class UBDDifficultyData;

/**
 * Everything about how a match paces itself, kept out of the code so it can be tuned
 * without a recompile. Edited in Project Settings > Game > Brazil Defense - Balance.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Balance"))
class BRAZIL_DEFENSE_API UBDGameBalanceSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDGameBalanceSettings();

	static const UBDGameBalanceSettings& Get();

	/** The difficulty asset to use for each difficulty. */
	UPROPERTY(config, EditAnywhere, Category = "Difficulty", meta = (AllowedClasses = "/Script/Brazil_Defense.BDDifficultyData"))
	TMap<EBDDifficulty, TSoftObjectPtr<UBDDifficultyData>> DifficultyAssets;

	/** Difficulty a match starts on when nothing picked one. */
	UPROPERTY(config, EditAnywhere, Category = "Difficulty")
	EBDDifficulty DefaultDifficulty = EBDDifficulty::Normal;

	//~ Wave countdown --------------------------------------------------------
	// Delay = Max(MinWaveDelay, BaseWaveDelay * DecayRate ^ Wave). The countdown between
	// waves shrinks as the match goes on, so the pressure builds without a hand authored
	// table of per wave timings.

	UPROPERTY(config, EditAnywhere, Category = "Waves", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float BaseWaveDelay = 45.0f;

	UPROPERTY(config, EditAnywhere, Category = "Waves", meta = (ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0"))
	float WaveDelayDecayRate = 0.94f;

	UPROPERTY(config, EditAnywhere, Category = "Waves", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float MinWaveDelay = 10.0f;

	/** Bonus granted per second of countdown skipped when the player calls a wave early. */
	UPROPERTY(config, EditAnywhere, Category = "Waves", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float EarlyCallBonusPerSecond = 2.0f;

	//~ Removal rules ---------------------------------------------------------

	/**
	 * Last wave on which a divider may still be taken back, at no refund.
	 *
	 * Dividers turn permanent when the first wave goes out, which is the point of the
	 * building phase. The grace window exists because the first waves are also the only
	 * information the player has about their own maze: losing a match to a choice made
	 * blind is not difficulty. Refund is zero so it stays a correction, not a strategy.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Removal", meta = (ClampMin = "0", UIMin = "0"))
	int32 DividerRemovalGraceWave = 3;

	/** Fraction refunded when taking a piece back during the building phase, before wave 1. */
	UPROPERTY(config, EditAnywhere, Category = "Removal", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BuildingPhaseRefundRatio = 1.0f;

	//~ Game speed ------------------------------------------------------------

	/**
	 * Speeds the player may switch between. Applied with global time dilation, never by
	 * multiplying deltas in individual systems: one clock means one place to be wrong.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Speed")
	TArray<float> AllowedGameSpeeds = { 1.0f, 2.0f, 4.0f };

	/** Whether the requested speed is one of the allowed ones. */
	bool IsGameSpeedAllowed(float Speed) const;

	/** Countdown before a given wave, in seconds. */
	float GetWaveDelay(int32 Wave) const;

	/** Resolves the asset for a difficulty, loading it if needed. Null when unconfigured. */
	const UBDDifficultyData* FindDifficultyData(EBDDifficulty Difficulty) const;
};

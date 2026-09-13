// Brazil Defense. Pacing of a match: countdowns, refunds and game speed.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Grid/BDGridTypes.h"
#include "Match/BDMatchTypes.h"
#include "BDGameBalanceSettings.generated.h"

class UBDDifficultyData;
class UCurveFloat;

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

	//~ Selling ---------------------------------------------------------------
	// Nothing on the board is permanent. Any piece can be sold at any point; what changes
	// is how much of its build cost comes back in blue votes: all of it before the first
	// wave, when the player is still trying things out, and a share of it afterwards.

	/**
	 * Last wave on which a divider still sells for the full refund rather than the
	 * reduced one. The first waves are the only information the player has about their
	 * own maze, and a match lost to a fence placed blind is not difficulty. 0 gives
	 * dividers no window.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Removal", meta = (ClampMin = "0", UIMin = "0"))
	int32 DividerRemovalGraceWave = 3;

	/** Fraction refunded when selling a piece before wave 1, and a divider inside its grace window. */
	UPROPERTY(config, EditAnywhere, Category = "Removal", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float BuildingPhaseRefundRatio = 1.0f;

	/** Fraction of the build cost paid back in blue votes when a piece is sold once the first wave has gone out. */
	UPROPERTY(config, EditAnywhere, Category = "Removal", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float SellRefundRatio = 0.5f;

	/** Fraction of the build cost paid back for selling a piece of this kind on a given wave. */
	float GetSellRefundRatio(EBDPieceKind Kind, int32 Wave) const;

	//~ The red candidate ------------------------------------------------------
	// The climax of a match. When the red counter passes the blue one, a candidate walks
	// out of a mouth: slow, huge, shot by every defender that sees it. If it reaches the
	// urn the match is lost. If it dies the count freezes for a while and the waves hold,
	// and it comes back on the next wave if the scoreboard is still inverted.

	/** Health of the candidate as a multiple of the health of the creeps of the wave it comes out on. */
	UPROPERTY(config, EditAnywhere, Category = "Candidate", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float CandidateHealthMultiplier = 40.0f;

	/** Walking speed of the candidate, in cells per second. 0 leaves the speed of its enemy data. */
	UPROPERTY(config, EditAnywhere, Category = "Candidate", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CandidateSpeed = 0.5f;

	/** Seconds the waves hold and the red count stays frozen after the candidate is killed. */
	UPROPERTY(config, EditAnywhere, Category = "Candidate", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float CandidateKillPauseSeconds = 30.0f;

	//~ Wave scaling -----------------------------------------------------------

	/**
	 * Creep health multiplier by wave: effective health = MaxHealth x curve(Wave). A
	 * curve so it is tuned in the graph, not in code. When no curve is set the fallback
	 * is HealthScaleGrowth ^ (Wave - 1): with 10 health against 10 damage that is one
	 * shot on wave 1, two on wave 5, three on wave 10, nine on wave 20.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Wave Scaling", meta = (AllowedClasses = "/Script/Engine.CurveFloat"))
	TSoftObjectPtr<UCurveFloat> HealthScaleByWave;

	/** Per wave growth of the fallback exponential, used when HealthScaleByWave is unset. */
	UPROPERTY(config, EditAnywhere, Category = "Wave Scaling", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float HealthScaleGrowth = 1.12f;

	//~ Spawn pacing ----------------------------------------------------------
	// Interval = Max(WaveSpawnIntervalMin, WaveSpawnIntervalBase x WaveSpawnIntervalDecay ^ Wave).
	// A wave has to arrive as a mass, not as a thread: at one creep a second the defense
	// kills them at the rate they come in and the peak never builds. The interval shrinks
	// with the wave so the late ones pile up on purpose.

	UPROPERTY(config, EditAnywhere, Category = "Wave Scaling", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float WaveSpawnIntervalBase = 0.35f;

	UPROPERTY(config, EditAnywhere, Category = "Wave Scaling", meta = (ClampMin = "0.01", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0"))
	float WaveSpawnIntervalDecay = 0.97f;

	UPROPERTY(config, EditAnywhere, Category = "Wave Scaling", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float WaveSpawnIntervalMin = 0.2f;

	/** Seconds between two creeps of a wave. */
	float GetWaveSpawnInterval(int32 Wave) const;

	/** Creeps each spawn point sends on wave 1. */
	UPROPERTY(config, EditAnywhere, Category = "Wave Scaling", meta = (ClampMin = "1", UIMin = "1"))
	int32 CreepsPerSpawnPointBase = 1;

	/** Extra creeps per spawn point on every wave after the first: per point = Base + Step x (Wave - 1). */
	UPROPERTY(config, EditAnywhere, Category = "Wave Scaling", meta = (ClampMin = "0", UIMin = "0"))
	int32 CreepsPerSpawnPointStep = 1;

	/** Health multiplier for the creeps of a wave. */
	float GetHealthScale(int32 Wave) const;

	/** How many creeps each spawn point sends on a wave. The total is that times the number of points. */
	int32 GetCreepsPerSpawnPoint(int32 Wave) const;

	//~ Active spawn points ----------------------------------------------------
	// Which mouths open is drawn per wave, from the seed of the match. Every mouth every
	// wave tells the player where the horde comes from before it comes; one wave out of
	// a single mouth and the next out of five is the same horde arriving somewhere else.
	//
	// The size of a wave does not follow the draw: the total is the growing number it
	// always was, split over the mouths that opened. Fewer mouths is a thicker stream,
	// never a smaller wave.

	/** Fewest mouths a wave may come out of. Clamped to the mouths that have a route. */
	UPROPERTY(config, EditAnywhere, Category = "Wave Scaling", meta = (ClampMin = "1", UIMin = "1"))
	int32 MinActiveSpawnPoints = 1;

	/** Most mouths a wave may come out of. 0 means every mouth of the board. */
	UPROPERTY(config, EditAnywhere, Category = "Wave Scaling", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxActiveSpawnPoints = 0;

	//~ Upgrades ---------------------------------------------------------------
	// Cost of level N = UpgradeCostBase x UpgradeCostGrowth ^ (N - 1); damage at level N =
	// Damage x (1 + DamageGrowthPerLevel x (N - 1)). Calibrated so a level 5 defender costs
	// a few hundred votes, payable inside one match. Exponential cost against linear damage
	// makes stacking the same defender expensive on its own, so spreading out becomes the
	// right move without forbidding anything. Paid in blue votes, which are the score: an
	// upgrade is bought with the scoreboard, on purpose.

	UPROPERTY(config, EditAnywhere, Category = "Upgrades", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float UpgradeCostGrowth = 1.35f;

	UPROPERTY(config, EditAnywhere, Category = "Upgrades", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DamageGrowthPerLevel = 0.60f;

	/** Blue votes it costs to bring a defender of this upgrade cost base to a level (2 and up). */
	int32 GetUpgradeCost(int32 UpgradeCostBase, int32 Level) const;

	/** Damage multiplier of a level, 1.0 at level 1. */
	float GetUpgradeDamageScale(int32 Level) const;

	//~ Moving pieces between waves -------------------------------------------
	// Rate = Min(MoveTaxMax, MoveTaxInitial + MoveTaxStep * Wave), charged on the build
	// cost of the piece in blue votes. A percentage rather than a table: moving a cheap
	// shooter stays cheap and moving a cannon costs, with nothing else to author. The
	// early waves are close to free so the player learns the maze without being punished.

	UPROPERTY(config, EditAnywhere, Category = "Moving", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MoveTaxInitial = 0.0f;

	UPROPERTY(config, EditAnywhere, Category = "Moving", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MoveTaxStep = 0.02f;

	UPROPERTY(config, EditAnywhere, Category = "Moving", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MoveTaxMax = 0.25f;

	/** Fraction of the build cost charged for moving a piece on a given wave. */
	float GetMoveTaxRate(int32 Wave) const;

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

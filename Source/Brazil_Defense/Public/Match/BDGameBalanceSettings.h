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
	// is how much of what it was bought for comes back in public money: all of it before
	// the first wave, when the player is still trying things out, and a share of it
	// afterwards. The share is of the price paid, never of today's price: a piece bought
	// on wave 5 and sold on wave 80 must not turn a profit.

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

	/** Fraction of the price paid that comes back in public money when a piece is sold once the first wave has gone out. */
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

	/** A candidate walks out with every wave that is a multiple of this: 5 makes twenty over a hundred waves. 0 turns the schedule off. */
	UPROPERTY(config, EditAnywhere, Category = "Candidate", meta = (ClampMin = "0", UIMin = "0"))
	int32 CandidateInterval = 5;

	/**
	 * Seconds the creeps of a candidate's wave wait after he walks out. He is the first
	 * thing out of the buses on his wave, so the player sees him and can make him the
	 * priority; walking out in the middle of the horde, he went by unnoticed.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Candidate", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float CandidateLeadSeconds = 5.0f;

	/** Health of a scheduled candidate on a wave, for a wave creep of this base health. */
	float GetCandidateHealth(float CreepHealth, int32 Wave) const;

	// A scheduled candidate killed pays in bribe, and in nothing else. There are no
	// ceilings left for him to raise: defenders are held back by votes and by the board,
	// so what a boss is worth is the bag that falls out of him. The fallen who come back
	// with the count drop nothing: the return is a punishment, and a reward on it would
	// make the count worth throwing.

	/** Seconds the return of the fallen is spread over when the count turns red: the parade's length. */
	UPROPERTY(config, EditAnywhere, Category = "Candidate", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "s"))
	float ReturnParadeSeconds = 30.0f;

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

	//~ Votes by health ------------------------------------------------------------
	// A vote is worth health: a creep that reaches the urn scores floor(MaxHealth / K)
	// red, so a leak on wave 84 costs hundreds where one on wave 3 costs a few. Blue
	// scores the same way for a kill, so both sides grow at the same pace and an early
	// lead settles nothing. Wasted damage (overkill, lost shots) counts as null votes
	// at the same rate: shown, never scored.

	/** Health per vote, both sides. 1 makes a 10 hp creep worth 10 votes. */
	UPROPERTY(config, EditAnywhere, Category = "Votes", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float HealthPerVote = 1.0f;

	/** Blue by health too (true) or one flat VotesOnDeath per kill (false). */
	UPROPERTY(config, EditAnywhere, Category = "Votes")
	bool bBlueVotesByHealth = true;

	/**
	 * Weight on the red side alone. Both counters read health through HealthPerVote, so
	 * that number moves the whole board and never the balance between the sides; this is
	 * the one that decides how heavily a leak lands against how heavily a kill lands.
	 *
	 * It exists because of what the simulation showed: while the defense holds, almost
	 * nothing reaches the urn and red sits near zero, which is right - defending well IS
	 * winning the election. What was missing is for the late leaks, when the defense
	 * finally gives, to weigh enough that the count is a contest at the end rather than a
	 * formality. Scale, not a new rule: a creep still scores only by arriving.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Votes", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float RedVoteWeight = 1.0f;

	/** Votes a creep of this much health is worth. At least 1. */
	int32 VotesForHealth(float Health) const;

	/** Red votes a creep of this much health scores by reaching the urn: the health rate, times the red weight. */
	int32 RedVotesForHealth(float Health) const;

	//~ Wandering mouths ---------------------------------------------------------
	// The buses do not park: before a wave goes out each mouth may slide along its edge
	// of the board by a few cells, so the maze the player built for one entrance meets
	// the horde a little to the side of it. Never off the edge, never onto a taken cell,
	// never into another mouth, and never somewhere without a route to the urn.

	/** Chance, per mouth per wave, that it moves at all. 0 parks them. */
	UPROPERTY(config, EditAnywhere, Category = "Wandering Mouths", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MouthWanderChance = 0.75f;

	/** Farthest a mouth slides in one wave, in cells along its edge. */
	UPROPERTY(config, EditAnywhere, Category = "Wandering Mouths", meta = (ClampMin = "0", UIMin = "0"))
	int32 MouthWanderMaxCells = 5;

	//~ Balance reference --------------------------------------------------------
	// Numbers BD.Balance.Report uses to compare the horde of a wave with a fully developed
	// defense: so many defenders, every one of them at ReferenceMaxLevel, hitting a
	// fraction of the time creeps spend on the route.
	//
	// The two counts are a measuring stick, not a game rule: nothing limits how many
	// defenders the player builds any more, so the report has to be told what "a full
	// board" means before it can say whether a wave is winnable. They start at what the
	// old tower and character ceilings were, so a report taken now can be read against
	// the rounds already recorded.

	/** Level a fully developed defense is assumed to reach. */
	UPROPERTY(config, EditAnywhere, Category = "Balance Reference", meta = (ClampMin = "1", UIMin = "1"))
	int32 ReferenceMaxLevel = 5;

	/** Ground towers a fully developed defense is assumed to stand on the board. */
	UPROPERTY(config, EditAnywhere, Category = "Balance Reference", meta = (ClampMin = "0", UIMin = "0"))
	int32 ReferenceTowerCount = 8;

	/** Characters a fully developed defense is assumed to have mounted on its platforms. */
	UPROPERTY(config, EditAnywhere, Category = "Balance Reference", meta = (ClampMin = "0", UIMin = "0"))
	int32 ReferenceCharacterCount = 12;

	/** Fraction of the route time a defender is assumed to spend actually hitting something. */
	UPROPERTY(config, EditAnywhere, Category = "Balance Reference", meta = (ClampMin = "0.05", ClampMax = "1.0", UIMin = "0.05", UIMax = "1.0"))
	float ReferenceEngagementEfficiency = 0.5f;

	//~ Upgrades ---------------------------------------------------------------
	// Cost of level N = UpgradeCostBase x UpgradeCostGrowth ^ (N - 1), at wave 1 prices;
	// on a later wave it climbs with the price scale, like a piece does, so a level keeps
	// its weight against what a boss drops all match long. Damage at level N =
	// Damage x (1 + DamageGrowthPerLevel x (N - 1)). Exponential cost against linear damage
	// makes stacking the same defender expensive on its own, so spreading out becomes the
	// right move without forbidding anything. Paid in public money, like everything the
	// player buys. UBDTowerData::MaxLevels caps the ladder at five.

	UPROPERTY(config, EditAnywhere, Category = "Upgrades", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float UpgradeCostGrowth = 1.35f;

	UPROPERTY(config, EditAnywhere, Category = "Upgrades", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float DamageGrowthPerLevel = 0.60f;

	/** Public money it costs to bring a defender of this upgrade cost base to a level (2 and up), at wave 1 prices. */
	int32 GetUpgradeCost(int32 UpgradeCostBase, int32 Level) const;

	/** The same, bought on a given wave. */
	int32 GetUpgradeCostOnWave(int32 UpgradeCostBase, int32 Level, int32 Wave) const;

	/** Damage multiplier of a level, 1.0 at level 1. */
	float GetUpgradeDamageScale(int32 Level) const;

	/** Everything a defender of this upgrade cost base costs to reach a level, at wave 1 prices: levels 2 to Level. 0 at level 1. */
	int32 GetEvolutionSpent(int32 UpgradeCostBase, int32 Level) const;

	//~ The bribe -----------------------------------------------------------------
	// The currency of the match. A scheduled candidate killed drops the bribe he stole -
	// floor(MaxHealth / HealthPerBribe), so the late bosses are worth many times the early
	// ones - the thief's counter takes it, and the mint turns it into public money, which
	// is what every piece and every level is bought with. Votes are the score and only
	// the score: nothing the player buys or sells ever moves them.
	//
	// The shape the curve is calibrated for: every boss buys something, the middle of the
	// match is short of money on purpose, and the whole defense at the top level only
	// becomes payable in the last stretch. BD.Bribe.Report prints where a given
	// HealthPerBribe actually lands that. The fallen who come back drop nothing, like the
	// budget they do not raise.

	/** Health per unit of bribe. 2.5 makes a 12,000 hp boss worth 4,800. */
	UPROPERTY(config, EditAnywhere, Category = "Bribe", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float HealthPerBribe = 2.5f;

	/** Fraction of the public money sunk into a defender's levels that comes back when it is taken off the board. */
	UPROPERTY(config, EditAnywhere, Category = "Bribe", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float EvolutionRefundRatio = 0.6f;

	/** Bribe a candidate of this much health drops. At least 1. */
	int32 BribeForHealth(float Health) const;

	/** Public money a defender that has sunk this much into its levels pays back when it is sold. */
	int32 GetEvolutionRefund(int32 EvolutionSpent) const;

	//~ The price of a piece --------------------------------------------------------
	// A piece costs about what a candidate of the current wave drops: each boss killed
	// pays for roughly one new defense or one evolution, never both, and the price
	// climbs with the bosses, so a piece on wave 80 costs what the bosses of wave 80 pay.
	//
	//   price = candidate funds of the wave x ReplacementCostRatio x BaseCost / ReplacementReferenceCost
	//
	// BaseCost is the piece's own number (BuildCost of a defender's tower data, Cost of
	// anything else) and says only how it compares to the others: a piece whose base is
	// the reference costs one candidate, a divider a fraction of one. Placeholders, like
	// every number here, until there is a cast to calibrate against.

	/** Candidates a reference piece costs. 1 makes a boss pay for one of them. */
	UPROPERTY(config, EditAnywhere, Category = "Price", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ReplacementCostRatio = 1.0f;

	/** Base cost that is priced at ReplacementCostRatio candidates; every other base is priced in proportion. */
	UPROPERTY(config, EditAnywhere, Category = "Price", meta = (ClampMin = "1", UIMin = "1"))
	int32 ReplacementReferenceCost = 100;

	/** Public money a scheduled candidate on this wave drops, for a wave creep of this base health. */
	int32 GetCandidateFunds(float CreepHealth, int32 Wave) const;

	/** Public money a piece of this base cost costs on this wave. 0 for a free piece; at least 1 otherwise. */
	int32 GetReplacementCost(int32 BaseCost, float CreepHealth, int32 Wave) const;

	/**
	 * How much dearer everything is on a wave than on wave 1: the growth of the bosses'
	 * health, which is the growth of what they drop. Levels are priced with it, so a
	 * level on wave 80 weighs against a boss of wave 80 what one on wave 5 did.
	 */
	float GetPriceScale(int32 Wave) const;

	//~ Moving pieces between waves -------------------------------------------
	// Rate = Min(MoveTaxMax, MoveTaxInitial + MoveTaxStep * Wave), charged on the price
	// the piece was bought for, in public money. A percentage rather than a table: moving a cheap
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

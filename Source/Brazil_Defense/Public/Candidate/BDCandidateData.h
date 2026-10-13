// Brazil Defense. One candidate of the cast: his body, like any creep's, and his rallies.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/BDEnemyData.h"
#include "BDCandidateData.generated.h"

class USoundBase;

/** One kind of militant a rally raises, and how often against the others. */
USTRUCT(BlueprintType)
struct FBDRallyCrowdEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rally")
	TSoftObjectPtr<UBDEnemyData> Enemy;

	/** Its share of the crowd against the other entries. 0 leaves it out. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rally", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Weight = 1.0f;
};

/**
 * What a candidate's rallies are: how soon, how often, how long and how many rise around
 * him. Read off his data when he first walks out and kept on his record like his health,
 * so a candidate coming back, or loaded, rallies as he did.
 *
 * A rally stops him on the spot: a warning (the tell), then the speech, invulnerable and
 * out of the Mito's sight, then the crowd rising out of the ground around him, and on he
 * walks. UBDRallyComponent runs it.
 */
USTRUCT(BlueprintType)
struct FBDRallyParams
{
	GENERATED_BODY()

	/** Whether he rallies at all. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rally")
	bool bRallies = true;

	/** Seconds after he steps out of the bus before the first rally: never at the mouth. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rally", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float RallyFirstDelay = 6.0f;

	/** Seconds of walking between the end of a rally and the next one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rally", meta = (ClampMin = "1.0", UIMin = "1.0", ForceUnits = "s"))
	float RallyEvery = 20.0f;

	/** RallyEvery is drawn this fraction either way at every rally, so they never fall into a beat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rally", meta = (ClampMin = "0.0", ClampMax = "0.9", UIMin = "0.0", UIMax = "0.9"))
	float RallyEveryJitter = 0.2f;

	/** Seconds of warning before he stops: the red exclamation over him and a short sound, still walking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rally", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float RallyTellSeconds = 1.5f;

	/** Seconds the speech lasts with no cue to time it; with RallySpeech, the cue's own length. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rally", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float RallyDuration = 4.0f;

	/** The speech never lasts less than this, whatever the cue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rally", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float RallyMinDuration = 2.5f;

	/** Militants rising out of the ground around him at the end of every speech. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rally", meta = (ClampMin = "0", UIMin = "0"))
	int32 RallyCount = 0;

	/** Which militants rise, by weight. Empty: the creep of the wave. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rally")
	TArray<FBDRallyCrowdEntry> RallyComposition;

	/** What he says at the rally, apart from SpeechSound (walking) and DeathSound. Empty: his SpeechSound stands in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rally", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	TSoftObjectPtr<USoundBase> RallySpeech;

	/** Whether these were ever filled: a record from a save older than the rallies has them empty. */
	bool IsSet() const { return RallyCount > 0 || RallyComposition.Num() > 0; }

	/**
	 * The defaults of the Nth candidate of the cast (1..20, clamped): one every 20 s for the
	 * first down to one every 10 s for the twentieth, 2 x N militants a rally, all of them
	 * DA_Horda_01 until the other kinds have their art.
	 */
	static FBDRallyParams DefaultsFor(int32 Ordinal);

	/** The cast number the defaults stop growing at: the twentieth candidate. */
	static constexpr int32 MaxOrdinal = 20;
};

/**
 * The data of a candidate (DA_Candidato_NN): everything a creep has, which is his body, his
 * walk, his words and his cry, and his rallies on top.
 *
 * The body still comes from the one candidate of the wave settings (CandidateEnemy) until
 * each of the twenty has art of his own; the rallies are read off the asset of his own
 * number already.
 */
UCLASS(BlueprintType, meta = (DisplayName = "BD Candidate"))
class BRAZIL_DEFENSE_API UBDCandidateData : public UBDEnemyData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rally", meta = (ShowOnlyInnerProperties))
	FBDRallyParams Rally;

	/** The data of the Nth candidate of the cast, the numbers past the cast going round it again. Null when it is missing. */
	static const UBDCandidateData* LoadForOrdinal(int32 Ordinal);

	/** His rallies: the asset's, or the defaults of his number when the asset is missing or never filled. */
	static FBDRallyParams RallyForOrdinal(int32 Ordinal);
};

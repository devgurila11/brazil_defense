// Brazil Defense. Debug tooling: a whole match played without a player, to be measured.

#pragma once

#include "CoreMinimal.h"
#include "Match/BDMatchTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDSimSubsystem.generated.h"

class ABDMatchManager;

/**
 * Debug tooling, and only that: it drives a match to its end so the balance can be read
 * off numbers instead of impressions. Nothing here is a game rule and nothing here is
 * reachable from play.
 *
 * A run builds the AutoSetup defense from a seed, then, on every building phase, buys
 * what evolution the public money allows and calls the wave at once. The waves therefore
 * follow each other as fast as the creeps can walk, and the match plays itself to a
 * defeat, to the winning wave, or to the wave cap.
 *
 * Evolution is bought cheapest first, which is the closest thing to a player spreading
 * their money evenly rather than pouring it into one defender. It goes through the real
 * ABDTowerBase::Upgrade, so it pays real public money and obeys the block rule of the
 * platforms exactly as a player would.
 *
 * Every line it writes starts with SIM so a run can be grepped out of the log.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDSimSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject interface

	static UBDSimSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * Starts a run.
	 * @param Seed the AutoSetup seed, which is also the obstacle seed: one number brings the whole board back.
	 * @param Waves the wave to stop at, when nothing has ended the match before it.
	 * @param bInEvolve whether the run buys evolution as the money comes in, or leaves every defender at level 1.
	 * @param Speed game speed the match is run at. The clock is dilated, so the numbers are the same at any of them.
	 * @param bQuitWhenDone whether the process exits once the run is reported.
	 */
	void Start(int32 Seed, int32 Waves, bool bInEvolve, float Speed, bool bQuitWhenDone);

	/**
	 * Spends the public money on evolution, cheapest upgrade first, until nothing else
	 * can be afforded. Goes through the paid path, so it obeys the block rule and the
	 * level cap. @return how many levels were bought.
	 */
	int32 AutoEvolve(int32& OutSpent);

	bool IsRunning() const { return bRunning; }

private:
	ABDMatchManager* GetMatch() const;

	void HandleWaveStarted(int32 Wave);

	/** One SIM line: where the match stands right now. */
	void LogSample(const TCHAR* Tag, int32 Wave) const;

	/** The last SIM line of a run: how it ended. */
	void Report(const TCHAR* Outcome);

	void Stop();

	/** The defenders standing, and the average level among them. */
	void GetDefense(int32& OutCount, float& OutAverageLevel) const;

	bool bRunning = false;
	bool bEvolve = true;
	int32 Seed = 0;
	int32 TargetWaves = 100;
	bool bQuit = false;

	/** Real seconds the run has taken, so a match that stalls is reported rather than hung on. */
	float RealSecondsElapsed = 0.0f;

	/** The wave the first level was ever bought on: what "when can the player evolve at all" means. */
	int32 FirstEvolutionWave = 0;

	/** Levels bought and public money spent over the whole run. */
	int32 LevelsBought = 0;
	int32 MoneySpent = 0;

	/** The wave last sampled, so a wave is not reported twice. */
	int32 LastSampledWave = -1;

	TWeakObjectPtr<ABDMatchManager> BoundMatch;
	FDelegateHandle WaveStartedHandle;
};

// Brazil Defense. The bribe: what a dead candidate drops, and its way to the mint.

#pragma once

#include "CoreMinimal.h"
#include "Match/BDMatchTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDBribeSubsystem.generated.h"

class ABDMatchManager;
class ABDMoneyBag;
class USoundBase;
class USoundConcurrency;

/** Where a conversion stands. One runs at a time; the rest wait in the queue. */
UENUM()
enum class EBDBribeStage : uint8
{
	/** Nothing to convert. */
	Idle,
	/** The bag is falling and ringing over the body. */
	Bag,
	/** The thief's counter is climbing. */
	Counting,
	/** The till has rung; a beat before the money moves. */
	Register,
	/** The value is coming off the thief's counter and landing on the mint's. */
	Transfer
};

/**
 * The bribe a scheduled candidate stole, on its way to becoming money the player may
 * spend. One conversion at a time, in the order the candidates fell:
 *
 *   the bag drops on the body and bounces, ringing coins at every touch; it fades; the
 *   thief's counter climbs the whole bribe; the till rings; the value comes off the
 *   thief's counter and lands on the mint's, which is what evolution is paid from.
 *
 * The ledger is ABDMatchManager's: this only decides when the numbers move, so what the
 * player sees climbing is the balance itself, never a second copy of it that could
 * disagree. A conversion that is cut short by the end of the match is dropped with the
 * money, because none of it carries to the next one.
 *
 * Only a scheduled candidate pays. The fallen who come back with an inverted count drop
 * nothing, the same rule that keeps them from raising the budget: a count thrown on
 * purpose must not be worth throwing.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDBribeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject interface

	/** Convenience accessor. Returns null when the world context has no world. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Bribe", meta = (WorldContext = "WorldContextObject"))
	static UBDBribeSubsystem* Get(const UObject* WorldContextObject);

	/**
	 * A scheduled candidate is down: queue his bribe and the show that converts it.
	 * @param Amount already worked out from his health; nothing is recomputed here.
	 * @param Where the body, which is where the bag falls and the coins are heard.
	 * @param Ordinal which candidate of the match he was, for the log.
	 */
	void Collect(int32 Amount, const FVector& Where, int32 Ordinal);

	/** Finishes the running conversion and everything queued at once, with no show. Debug, and the end of a wave nobody watched. */
	void FlushNow(const TCHAR* Why);

	/** Whether a conversion is running or waiting. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Bribe")
	bool IsConverting() const { return Stage != EBDBribeStage::Idle || Queue.Num() > 0; }

	/** How far the running conversion has come, 0..1. 0 when nothing runs. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Bribe")
	float GetConversionProgress() const;

	/**
	 * Plays one of the bribe's sounds in the world, 3D at a place on the board, through
	 * the effects class and attenuated over the board like the urn's beep. Public because
	 * the bag rings its own coins.
	 */
	void PlaySoundAt(USoundBase* Sound, const FVector& Where, float Volume, float Pitch);

private:
	/** One candidate's bribe, waiting its turn. */
	struct FBDPendingBribe
	{
		int32 Amount = 0;
		FVector Where = FVector::ZeroVector;
		int32 Ordinal = 0;
	};

	ABDMatchManager* GetMatch() const;

	/** Listens to the match once there is one; it is spawned after this subsystem. */
	void EnsureMatchBinding();
	void HandlePhaseChanged(EBDMatchPhase NewPhase);

	/** Takes the next bribe off the queue and drops its bag. */
	void BeginNext();

	/** Moves the running conversion on by the fraction of its stage that has elapsed. */
	void AdvanceCounting(float Alpha);
	void AdvanceTransfer(float Alpha);

	/** Everything running and queued thrown away: a rewind, or a match that ended. */
	void ResetForNewMatch(const TCHAR* Why);

	TArray<FBDPendingBribe> Queue;

	EBDBribeStage Stage = EBDBribeStage::Idle;

	/** The conversion running now. */
	FBDPendingBribe Current;

	/** Seconds spent in the current stage. */
	float StageElapsed = 0.0f;

	/** How much of Current.Amount has already been put on the thief's counter, and taken off it. */
	int32 Counted = 0;
	int32 Transferred = 0;

	TWeakObjectPtr<ABDMoneyBag> Bag;

	/** Built once: a few coin drops may sound together, the oldest gives way. */
	UPROPERTY(Transient)
	TObjectPtr<USoundConcurrency> Concurrency;

	TWeakObjectPtr<ABDMatchManager> BoundMatch;
	FDelegateHandle PhaseChangedHandle;
};

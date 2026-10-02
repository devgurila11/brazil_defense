// Brazil Defense. The piece the player clicked: the one whose reach is drawn.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDInspection.generated.h"

/**
 * The reach of a defender is not drawn all the time: it would cover the board. A click on
 * a piece selects it, and the piece selected is the one whose reach shows - a tower its
 * range, a platform the range of everyone standing on it, the palace or the Agent the
 * Agent's radius. A click on another piece moves it there; a click on empty ground, or
 * Escape, hides it. The placement component sets it; whoever draws a reach asks it.
 *
 * The console variables of each debug (BD.Tower.ShowRange, BD.Agent.ShowRange) say 1 for
 * this, 2 to draw every one regardless, 0 for none.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDInspectionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of the world an object lives in, or null. */
	static UBDInspectionSubsystem* Get(const UObject* WorldContextObject);

	/** Null hides every reach. */
	void SetInspected(AActor* Actor);

	AActor* GetInspected() const { return Inspected.Get(); }

	/** Whether this actor, or one of these owners of it, is the piece selected. */
	bool IsInspected(const AActor* Actor, const AActor* Owner = nullptr) const;

	/**
	 * Whether a reach should be drawn under a show-range console variable: 0 never, 2
	 * always, anything else only for the piece selected.
	 */
	static bool ShouldDrawReach(int32 ShowRangeMode, const AActor* Actor, const AActor* Owner = nullptr);

private:
	TWeakObjectPtr<AActor> Inspected;
};

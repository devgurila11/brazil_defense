// Brazil Defense. A foot of a creep lands: the notify placed on its walk cycle.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "BDAnimNotify_Footstep.generated.h"

/**
 * Placed on a creep's MoveAnimation at every frame a foot touches the ground. It carries
 * no sound of its own: the creep's data says what a step sounds like, so one walk cycle
 * can be shared by creeps that sound different, and UBDCreepSoundSubsystem plays it under
 * the step budget.
 *
 * Tied to the animation rather than to a timer on purpose: the rate of the loop follows
 * the creep's speed, and so do the steps, without anyone computing a cadence.
 */
UCLASS(const, hidecategories = Object, collapsecategories, meta = (DisplayName = "BD Footstep"))
class BRAZIL_DEFENSE_API UBDAnimNotify_Footstep : public UAnimNotify
{
	GENERATED_BODY()

public:
	UBDAnimNotify_Footstep();

	//~ Begin UAnimNotify interface
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
	//~ End UAnimNotify interface
};

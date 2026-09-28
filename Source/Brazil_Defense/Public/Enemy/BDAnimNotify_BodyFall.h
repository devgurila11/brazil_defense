// Brazil Defense. A killed creep's body hits the ground: the notify placed on its falls.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "BDAnimNotify_BodyFall.generated.h"

/**
 * Placed on each of a creep's DeathAnimations at the frame the body lands. Like the step,
 * it carries no sound of its own: the creep's data says what the thud sounds like, and
 * UBDCreepSoundSubsystem plays it under the fall budget. Only a body left by a kill
 * (ABDCreepCorpse) answers it.
 */
UCLASS(const, hidecategories = Object, collapsecategories, meta = (DisplayName = "BD Body Fall"))
class BRAZIL_DEFENSE_API UBDAnimNotify_BodyFall : public UAnimNotify
{
	GENERATED_BODY()

public:
	UBDAnimNotify_BodyFall();

	//~ Begin UAnimNotify interface
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
	//~ End UAnimNotify interface
};

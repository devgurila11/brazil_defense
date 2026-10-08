// Brazil Defense. The gun goes off: the notify placed on a shooting animation.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "BDAnimNotify_Shot.generated.h"

/**
 * Placed on the frame of a shooting animation where the gun kicks. The shot leaves then,
 * not on a timer of its own: a timer and a loop drift apart after a couple of shots, and
 * the arm would kick with nothing coming out. The shooter plays the loop at the rate its
 * weapon fires, so one gesture is one shot.
 *
 * The Agent (ABDAgent) and the platform shooters (ABDShooter) listen to it. The Agent's
 * Shooting carries it at 0.21 s, where the right wrist snaps back.
 */
UCLASS(const, hidecategories = Object, collapsecategories, meta = (DisplayName = "BD Shot"))
class BRAZIL_DEFENSE_API UBDAnimNotify_Shot : public UAnimNotify
{
	GENERATED_BODY()

public:
	//~ Begin UAnimNotify interface
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
	//~ End UAnimNotify interface

	/** Whether an animation carries this notify anywhere. */
	static bool IsOn(const UAnimSequenceBase* Animation);
};

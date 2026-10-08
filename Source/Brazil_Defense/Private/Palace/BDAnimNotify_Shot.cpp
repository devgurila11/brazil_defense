// Brazil Defense. The gun goes off: the notify placed on a shooting animation.

#include "Palace/BDAnimNotify_Shot.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Palace/BDAgent.h"
#include "Tower/BDShooter.h"

void UBDAnimNotify_Shot::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* Owner = MeshComp != nullptr ? MeshComp->GetOwner() : nullptr;
	if (ABDAgent* Agent = Cast<ABDAgent>(Owner))
	{
		Agent->OnShotFrame();
	}
	else if (ABDShooter* Shooter = Cast<ABDShooter>(Owner))
	{
		Shooter->OnShotFrame();
	}
}

FString UBDAnimNotify_Shot::GetNotifyName_Implementation() const
{
	return TEXT("BD Shot");
}

bool UBDAnimNotify_Shot::IsOn(const UAnimSequenceBase* Animation)
{
	if (Animation == nullptr)
	{
		return false;
	}
	for (const FAnimNotifyEvent& Event : Animation->Notifies)
	{
		if (Cast<UBDAnimNotify_Shot>(Event.Notify) != nullptr)
		{
			return true;
		}
	}
	return false;
}

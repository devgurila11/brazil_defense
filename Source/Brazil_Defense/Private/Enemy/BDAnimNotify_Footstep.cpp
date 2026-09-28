// Brazil Defense. A foot of a creep lands: the notify placed on its walk cycle.

#include "Enemy/BDAnimNotify_Footstep.h"

#include "Components/SkeletalMeshComponent.h"
#include "Enemy/BDCreepSoundSubsystem.h"
#include "Enemy/BDEnemyBase.h"

UBDAnimNotify_Footstep::UBDAnimNotify_Footstep()
{
#if WITH_EDITORONLY_DATA
	// The preview in the animation editor has no creep and so no step to play.
	bShouldFireInEditor = false;
	NotifyColor = FColor(190, 140, 60, 255);
#endif
}

void UBDAnimNotify_Footstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	const ABDEnemyBase* Creep = MeshComp != nullptr ? Cast<ABDEnemyBase>(MeshComp->GetOwner()) : nullptr;
	if (Creep == nullptr)
	{
		return;
	}
	if (UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(Creep))
	{
		Sounds->PlayFootstep(*Creep);
	}
}

FString UBDAnimNotify_Footstep::GetNotifyName_Implementation() const
{
	return TEXT("BD Footstep");
}

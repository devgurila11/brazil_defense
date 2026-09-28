// Brazil Defense. A killed creep's body hits the ground: the notify placed on its falls.

#include "Enemy/BDAnimNotify_BodyFall.h"

#include "Components/SkeletalMeshComponent.h"
#include "Enemy/BDCreepCorpse.h"
#include "Enemy/BDCreepSoundSubsystem.h"

UBDAnimNotify_BodyFall::UBDAnimNotify_BodyFall()
{
#if WITH_EDITORONLY_DATA
	// The preview in the animation editor has no body left by a kill and so nothing to play.
	bShouldFireInEditor = false;
	NotifyColor = FColor(150, 90, 60, 255);
#endif
}

void UBDAnimNotify_BodyFall::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	const ABDCreepCorpse* Corpse = MeshComp != nullptr ? Cast<ABDCreepCorpse>(MeshComp->GetOwner()) : nullptr;
	if (Corpse == nullptr)
	{
		return;
	}
	if (UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(Corpse))
	{
		Sounds->PlayBodyFall(*Corpse);
	}
}

FString UBDAnimNotify_BodyFall::GetNotifyName_Implementation() const
{
	return TEXT("BD Body Fall");
}

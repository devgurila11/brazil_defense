// Brazil Defense. What the Palácio do Governo is: its look and how far it has evolved.

#include "Palace/BDPalaceData.h"

#include "Animation/AnimSequenceBase.h"
#include "Engine/SkeletalMesh.h"

UBDPalaceData::UBDPalaceData()
{
	AgentMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/BD/Meshs/Agent/Mesh/SMK_Mito.SMK_Mito")));
	WalkAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/BD/Meshs/Agent/Animations/Pistol_Walk.Pistol_Walk")));
	IdleAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/BD/Meshs/Agent/Animations/Pistol_Idle.Pistol_Idle")));
	ShootAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/BD/Meshs/Agent/Animations/Shooting.Shooting")));
	SleepAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/BD/Meshs/Agent/Animations/Sleeping_Idle.Sleeping_Idle")));
	KickAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/BD/Meshs/Agent/Animations/Kicking.Kicking")));

	// The pistol, then one entry per star still to be designed: the pistol's numbers, and
	// a kill bonus 0.15 s smaller each level, from 1 s down to 0.25 s at the fifth star.
	Weapons.SetNum(MaxLevel + 1);
	for (int32 Index = 0; Index < Weapons.Num(); ++Index)
	{
		FBDAgentWeapon& Weapon = Weapons[Index];
		Weapon.Name = Index == 0 ? FName(TEXT("Pistol")) : FName(*FString::Printf(TEXT("Weapon%d"), Index));
		Weapon.bPlaceholder = Index > 0;
		Weapon.Damage = 10.0f;
		Weapon.FireRate = 1.5f;
		Weapon.KillBonusSeconds = 1.0f - 0.15f * Index;
	}
}

const FBDAgentWeapon* UBDPalaceData::GetWeapon(const int32 PalaceLevel) const
{
	return Weapons.Num() > 0 ? &Weapons[FMath::Clamp(PalaceLevel, 0, Weapons.Num() - 1)] : nullptr;
}

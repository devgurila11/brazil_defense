// Brazil Defense. A platform shooter: the man, his animations, and the weapon of each level.

#include "Tower/BDShooterData.h"

#include "Animation/AnimSequenceBase.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"

namespace BDShooterDataPrivate
{
	/** Walks down from a level's entry to the first that has the slot filled. */
	template <typename TSlot, typename TGetter>
	static TSlot ResolveDown(const TArray<FBDShooterWeapon>& Weapons, const int32 Level, TGetter Get)
	{
		for (int32 Index = FMath::Clamp(Level - 1, 0, Weapons.Num() - 1); Index >= 0; --Index)
		{
			const TSlot& Slot = Get(Weapons[Index]);
			if (!Slot.IsNull())
			{
				return Slot;
			}
		}
		return TSlot();
	}
}

UBDShooterData::UBDShooterData()
{
	// A character, never a tower: he stands on a platform slot.
	bCanPlaceOnGround = false;
	bCanPlaceOnSlot = true;

	// The pistol he is built with, then a placeholder per star.
	static const TCHAR* const Names[MaxLevels] = { TEXT("Pistola"), TEXT("Arma2"), TEXT("Arma3"), TEXT("Arma4"), TEXT("Arma5"), TEXT("Arma6") };
	Weapons.SetNum(MaxLevels);
	for (int32 Index = 0; Index < MaxLevels; ++Index)
	{
		Weapons[Index].Name = Names[Index];
		Weapons[Index].bPlaceholder = Index > 0;
	}
}

const FBDShooterWeapon* UBDShooterData::GetWeapon(const int32 Level) const
{
	return Weapons.Num() > 0 ? &Weapons[FMath::Clamp(Level - 1, 0, Weapons.Num() - 1)] : nullptr;
}

TSoftObjectPtr<UStaticMesh> UBDShooterData::ResolveWeaponMesh(const int32 Level) const
{
	return BDShooterDataPrivate::ResolveDown<TSoftObjectPtr<UStaticMesh>>(Weapons, Level,
		[](const FBDShooterWeapon& Weapon) -> const TSoftObjectPtr<UStaticMesh>& { return Weapon.Mesh; });
}

TSoftObjectPtr<UNiagaraSystem> UBDShooterData::ResolveMuzzleFlash(const int32 Level) const
{
	return BDShooterDataPrivate::ResolveDown<TSoftObjectPtr<UNiagaraSystem>>(Weapons, Level,
		[](const FBDShooterWeapon& Weapon) -> const TSoftObjectPtr<UNiagaraSystem>& { return Weapon.MuzzleFlash; });
}

TSoftObjectPtr<USoundBase> UBDShooterData::ResolveFireSound(const int32 Level) const
{
	return BDShooterDataPrivate::ResolveDown<TSoftObjectPtr<USoundBase>>(Weapons, Level,
		[](const FBDShooterWeapon& Weapon) -> const TSoftObjectPtr<USoundBase>& { return Weapon.FireSound; });
}

// Brazil Defense. Definition of one kind of enemy.

#include "Enemy/BDEnemyData.h"

#include "BDLog.h"
#include "Materials/MaterialInterface.h"

const FPrimaryAssetType UBDEnemyData::EnemyAssetType = TEXT("BDEnemy");

FPrimaryAssetId UBDEnemyData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(EnemyAssetType, GetFName());
}

void UBDEnemyData::PreloadSkins() const
{
	if (bSkinsLoaded)
	{
		return;
	}
	bSkinsLoaded = true;

	LoadedSkins.Reset();
	auto Load = [this](const TSoftObjectPtr<UMaterialInterface>& Skin)
	{
		if (Skin.IsNull())
		{
			return;
		}
		if (UMaterialInterface* Material = Skin.LoadSynchronous())
		{
			LoadedSkins.AddUnique(Material);
		}
		else
		{
			UE_LOG(LogBDWave, Error, TEXT("%s: skin %s failed to load."), *GetName(), *Skin.ToString());
		}
	};

	Load(MeshMaterial);
	for (const TSoftObjectPtr<UMaterialInterface>& Skin : SkinMaterials)
	{
		Load(Skin);
	}

	FString Names;
	for (const UMaterialInterface* Skin : LoadedSkins)
	{
		Names += Names.IsEmpty() ? Skin->GetName() : TEXT(", ") + Skin->GetName();
	}
	UE_LOG(LogBDWave, Log, TEXT("%s: %d skin(s) in the pool: %s."), *GetName(), LoadedSkins.Num(), Names.IsEmpty() ? TEXT("none") : *Names);
}

UMaterialInterface* UBDEnemyData::PickSkin() const
{
	PreloadSkins();
	return LoadedSkins.Num() > 0 ? LoadedSkins[FMath::RandHelper(LoadedSkins.Num())].Get() : nullptr;
}

int32 UBDEnemyData::GetSkinCount() const
{
	int32 Count = MeshMaterial.IsNull() ? 0 : 1;
	for (const TSoftObjectPtr<UMaterialInterface>& Skin : SkinMaterials)
	{
		Count += Skin.IsNull() ? 0 : 1;
	}
	return Count;
}

bool UBDEnemyData::IsSkin(const UMaterialInterface* Material) const
{
	PreloadSkins();
	return Material != nullptr && LoadedSkins.Contains(Material);
}

#if WITH_EDITOR
void UBDEnemyData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	bSkinsLoaded = false;
	LoadedSkins.Reset();
}
#endif

// Brazil Defense. Definition of one kind of defender and its levels.

#include "Tower/BDTowerData.h"

#include "BDLog.h"

const FPrimaryAssetType UBDTowerData::TowerAssetType = TEXT("BDTower");

FPrimaryAssetId UBDTowerData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TowerAssetType, GetFName());
}

const FBDTowerLevel* UBDTowerData::GetLevel(const int32 Level) const
{
	if (Levels.Num() == 0)
	{
		return nullptr;
	}

	return &Levels[FMath::Clamp(Level - 1, 0, Levels.Num() - 1)];
}

#if WITH_EDITOR
void UBDTowerData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (Levels.Num() > MaxLevels)
	{
		UE_LOG(LogBDTower, Warning, TEXT("%s: a tower has at most %d levels; the extra ones were dropped."),
			*GetName(), MaxLevels);
		Levels.SetNum(MaxLevels);
	}
}
#endif

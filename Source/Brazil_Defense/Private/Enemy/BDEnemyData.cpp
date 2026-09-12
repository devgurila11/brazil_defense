// Brazil Defense. Definition of one kind of enemy.

#include "Enemy/BDEnemyData.h"

const FPrimaryAssetType UBDEnemyData::EnemyAssetType = TEXT("BDEnemy");

FPrimaryAssetId UBDEnemyData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(EnemyAssetType, GetFName());
}

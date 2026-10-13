// Brazil Defense. One minister of the STF: his body, his animations and his voice.

#include "Palace/BDMinisterData.h"

const FPrimaryAssetType UBDMinisterData::MinisterAssetType = TEXT("BDMinister");

FPrimaryAssetId UBDMinisterData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(MinisterAssetType, GetFName());
}

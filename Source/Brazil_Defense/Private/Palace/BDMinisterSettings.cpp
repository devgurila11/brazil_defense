// Brazil Defense. What a minister is, until his model and his voice arrive.

#include "Palace/BDMinisterSettings.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"

UBDMinisterSettings::UBDMinisterSettings()
{
	CategoryName = TEXT("Game");
	StfMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/BD/Meshs/Building/STF/SM_STF.SM_STF")));
	BlockMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	BlockMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
	ShieldMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	ShieldMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BD/Materials/M_MinisterShield.M_MinisterShield")));
}

const UBDMinisterSettings& UBDMinisterSettings::Get()
{
	return *GetDefault<UBDMinisterSettings>();
}

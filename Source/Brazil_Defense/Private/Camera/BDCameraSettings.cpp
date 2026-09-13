// Brazil Defense. Where the match is looked at from.

#include "Camera/BDCameraSettings.h"

UBDCameraSettings::UBDCameraSettings()
{
	CategoryName = TEXT("Game");
}

const UBDCameraSettings& UBDCameraSettings::Get()
{
	const UBDCameraSettings* Settings = GetDefault<UBDCameraSettings>();
	check(Settings);
	return *Settings;
}

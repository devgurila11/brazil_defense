// Brazil Defense. Configuration of the interface layer: maps, timings and the audio it drives.

#include "UI/BDUISettings.h"

UBDUISettings::UBDUISettings()
{
	CategoryName = TEXT("Game");
}

const UBDUISettings& UBDUISettings::Get()
{
	const UBDUISettings* Settings = GetDefault<UBDUISettings>();
	check(Settings);
	return *Settings;
}

FLinearColor UBDUISettings::GetSpeechMarkColor(const EBDSpeakerSide Side) const
{
	switch (Side)
	{
	case EBDSpeakerSide::Player: return SpeechMarkPlayerColor;
	case EBDSpeakerSide::Minister: return SpeechMarkMinisterColor;
	default: return SpeechMarkOpponentColor;
	}
}

float UBDUISettings::GetPalaceStarOpacity(const float CameraDistance) const
{
	// Eased both ends, so the stars do not pop on or off at either edge of the band.
	const float End = FMath::Max(PalaceStarFadeEnd, PalaceStarFadeStart + 1.0f);
	return 1.0f - FMath::SmoothStep(PalaceStarFadeStart, End, CameraDistance);
}

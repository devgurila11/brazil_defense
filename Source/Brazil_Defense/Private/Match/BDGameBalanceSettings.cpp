// Brazil Defense. Pacing of a match: countdowns, refunds and game speed.

#include "Match/BDGameBalanceSettings.h"

#include "Match/BDDifficultyData.h"

UBDGameBalanceSettings::UBDGameBalanceSettings()
{
	// Shows up under Project Settings > Game, next to the grid settings.
	CategoryName = TEXT("Game");
}

const UBDGameBalanceSettings& UBDGameBalanceSettings::Get()
{
	const UBDGameBalanceSettings* Settings = GetDefault<UBDGameBalanceSettings>();
	check(Settings);
	return *Settings;
}

bool UBDGameBalanceSettings::IsGameSpeedAllowed(const float Speed) const
{
	for (const float Allowed : AllowedGameSpeeds)
	{
		if (FMath::IsNearlyEqual(Allowed, Speed))
		{
			return true;
		}
	}

	return false;
}

float UBDGameBalanceSettings::GetMoveTaxRate(const int32 Wave) const
{
	return FMath::Min(MoveTaxMax, MoveTaxInitial + MoveTaxStep * FMath::Max(0, Wave));
}

float UBDGameBalanceSettings::GetWaveDelay(const int32 Wave) const
{
	const float Decayed = BaseWaveDelay * FMath::Pow(WaveDelayDecayRate, static_cast<float>(FMath::Max(0, Wave)));
	return FMath::Max(MinWaveDelay, Decayed);
}

const UBDDifficultyData* UBDGameBalanceSettings::FindDifficultyData(const EBDDifficulty Difficulty) const
{
	const TSoftObjectPtr<UBDDifficultyData>* Found = DifficultyAssets.Find(Difficulty);
	if (Found == nullptr || Found->IsNull())
	{
		return nullptr;
	}

	// Synchronous: this is asked once at the start of a match, on a path that is already
	// waiting for the board to be built.
	return Found->LoadSynchronous();
}

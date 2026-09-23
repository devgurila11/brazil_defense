// Brazil Defense. Pacing of a match: countdowns, refunds and game speed.

#include "Match/BDGameBalanceSettings.h"

#include "Curves/CurveFloat.h"
#include "Match/BDDifficultyData.h"
#include "Tower/BDTowerData.h"

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

float UBDGameBalanceSettings::GetHealthScale(const int32 Wave) const
{
	const int32 SafeWave = FMath::Max(1, Wave);
	if (const UCurveFloat* Curve = HealthScaleByWave.LoadSynchronous())
	{
		return FMath::Max(0.0f, Curve->GetFloatValue(static_cast<float>(SafeWave)));
	}

	return FMath::Pow(FMath::Max(1.0f, HealthScaleGrowth), static_cast<float>(SafeWave - 1));
}

float UBDGameBalanceSettings::GetSellRefundRatio(const EBDPieceKind Kind, const int32 Wave) const
{
	if (Wave < 1 || (Kind == EBDPieceKind::Divider && Wave <= DividerRemovalGraceWave))
	{
		return BuildingPhaseRefundRatio;
	}

	return SellRefundRatio;
}

float UBDGameBalanceSettings::GetWaveSpawnInterval(const int32 Wave) const
{
	return FMath::Max(WaveSpawnIntervalMin, WaveSpawnIntervalBase * FMath::Pow(WaveSpawnIntervalDecay, static_cast<float>(FMath::Max(0, Wave))));
}

int32 UBDGameBalanceSettings::VotesForHealth(const float Health) const
{
	return FMath::Max(1, FMath::FloorToInt(FMath::Max(0.0f, Health) / FMath::Max(0.01f, HealthPerVote)));
}

int32 UBDGameBalanceSettings::RedVotesForHealth(const float Health) const
{
	return FMath::Max(1, FMath::FloorToInt(VotesForHealth(Health) * FMath::Max(0.1f, RedVoteWeight)));
}

int32 UBDGameBalanceSettings::GetCreepsPerSpawnPoint(const int32 Wave) const
{
	return FMath::Max(1, CreepsPerSpawnPointBase) + FMath::Max(0, CreepsPerSpawnPointStep) * FMath::Max(0, Wave - 1);
}

int32 UBDGameBalanceSettings::GetUpgradeCost(const int32 UpgradeCostBase, const int32 Level) const
{
	return FMath::RoundToInt(FMath::Max(0, UpgradeCostBase) * FMath::Pow(FMath::Max(1.0f, UpgradeCostGrowth), static_cast<float>(FMath::Max(1, Level) - 1)));
}

int32 UBDGameBalanceSettings::GetUpgradeCostOnWave(const int32 UpgradeCostBase, const int32 Level, const int32 Wave) const
{
	const int32 AtWaveOne = GetUpgradeCost(UpgradeCostBase, Level);
	return AtWaveOne <= 0 ? 0 : FMath::Max(1, FMath::RoundToInt(AtWaveOne * GetPriceScale(Wave)));
}

float UBDGameBalanceSettings::GetPriceScale(const int32 Wave) const
{
	return GetHealthScale(FMath::Max(1, Wave)) / FMath::Max(KINDA_SMALL_NUMBER, GetHealthScale(1));
}

float UBDGameBalanceSettings::GetUpgradeDamageScale(const int32 Level) const
{
	return 1.0f + FMath::Max(0.0f, DamageGrowthPerLevel) * (FMath::Max(1, Level) - 1);
}

int32 UBDGameBalanceSettings::GetEvolutionSpent(const int32 UpgradeCostBase, const int32 Level) const
{
	// Summed rather than closed form: the rounding of each level is what was actually
	// charged, and a refund that does not match what was paid is a bug the player sees.
	int32 Spent = 0;
	for (int32 Step = 2; Step <= FMath::Min(Level, UBDTowerData::MaxLevels); ++Step)
	{
		Spent += GetUpgradeCost(UpgradeCostBase, Step);
	}
	return Spent;
}

int32 UBDGameBalanceSettings::BribeForHealth(const float Health) const
{
	return FMath::Max(1, FMath::FloorToInt(FMath::Max(0.0f, Health) / FMath::Max(0.01f, HealthPerBribe)));
}

float UBDGameBalanceSettings::GetCandidateHealth(const float CreepHealth, const int32 Wave) const
{
	return FMath::Max(1.0f, FMath::Max(0.0f, CreepHealth) * GetHealthScale(Wave) * FMath::Max(1.0f, CandidateHealthMultiplier));
}

int32 UBDGameBalanceSettings::GetCandidateFunds(const float CreepHealth, const int32 Wave) const
{
	return BribeForHealth(GetCandidateHealth(CreepHealth, Wave));
}

int32 UBDGameBalanceSettings::GetReplacementCost(const int32 BaseCost, const float CreepHealth, const int32 Wave) const
{
	if (BaseCost <= 0)
	{
		return 0;
	}

	const double Candidate = GetCandidateFunds(CreepHealth, Wave);
	const double Share = static_cast<double>(BaseCost) / FMath::Max(1, ReplacementReferenceCost);
	return FMath::Max(1, FMath::RoundToInt(Candidate * FMath::Max(0.0f, ReplacementCostRatio) * Share));
}

int32 UBDGameBalanceSettings::GetEvolutionRefund(const int32 EvolutionSpent) const
{
	return FMath::FloorToInt(FMath::Max(0, EvolutionSpent) * FMath::Clamp(EvolutionRefundRatio, 0.0f, 1.0f));
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

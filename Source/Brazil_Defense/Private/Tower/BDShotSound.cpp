// Brazil Defense. The sound of a shot: one path for every defender that fires.

#include "Tower/BDShotSound.h"

#include "Components/AudioComponent.h"
#include "BDLog.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "UI/BDUISettings.h"

UBDShotSoundSettings::UBDShotSoundSettings()
{
	CategoryName = TEXT("Game");
}

const UBDShotSoundSettings& UBDShotSoundSettings::Get()
{
	return *GetDefault<UBDShotSoundSettings>();
}

bool UBDShotSoundSubsystem::PlayShot(USoundBase* Sound, const FVector& Where)
{
	++ShotsRequested;
	UWorld* World = GetWorld();
	if (Sound == nullptr || World == nullptr)
	{
		// The slot is empty until the weapon has a sound: the shot still happens, quietly.
		++ShotsSilent;
		return false;
	}

	const UBDShotSoundSettings& Settings = UBDShotSoundSettings::Get();
	if (Concurrency == nullptr)
	{
		// Farthest first: the defender the camera is on is heard over one across the board.
		Concurrency = NewObject<USoundConcurrency>(this, TEXT("ShotConcurrency"));
		Concurrency->Concurrency.MaxCount = FMath::Clamp(Settings.ShotMaxConcurrent, 1, 32);
		Concurrency->Concurrency.bLimitToOwner = false;
		Concurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopFarthestThenOldest;
	}

	// 3D at the shooter, attenuated against the match camera, the same curve as the creeps.
	FSoundAttenuationSettings Attenuation;
	Attenuation.bAttenuate = true;
	Attenuation.bSpatialize = true;
	Attenuation.StereoSpread = 400.0f;
	Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Attenuation.dBAttenuationAtMax = Settings.ShotAttenuationAtMax;
	Attenuation.AttenuationShapeExtents = FVector(Settings.ShotInnerRadius, 0.0f, 0.0f);
	Attenuation.FalloffDistance = Settings.ShotFalloffDistance;
	Attenuation.FalloffMode = ENaturalSoundFalloffMode::Silent;
	Attenuation.bEnableListenerFocus = false;
	Attenuation.bAttenuateWithLPF = true;
	Attenuation.bEnableLogFrequencyScaling = true;
	Attenuation.LPFRadiusMin = Settings.ShotInnerRadius;
	Attenuation.LPFRadiusMax = Settings.ShotInnerRadius + Settings.ShotFalloffDistance;
	Attenuation.LPFFrequencyAtMax = 3000.0f;
	Attenuation.bEnableOcclusion = false;

	UAudioComponent* Audio = UGameplayStatics::SpawnSoundAtLocation(World, Sound, Where, FRotator::ZeroRotator,
		Settings.ShotVolume, 1.0f, 0.0f, nullptr, Concurrency, /*bAutoDestroy*/ true);
	if (Audio == nullptr)
	{
		return false;
	}

	// Through the effects class, so the options slider and mute apply to it.
	if (USoundClass* Effects = UBDUISettings::Get().EffectsSoundClass.LoadSynchronous())
	{
		Audio->SoundClassOverride = Effects;
	}
	Audio->bOverrideAttenuation = true;
	Audio->AttenuationOverrides = Attenuation;
	Audio->Play();
	++ShotsPlayed;
	return true;
}

namespace BDShotSoundCommands
{
	static void ExecStatus(const TArray<FString>& Args, UWorld* World)
	{
		const UBDShotSoundSubsystem* Shots = World != nullptr ? World->GetSubsystem<UBDShotSoundSubsystem>() : nullptr;
		if (Shots == nullptr)
		{
			UE_LOG(LogBDDebug, Warning, TEXT("BD.Shot.Status: no shot sound in this world."));
			return;
		}
		UE_LOG(LogBDDebug, Log, TEXT("BD.Shot.Status: %d shot(s) asked for a sound, %d played, %d silent (empty slot)."),
			Shots->GetShotsRequested(), Shots->GetShotsPlayed(), Shots->GetShotsSilent());
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdStatus(
		TEXT("BD.Shot.Status"),
		TEXT("BD.Shot.Status: how many shots asked for a sound, how many played it and how many had none."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));
}

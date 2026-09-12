// Brazil Defense. The sky, driven by how far the match has got.

#include "Day/BDDayCycleComponent.h"

#include "BDLog.h"
#include "Components/DirectionalLightComponent.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Day/BDDaySettings.h"
#include "Day/BDStreetLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

UBDDayCycleComponent::UBDDayCycleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBDDayCycleComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveDefaultCurves();
	ResolveSunLight();

	// Street lights that were already in the level when this started would otherwise have
	// nothing to register with, since their BeginPlay may well have run first. Adopting
	// them here means the order stops mattering.
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			TArray<UBDStreetLightComponent*> Lights;
			It->GetComponents(Lights);
			for (UBDStreetLightComponent* Light : Lights)
			{
				RegisterStreetLight(Light);
			}
		}
	}

	ApplyCycle();
}

void UBDDayCycleComponent::ResolveDefaultCurves()
{
	// Only what was left unset falls back, so a cycle configured by hand always wins.
	const UBDDaySettings& Settings = UBDDaySettings::Get();

	if (SunPitchByAlpha == nullptr)
	{
		SunPitchByAlpha = Settings.SunPitchByAlpha.LoadSynchronous();
	}

	if (SunIntensityByAlpha == nullptr)
	{
		SunIntensityByAlpha = Settings.SunIntensityByAlpha.LoadSynchronous();
	}

	if (SunColorByAlpha == nullptr)
	{
		SunColorByAlpha = Settings.SunColorByAlpha.LoadSynchronous();
	}

	if (StreetLightIntensityByAlpha == nullptr)
	{
		StreetLightIntensityByAlpha = Settings.StreetLightIntensityByAlpha.LoadSynchronous();
	}
}

void UBDDayCycleComponent::RegisterStreetLight(UBDStreetLightComponent* Light)
{
	if (Light != nullptr)
	{
		StreetLights.AddUnique(Light);
	}
}

void UBDDayCycleComponent::UnregisterStreetLight(UBDStreetLightComponent* Light)
{
	StreetLights.Remove(Light);
}

float UBDDayCycleComponent::ComputeAlphaForWave(const int32 Wave) const
{
	const int32 Waves = FMath::Max(1, WavesPerCycle);
	const int32 Position = ((Wave % Waves) + Waves) % Waves;
	return static_cast<float>(Position) / static_cast<float>(Waves);
}

namespace BDDayDebug
{
	/** Debug: holds the sky where it is, so a high wave can be balanced in daylight. */
	static int32 GFreeze = 0;

	static FAutoConsoleVariableRef CVarFreeze(
		TEXT("BD.Day.Freeze"),
		GFreeze,
		TEXT("1 freezes the day cycle: waves stop moving the sun. 0 to resume, which catches up to the current wave."),
		ECVF_Cheat);
}

void UBDDayCycleComponent::SetWave(const int32 Wave)
{
	// Remembered even while frozen, so unfreezing lands on the right sky.
	LastWave = Wave;
	if (BDDayDebug::GFreeze != 0)
	{
		return;
	}

	TargetAlpha = ComputeAlphaForWave(Wave);
}

void UBDDayCycleComponent::SetAlphaImmediate(const float Alpha)
{
	// Clamped, not wrapped: 1.0 has to stay 1.0. On a cycle it means the same sky as 0.0,
	// but someone sweeping the curves needs the far end of the graph to hold still rather
	// than snap back to dawn under them.
	CycleAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
	TargetAlpha = CycleAlpha;
	ApplyCycle();
}

void UBDDayCycleComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Frozen: the sky stays put, whatever the waves did meanwhile. On unfreeze the target
	// is the sky of the last wave seen, and the blend takes it there.
	if (BDDayDebug::GFreeze != 0)
	{
		bWasFrozen = true;
		return;
	}
	if (bWasFrozen)
	{
		bWasFrozen = false;
		TargetAlpha = ComputeAlphaForWave(LastWave);
	}

	if (FMath::IsNearlyEqual(CycleAlpha, TargetAlpha))
	{
		return;
	}

	// The day only ever runs forwards, so the blend goes the long way round rather than
	// rewinding through the afternoon when a new cycle starts at dawn.
	float Remaining = TargetAlpha - CycleAlpha;
	if (Remaining < 0.0f)
	{
		Remaining += 1.0f;
	}

	const float Step = BlendSpeed * DeltaTime;
	CycleAlpha = Step >= Remaining ? TargetAlpha : FMath::Frac(CycleAlpha + Step);

	ApplyCycle();
}

UDirectionalLightComponent* UBDDayCycleComponent::ResolveSunLight()
{
	if (ResolvedSun.IsValid())
	{
		return ResolvedSun.Get();
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	// The component, not the actor: a plain directional light keeps it at the root, but a
	// Sun and Sky rig hangs it off a chain of others, and rotating that chain is not the
	// same thing as rotating the sun.
	UDirectionalLightComponent* Found = nullptr;
	if (const AActor* Configured = SunActor.LoadSynchronous())
	{
		Found = Configured->FindComponentByClass<UDirectionalLightComponent>();
		if (Found == nullptr)
		{
			UE_LOG(LogBDMatch, Warning, TEXT("Day cycle: %s carries no directional light component."),
				*Configured->GetName());
		}
	}

	if (Found == nullptr)
	{
		for (TActorIterator<AActor> It(World); It && Found == nullptr; ++It)
		{
			Found = It->FindComponentByClass<UDirectionalLightComponent>();
		}
	}

	if (Found == nullptr)
	{
		UE_LOG(LogBDMatch, Warning,
			TEXT("Day cycle found no directional light to drive, so the sky will not move. "
				 "Add one to the level, or point SunActor at whatever carries it."));
		return nullptr;
	}

	if (Found->Mobility != EComponentMobility::Movable)
	{
		// Worth saying out loud: the curves will evaluate, the rotation will be refused,
		// and the sun will sit still for reasons that have nothing to do with the curves.
		UE_LOG(LogBDMatch, Warning,
			TEXT("Day cycle: the sun on %s is %s, not Movable, so it cannot be rotated at runtime."),
			*GetNameSafe(Found->GetOwner()),
			Found->Mobility == EComponentMobility::Static ? TEXT("Static") : TEXT("Stationary"));
	}

	ResolvedSun = Found;
	return Found;
}

void UBDDayCycleComponent::ApplyCycle()
{
	if (UDirectionalLightComponent* Sun = ResolveSunLight())
	{
		if (SunPitchByAlpha != nullptr)
		{
			Sun->SetWorldRotation(FRotator(SunPitchByAlpha->GetFloatValue(CycleAlpha), SunYaw, 0.0f));
		}

		if (SunIntensityByAlpha != nullptr)
		{
			Sun->SetIntensity(SunIntensityByAlpha->GetFloatValue(CycleAlpha));
		}

		if (SunColorByAlpha != nullptr)
		{
			Sun->SetLightColor(SunColorByAlpha->GetLinearColorValue(CycleAlpha));
		}
	}

	const float StreetMultiplier = StreetLightIntensityByAlpha != nullptr
		? StreetLightIntensityByAlpha->GetFloatValue(CycleAlpha)
		: 0.0f;

	for (int32 Index = StreetLights.Num() - 1; Index >= 0; --Index)
	{
		if (UBDStreetLightComponent* Light = StreetLights[Index].Get())
		{
			Light->ApplyCycleIntensity(StreetMultiplier);
		}
		else
		{
			StreetLights.RemoveAtSwap(Index);
		}
	}
}

namespace BDDayCommands
{
	static constexpr int32 ArgCountSetAlpha = 1;

	/** Finds the day cycle of a world, wherever it happens to be mounted. */
	static UBDDayCycleComponent* FindDayCycle(const UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (UBDDayCycleComponent* Cycle = It->FindComponentByClass<UBDDayCycleComponent>())
			{
				return Cycle;
			}
		}

		return nullptr;
	}

	static void ExecSetAlpha(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != ArgCountSetAlpha)
		{
			UE_LOG(LogBDMatch, Error, TEXT("Usage: BD.Day.SetAlpha <0..1>"));
			return;
		}

		UBDDayCycleComponent* Cycle = FindDayCycle(World);
		if (Cycle == nullptr)
		{
			UE_LOG(LogBDMatch, Error, TEXT("BD.Day.SetAlpha: no day cycle in this world."));
			return;
		}

		const float Alpha = FCString::Atof(*Args[0]);
		Cycle->SetAlphaImmediate(Alpha);

		UE_LOG(LogBDMatch, Log, TEXT("BD.Day.SetAlpha: cycle at %.3f."), Cycle->GetCycleAlpha());
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSetAlpha(
		TEXT("BD.Day.SetAlpha"),
		TEXT("BD.Day.SetAlpha <0..1>: puts the day cycle straight at an alpha, for authoring the curves."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSetAlpha));
}

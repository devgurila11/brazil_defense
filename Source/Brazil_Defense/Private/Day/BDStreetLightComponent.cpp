// Brazil Defense. A street light: scenery that lights up at dusk and stands in the way.

#include "Day/BDStreetLightComponent.h"

#include "BDLog.h"
#include "Components/LightComponent.h"
#include "Components/MeshComponent.h"
#include "Day/BDDayCycleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Grid/BDGridSubsystem.h"
#include "Match/BDMatchManager.h"
#include "Materials/MaterialInterface.h"

namespace BDStreetLightPrivate
{
	/** Below this the lights are switched off outright rather than drawn at nothing. */
	static constexpr float DarkMultiplier = 0.001f;
}

UBDStreetLightComponent::UBDStreetLightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UBDGridSubsystem* UBDStreetLightComponent::GetGrid() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
}

void UBDStreetLightComponent::OnRegister()
{
	Super::OnRegister();

	// Marked on registration rather than at BeginPlay so the editor grid shows the
	// footprint too: a lamp post that only blocks in game is a path that only breaks
	// in game.
	MarkCellBlocked();
}

void UBDStreetLightComponent::MarkCellBlocked()
{
	if (!bOccupiesCell || GetOwner() == nullptr)
	{
		return;
	}

	UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr || Grid->GetCellCount() <= 0)
	{
		return;
	}

	FBDCellCoord Coord;
	if (!Grid->WorldToCell(GetOwner()->GetActorLocation(), Coord))
	{
		// Lamp posts outside the battle area are ordinary scenery, which is fine.
		return;
	}

	OccupiedCell = Coord;
	bHasOccupiedCell = true;
	Grid->SetCellState(Coord, EBDCellState::Blocked);
}

void UBDStreetLightComponent::BeginPlay()
{
	Super::BeginPlay();

	// The cell may not have been markable at registration, when a spawned actor has not
	// been moved into place yet.
	if (!bHasOccupiedCell)
	{
		MarkCellBlocked();
	}

	CaptureLitValues();

	if (const ABDMatchManager* Match = ABDMatchManager::Get(this))
	{
		if (UBDDayCycleComponent* Cycle = Match->GetDayCycle())
		{
			Cycle->RegisterStreetLight(this);
		}
	}
}

void UBDStreetLightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const ABDMatchManager* Match = ABDMatchManager::Get(this))
	{
		if (UBDDayCycleComponent* Cycle = Match->GetDayCycle())
		{
			Cycle->UnregisterStreetLight(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UBDStreetLightComponent::CaptureLitValues()
{
	AActor* Owner = GetOwner();
	if (bCapturedLitValues || Owner == nullptr)
	{
		return;
	}
	bCapturedLitValues = true;

	TArray<ULightComponent*> Lights;
	Owner->GetComponents(Lights);
	for (ULightComponent* Light : Lights)
	{
		LitLights.Add({ Light, Light->Intensity });
		Light->SetCastShadows(bLightsCastShadows);
	}

	if (GlowParameter.IsNone())
	{
		return;
	}

	TArray<UMeshComponent*> Meshes;
	Owner->GetComponents(Meshes);
	for (UMeshComponent* Mesh : Meshes)
	{
		// The brightest slot that has the parameter stands for the mesh: the lamp heads
		// are one material on any post seen so far.
		bool bFound = false;
		float LitValue = 0.0f;
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			float Value = 0.0f;
			const UMaterialInterface* Material = Mesh->GetMaterial(Slot);
			if (Material != nullptr && Material->GetScalarParameterValue(FHashedMaterialParameterInfo(GlowParameter), Value))
			{
				LitValue = bFound ? FMath::Max(LitValue, Value) : Value;
				bFound = true;
			}
		}

		if (bFound)
		{
			LitGlows.Add({ Mesh, LitValue });
		}
	}

	UE_LOG(LogBDMatch, Verbose, TEXT("Street light %s: %d lights, %d glowing meshes."),
		*Owner->GetName(), LitLights.Num(), LitGlows.Num());
}

void UBDStreetLightComponent::ApplyCycleIntensity(const float Multiplier)
{
	// The cycle may adopt this light before its own BeginPlay has run.
	CaptureLitValues();

	const float Clamped = FMath::Max(0.0f, Multiplier);
	if (FMath::IsNearlyEqual(Clamped, AppliedMultiplier, BDStreetLightPrivate::DarkMultiplier))
	{
		return;
	}
	AppliedMultiplier = Clamped;

	// Hidden rather than drawn at zero by day: an invisible light costs nothing, a dark
	// one still pays for its pass.
	const bool bLit = Clamped > BDStreetLightPrivate::DarkMultiplier;
	for (const FLitLight& Lit : LitLights)
	{
		if (ULightComponent* Light = Lit.Light.Get())
		{
			Light->SetIntensity(Lit.LitIntensity * Clamped);
			Light->SetVisibility(bLit);
		}
	}

	for (const FLitGlow& Glow : LitGlows)
	{
		if (UMeshComponent* Mesh = Glow.Mesh.Get())
		{
			Mesh->SetScalarParameterValueOnMaterials(GlowParameter, Glow.LitValue * Clamped);
		}
	}
}

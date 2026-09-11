// Brazil Defense. A street light: scenery that lights up at dusk and stands in the way.

#include "Day/BDStreetLightComponent.h"

#include "BDLog.h"
#include "Components/LightComponent.h"
#include "Day/BDDayCycleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Grid/BDGridSubsystem.h"
#include "Match/BDMatchManager.h"

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

void UBDStreetLightComponent::ApplyCycleIntensity(const float Multiplier)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	TArray<ULightComponent*> Lights;
	Owner->GetComponents(Lights);

	for (ULightComponent* Light : Lights)
	{
		Light->SetIntensity(LitIntensity * FMath::Max(0.0f, Multiplier));
	}
}

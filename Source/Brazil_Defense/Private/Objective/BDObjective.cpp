// Brazil Defense. The urn: the one point on the map every creep is walking to.

#include "Objective/BDObjective.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ABDObjective::ABDObjective()
{
	PrimaryActorTick.bCanEverTick = false;

	// The root is the point the creeps converge on; the mesh only dresses it.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);

	SetCanBeDamaged(false);

#if WITH_EDITOR
	bIsSpatiallyLoaded = false;
#endif
}

ABDObjective* ABDObjective::Get(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<ABDObjective> It(World); It; ++It)
	{
		return *It;
	}

	return nullptr;
}

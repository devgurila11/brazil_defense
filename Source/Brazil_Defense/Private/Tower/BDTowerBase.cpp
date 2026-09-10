// Brazil Defense. The single defender class, wherever it stands.

#include "Tower/BDTowerBase.h"

#include "Components/SceneComponent.h"
#include "Platform/BDPlatformComponent.h"

ABDTowerBase::ABDTowerBase()
{
	// Nothing to tick yet: targeting and firing come with the combat pass.
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

float ABDTowerBase::GetEffectiveRange() const
{
	const UBDPlatformComponent* CurrentPlatform = Platform.Get();
	return CurrentPlatform != nullptr ? BaseRange * CurrentPlatform->RangeMultiplier : BaseRange;
}

UBDPlatformComponent* ABDTowerBase::GetPlatform() const
{
	return Platform.Get();
}

bool ABDTowerBase::IsOnPlatform() const
{
	return Platform.IsValid() && PlatformSlotIndex != INDEX_NONE;
}

void ABDTowerBase::NotifyOccupiedSlot(UBDPlatformComponent* InPlatform, const int32 SlotIndex)
{
	Platform = InPlatform;
	PlatformSlotIndex = SlotIndex;
}

void ABDTowerBase::NotifyReleasedSlot()
{
	Platform.Reset();
	PlatformSlotIndex = INDEX_NONE;
}

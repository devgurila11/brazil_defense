// Brazil Defense. The piece the player clicked: the one whose reach is drawn.

#include "Placement/BDInspection.h"

#include "BDLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UBDInspectionSubsystem* UBDInspectionSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr && WorldContextObject != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World != nullptr ? World->GetSubsystem<UBDInspectionSubsystem>() : nullptr;
}

void UBDInspectionSubsystem::SetInspected(AActor* Actor)
{
	if (Inspected.Get() == Actor)
	{
		return;
	}
	Inspected = Actor;
	UE_LOG(LogBDGrid, Verbose, TEXT("Inspecting %s."), *GetNameSafe(Actor));
}

bool UBDInspectionSubsystem::IsInspected(const AActor* Actor, const AActor* Owner) const
{
	const AActor* Current = Inspected.Get();
	return Current != nullptr && (Current == Actor || (Owner != nullptr && Current == Owner));
}

bool UBDInspectionSubsystem::ShouldDrawReach(const int32 ShowRangeMode, const AActor* Actor, const AActor* Owner)
{
	if (ShowRangeMode <= 0)
	{
		return false;
	}
	if (ShowRangeMode >= 2)
	{
		return true;
	}
	const UBDInspectionSubsystem* Inspection = Get(Actor);
	return Inspection != nullptr && Inspection->IsInspected(Actor, Owner);
}

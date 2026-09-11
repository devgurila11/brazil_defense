// Brazil Defense. Grid debug drawing for the worlds that are actually played.

#include "Grid/BDGridDebugDrawer.h"

#include "Debug/DebugDrawService.h"
#include "Engine/World.h"
#include "Grid/BDGridDebug.h"
#include "Stats/Stats.h"

namespace BDGridDebugDrawerPrivate
{
	/**
	 * Engine show flags the canvas pass listens to. "Game" covers the PIE and standalone
	 * viewports; "Editor" is there for Simulate, where a game world is shown in a level
	 * editor viewport. The shared pass filters by world, so a viewport rendering another
	 * world is skipped instead of being drawn over.
	 */
	static const TCHAR* const ObservedShowFlags[] = { TEXT("Game"), TEXT("Editor") };
}

void UBDGridDebugDrawer::Initialize(FSubsystemCollectionBase& Collection)
{
	// Required by UTickableWorldSubsystem: ticking only starts once this runs.
	Super::Initialize(Collection);

	for (const TCHAR* ShowFlagName : BDGridDebugDrawerPrivate::ObservedShowFlags)
	{
		CanvasDrawHandles.Add(UDebugDrawService::Register(
			ShowFlagName,
			FDebugDrawDelegate::CreateUObject(this, &UBDGridDebugDrawer::DrawCoordLabels)));
	}
}

void UBDGridDebugDrawer::Deinitialize()
{
	for (const FDelegateHandle& Handle : CanvasDrawHandles)
	{
		UDebugDrawService::Unregister(Handle);
	}

	CanvasDrawHandles.Reset();

	Super::Deinitialize();
}

bool UBDGridDebugDrawer::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// Editor worlds are ABDGridVisualizer's job, so there is nothing to do there.
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UBDGridDebugDrawer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBDGridDebugDrawer, STATGROUP_Tickables);
}

void UBDGridDebugDrawer::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (const UWorld* World = GetWorld())
	{
		BDGridDebug::DrawGrid(*World);
	}
}

void UBDGridDebugDrawer::DrawCoordLabels(UCanvas* Canvas, APlayerController* PlayerController)
{
	const UWorld* World = GetWorld();
	if (Canvas != nullptr && World != nullptr)
	{
		BDGridDebug::DrawCoordLabels(*Canvas, *World);
	}
}

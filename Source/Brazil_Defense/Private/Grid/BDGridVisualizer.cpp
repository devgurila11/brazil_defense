// Brazil Defense. Debug visualization of the logical gameplay grid, in the editor.

#include "Grid/BDGridVisualizer.h"

#include "Components/SceneComponent.h"
#include "Debug/DebugDrawService.h"
#include "Engine/World.h"
#include "Grid/BDGridDebug.h"

ABDGridVisualizer::ABDGridVisualizer()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// The actor itself renders nothing: the root only gives it a transform and a
	// selection handle in the editor.
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SetHidden(true);
	SetCanBeDamaged(false);
}

void ABDGridVisualizer::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();

	RegisterCanvasDraw();
}

void ABDGridVisualizer::PostUnregisterAllComponents()
{
	UnregisterCanvasDraw();

	Super::PostUnregisterAllComponents();
}

void ABDGridVisualizer::RegisterCanvasDraw()
{
	if (CanvasDrawHandle.IsValid() || IsTemplate())
	{
		return;
	}

	// "Editor" is only set on level editor viewports, which is the only place this
	// actor draws. The game viewports are served by UBDGridDebugDrawer.
	CanvasDrawHandle = UDebugDrawService::Register(
		TEXT("Editor"),
		FDebugDrawDelegate::CreateUObject(this, &ABDGridVisualizer::DrawCoordLabels));
}

void ABDGridVisualizer::UnregisterCanvasDraw()
{
	if (CanvasDrawHandle.IsValid())
	{
		UDebugDrawService::Unregister(CanvasDrawHandle);
		CanvasDrawHandle.Reset();
	}
}

bool ABDGridVisualizer::ShouldDraw() const
{
	const UWorld* World = GetWorld();
	if (!bDrawGrid || World == nullptr)
	{
		return false;
	}

	// A game world is drawn by UBDGridDebugDrawer, whether or not this actor happened
	// to be streamed in. Drawing here as well would only double every line.
	return !World->IsGameWorld();
}

void ABDGridVisualizer::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ShouldDraw())
	{
		BDGridDebug::DrawGrid(*GetWorld());
	}
}

void ABDGridVisualizer::DrawCoordLabels(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (Canvas != nullptr && ShouldDraw())
	{
		BDGridDebug::DrawCoordLabels(*Canvas, *GetWorld());
	}
}

// Brazil Defense. Debug visualization of the logical gameplay grid.

#include "Grid/BDGridVisualizer.h"

#include "Components/SceneComponent.h"
#include "Debug/DebugDrawService.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Grid/BDGridDebug.h"
#include "Grid/BDGridSettings.h"
#include "Grid/BDGridSubsystem.h"
#include "SceneInterface.h"
#include "SceneView.h"

namespace BDGridVisualizerDraw
{
	/**
	 * Engine show flags the canvas pass listens to. "Editor" is only set on level
	 * editor viewports and "Game" only on game viewports, so together they cover
	 * both without drawing the labels twice in the same viewport.
	 */
	static const TCHAR* const ObservedShowFlags[] = { TEXT("Editor"), TEXT("Game") };
}

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
	if (CanvasDrawHandles.Num() > 0 || IsTemplate())
	{
		return;
	}

	for (const TCHAR* ShowFlagName : BDGridVisualizerDraw::ObservedShowFlags)
	{
		CanvasDrawHandles.Add(UDebugDrawService::Register(
			ShowFlagName,
			FDebugDrawDelegate::CreateUObject(this, &ABDGridVisualizer::DrawCoordLabels)));
	}
}

void ABDGridVisualizer::UnregisterCanvasDraw()
{
	for (const FDelegateHandle& Handle : CanvasDrawHandles)
	{
		UDebugDrawService::Unregister(Handle);
	}

	CanvasDrawHandles.Reset();
}

const UBDGridSubsystem* ABDGridVisualizer::GetGrid() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
}

bool ABDGridVisualizer::ShouldDraw() const
{
	const UWorld* World = GetWorld();
	if (!bDrawGrid || World == nullptr || !BDGridDebug::IsEnabled())
	{
		return false;
	}

	const UBDGridSettings& Settings = UBDGridSettings::Get();
	return World->IsGameWorld() ? Settings.bDrawInGame : Settings.bDrawInEditor;
}

void ABDGridVisualizer::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!ShouldDraw())
	{
		return;
	}

	const UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr || Grid->GetCellCount() <= 0)
	{
		return;
	}

	const UBDGridSettings& Settings = UBDGridSettings::Get();

	DrawGridLines(*Grid, Settings);
	DrawCellStates(*Grid, Settings);

	if (Settings.bDrawOriginMarker)
	{
		DrawOriginMarker(*Grid, Settings);
	}
}

void ABDGridVisualizer::DrawGridLines(const UBDGridSubsystem& Grid, const UBDGridSettings& Settings) const
{
	const UWorld* World = GetWorld();
	const FVector Corner = Grid.GetOrigin() + FVector(0.0f, 0.0f, Settings.DrawHeightOffset);
	const float SpanX = Grid.GetSizeX() * Grid.GetCellSize();
	const float SpanY = Grid.GetSizeY() * Grid.GetCellSize();

	// Internal lines. The outer ones are skipped here and drawn as the border below.
	for (int32 X = 1; X < Grid.GetSizeX(); ++X)
	{
		const FVector Offset(X * Grid.GetCellSize(), 0.0f, 0.0f);
		DrawDebugLine(World, Corner + Offset, Corner + Offset + FVector(0.0f, SpanY, 0.0f),
			Settings.LineColor, BDGridDebug::bPersistentLines,
			BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, Settings.LineThickness);
	}

	for (int32 Y = 1; Y < Grid.GetSizeY(); ++Y)
	{
		const FVector Offset(0.0f, Y * Grid.GetCellSize(), 0.0f);
		DrawDebugLine(World, Corner + Offset, Corner + Offset + FVector(SpanX, 0.0f, 0.0f),
			Settings.LineColor, BDGridDebug::bPersistentLines,
			BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, Settings.LineThickness);
	}

	const FVector BorderCorners[] = {
		Corner,
		Corner + FVector(SpanX, 0.0f, 0.0f),
		Corner + FVector(SpanX, SpanY, 0.0f),
		Corner + FVector(0.0f, SpanY, 0.0f)
	};

	constexpr int32 BorderCornerCount = UE_ARRAY_COUNT(BorderCorners);
	for (int32 Index = 0; Index < BorderCornerCount; ++Index)
	{
		const FVector& Start = BorderCorners[Index];
		const FVector& End = BorderCorners[(Index + 1) % BorderCornerCount];
		DrawDebugLine(World, Start, End, Settings.BorderColor, BDGridDebug::bPersistentLines,
			BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, Settings.BorderThickness);
	}
}

void ABDGridVisualizer::DrawCellStates(const UBDGridSubsystem& Grid, const UBDGridSettings& Settings) const
{
	const UWorld* World = GetWorld();
	const float HalfExtent = Grid.GetCellSize() * 0.5f * Settings.CellFillRatio;
	const FVector Extent(HalfExtent, HalfExtent, Settings.CellFillHeight * 0.5f);
	const float CenterHeight = Settings.DrawHeightOffset + Settings.CellFillHeight * 0.5f;

	for (int32 Y = 0; Y < Grid.GetSizeY(); ++Y)
	{
		for (int32 X = 0; X < Grid.GetSizeX(); ++X)
		{
			const FBDCellCoord Coord(X, Y);

			FColor FillColor;
			if (!Settings.TryGetCellStateColor(Grid.GetCellState(Coord), FillColor))
			{
				continue;
			}

			const FVector Center = Grid.CellToWorld(Coord) + FVector(0.0f, 0.0f, CenterHeight);
			DrawDebugSolidBox(World, Center, Extent, FillColor, BDGridDebug::bPersistentLines,
				BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority);
		}
	}
}

void ABDGridVisualizer::DrawOriginMarker(const UBDGridSubsystem& Grid, const UBDGridSettings& Settings) const
{
	const UWorld* World = GetWorld();
	const FVector Corner = Grid.GetOrigin() + FVector(0.0f, 0.0f, Settings.DrawHeightOffset);

	DrawDebugDirectionalArrow(World, Corner, Corner + FVector(Settings.OriginMarkerLength, 0.0f, 0.0f),
		Settings.OriginMarkerArrowSize, Settings.OriginMarkerColor, BDGridDebug::bPersistentLines,
		BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, Settings.BorderThickness);

	DrawDebugDirectionalArrow(World, Corner, Corner + FVector(0.0f, Settings.OriginMarkerLength, 0.0f),
		Settings.OriginMarkerArrowSize, Settings.OriginMarkerColor, BDGridDebug::bPersistentLines,
		BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, Settings.BorderThickness);
}

void ABDGridVisualizer::DrawCoordLabels(UCanvas* Canvas, APlayerController* PlayerController)
{
	if (Canvas == nullptr || Canvas->SceneView == nullptr || !ShouldDraw())
	{
		return;
	}

	const UBDGridSettings& Settings = UBDGridSettings::Get();
	if (!Settings.bDrawCellCoords || Settings.MaxCoordLabelsPerView <= 0)
	{
		return;
	}

	// The delegate fires for every viewport being rendered, including the PIE one
	// while the editor world is still loaded. Only draw into our own world.
	const FSceneInterface* Scene = Canvas->SceneView->Family != nullptr ? Canvas->SceneView->Family->Scene : nullptr;
	if (Scene == nullptr || Scene->GetWorld() != GetWorld())
	{
		return;
	}

	const UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr || Grid->GetCellCount() <= 0 || Grid->GetCellSize() <= 0.0f)
	{
		return;
	}

	UFont* Font = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	if (Font == nullptr)
	{
		return;
	}

	const FVector ViewLocation = Canvas->SceneView->ViewMatrices.GetViewOrigin();

	// Only the cells inside the draw distance can produce a label, so the sweep is
	// limited to that neighbourhood instead of the whole grid.
	const FBDCellCoord ViewCoord = Grid->WorldToCellUnclamped(ViewLocation);
	const int32 Radius = FMath::CeilToInt(Settings.CoordDrawDistance / Grid->GetCellSize());
	const int32 MinX = FMath::Max(0, ViewCoord.X - Radius);
	const int32 MaxX = FMath::Min(Grid->GetSizeX() - 1, ViewCoord.X + Radius);
	const int32 MinY = FMath::Max(0, ViewCoord.Y - Radius);
	const int32 MaxY = FMath::Min(Grid->GetSizeY() - 1, ViewCoord.Y + Radius);

	const float MaxDistanceSquared = FMath::Square(Settings.CoordDrawDistance);
	const float TextHeight = Font->GetMaxCharHeight() * Settings.CoordTextScale;

	int32 LabelsDrawn = 0;

	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const FBDCellCoord Coord(X, Y);
			const FVector CellCenter = Grid->CellToWorld(Coord) + FVector(0.0f, 0.0f, Settings.DrawHeightOffset);

			if (FVector::DistSquared(CellCenter, ViewLocation) > MaxDistanceSquared)
			{
				continue;
			}

			const FVector ScreenPosition = Canvas->Project(CellCenter);
			if (ScreenPosition.Z <= 0.0f)
			{
				continue;
			}

			const FString Label = Coord.ToString();
			const float TextWidth = Font->GetStringSize(*Label) * Settings.CoordTextScale;

			Canvas->SetDrawColor(Settings.CoordTextColor);
			Canvas->DrawText(Font, *Label,
				static_cast<float>(ScreenPosition.X) - TextWidth * 0.5f,
				static_cast<float>(ScreenPosition.Y) - TextHeight * 0.5f,
				Settings.CoordTextScale, Settings.CoordTextScale);

			if (++LabelsDrawn >= Settings.MaxCoordLabelsPerView)
			{
				return;
			}
		}
	}
}

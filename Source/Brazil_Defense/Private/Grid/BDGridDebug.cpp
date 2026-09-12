// Brazil Defense. Runtime switch and shared drawing of the grid debug.

#include "Grid/BDGridDebug.h"

#include "BDLog.h"
#include "DrawDebugHelpers.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Grid/BDGridSettings.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "SceneInterface.h"
#include "SceneView.h"

namespace BDGridDebug
{
	// Defaults to on so the grid stays visible while the layout is being authored.
	static int32 GGridDebugEnabled = 1;

	static FAutoConsoleVariableRef CVarGridDebug(
		TEXT("BD.Grid.Debug"),
		GGridDebugEnabled,
		TEXT("Brazil Defense grid debug drawing: grid lines, cell states, platform footprints and slots. 0 to disable."),
		ECVF_Cheat);

	bool IsEnabled()
	{
		return GGridDebugEnabled != 0;
	}

	bool ShouldDrawInWorld(const UWorld& World)
	{
		if (!IsEnabled())
		{
			return false;
		}

		const UBDGridSettings& Settings = UBDGridSettings::Get();
		return World.IsGameWorld() ? Settings.bDrawInGame : Settings.bDrawInEditor;
	}

	/** The grid of a world, or null when there is none to draw. */
	static const UBDGridSubsystem* GetDrawableGrid(const UWorld& World)
	{
		const UBDGridSubsystem* Grid = World.GetSubsystem<UBDGridSubsystem>();
		const bool bDrawable = Grid != nullptr && Grid->GetCellCount() > 0 && Grid->GetCellSize() > 0.0f;

		return bDrawable ? Grid : nullptr;
	}

	static void DrawGridLines(const UWorld& World, const UBDGridSubsystem& Grid, const UBDGridSettings& Settings)
	{
		const FVector Corner = Grid.GetOrigin() + FVector(0.0f, 0.0f, Settings.DrawHeightOffset);
		const float SpanX = Grid.GetSizeX() * Grid.GetCellSize();
		const float SpanY = Grid.GetSizeY() * Grid.GetCellSize();

		// Internal lines. The outer ones are skipped here and drawn as the border below.
		for (int32 X = 1; X < Grid.GetSizeX(); ++X)
		{
			const FVector Offset(X * Grid.GetCellSize(), 0.0f, 0.0f);
			DrawDebugLine(&World, Corner + Offset, Corner + Offset + FVector(0.0f, SpanY, 0.0f),
				Settings.LineColor, bPersistentLines, SingleFrameLifeTime, DepthPriority, Settings.LineThickness);
		}

		for (int32 Y = 1; Y < Grid.GetSizeY(); ++Y)
		{
			const FVector Offset(0.0f, Y * Grid.GetCellSize(), 0.0f);
			DrawDebugLine(&World, Corner + Offset, Corner + Offset + FVector(SpanX, 0.0f, 0.0f),
				Settings.LineColor, bPersistentLines, SingleFrameLifeTime, DepthPriority, Settings.LineThickness);
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
			DrawDebugLine(&World, Start, End, Settings.BorderColor, bPersistentLines,
				SingleFrameLifeTime, DepthPriority, Settings.BorderThickness);
		}
	}

	void DrawCellFill(const UWorld& World, const UBDGridSubsystem& Grid, const FBDCellCoord& Coord, const FColor& Color)
	{
		const UBDGridSettings& Settings = UBDGridSettings::Get();
		const float HalfExtent = Grid.GetCellSize() * 0.5f * Settings.CellFillRatio;
		const FVector Extent(HalfExtent, HalfExtent, Settings.CellFillHeight * 0.5f);
		const float CenterHeight = Settings.DrawHeightOffset + Settings.CellFillHeight * 0.5f;

		const FVector Center = Grid.CellToWorld(Coord) + FVector(0.0f, 0.0f, CenterHeight);
		DrawDebugSolidBox(&World, Center, Extent, Color, bPersistentLines, SingleFrameLifeTime, DepthPriority);
	}

	void DrawCellRectOutline(const UWorld& World, const UBDGridSubsystem& Grid,
		const FBDCellCoord& Min, const FBDCellCoord& Max, const FColor& Color, const float Thickness)
	{
		const UBDGridSettings& Settings = UBDGridSettings::Get();
		const FVector Lift(0.0f, 0.0f, Settings.DrawHeightOffset + Settings.CellFillHeight);

		// From the bottom-left corner of Min to the top-right corner of Max.
		const FVector Low = Grid.CellCornerToWorld(Min) + Lift;
		const FVector High = Grid.CellCornerToWorld(FBDCellCoord(Max.X + 1, Max.Y + 1)) + Lift;

		const FVector Corners[] = {
			Low,
			FVector(High.X, Low.Y, Low.Z),
			High,
			FVector(Low.X, High.Y, Low.Z)
		};

		constexpr int32 CornerCount = UE_ARRAY_COUNT(Corners);
		for (int32 Index = 0; Index < CornerCount; ++Index)
		{
			DrawDebugLine(&World, Corners[Index], Corners[(Index + 1) % CornerCount], Color,
				bPersistentLines, SingleFrameLifeTime, DepthPriority, Thickness);
		}
	}

	static void DrawCellStates(const UWorld& World, const UBDGridSubsystem& Grid, const UBDGridSettings& Settings)
	{
		for (int32 Y = 0; Y < Grid.GetSizeY(); ++Y)
		{
			for (int32 X = 0; X < Grid.GetSizeX(); ++X)
			{
				const FBDCellCoord Coord(X, Y);

				FColor FillColor;
				if (Settings.TryGetCellStateColor(Grid.GetCellState(Coord), FillColor))
				{
					DrawCellFill(World, Grid, Coord, FillColor);
				}
			}
		}
	}

	static void DrawBlockedEdges(const UWorld& World, const UBDGridSubsystem& Grid, const UBDGridSettings& Settings)
	{
		// Slightly above the cell fills so a fence between two painted cells stays visible.
		const FVector Lift(0.0f, 0.0f, Settings.DrawHeightOffset + Settings.CellFillHeight);

		TArray<FBDEdgeCoord> Edges;
		Grid.GetBlockedEdges(Edges);

		for (const FBDEdgeCoord& Edge : Edges)
		{
			FVector Start;
			FVector End;
			Grid.EdgeEndpointsToWorld(Edge, Start, End);
			DrawDebugLine(&World, Start + Lift, End + Lift, Settings.BlockedEdgeColor,
				bPersistentLines, SingleFrameLifeTime, DepthPriority, Settings.BlockedEdgeThickness);
		}
	}

	static void DrawOriginMarker(const UWorld& World, const UBDGridSubsystem& Grid, const UBDGridSettings& Settings)
	{
		const FVector Corner = Grid.GetOrigin() + FVector(0.0f, 0.0f, Settings.DrawHeightOffset);

		DrawDebugDirectionalArrow(&World, Corner, Corner + FVector(Settings.OriginMarkerLength, 0.0f, 0.0f),
			Settings.OriginMarkerArrowSize, Settings.OriginMarkerColor, bPersistentLines,
			SingleFrameLifeTime, DepthPriority, Settings.BorderThickness);

		DrawDebugDirectionalArrow(&World, Corner, Corner + FVector(0.0f, Settings.OriginMarkerLength, 0.0f),
			Settings.OriginMarkerArrowSize, Settings.OriginMarkerColor, bPersistentLines,
			SingleFrameLifeTime, DepthPriority, Settings.BorderThickness);
	}

	void DrawGrid(const UWorld& World)
	{
		if (!ShouldDrawInWorld(World))
		{
			return;
		}

		const UBDGridSubsystem* Grid = GetDrawableGrid(World);
		if (Grid == nullptr)
		{
			return;
		}

		const UBDGridSettings& Settings = UBDGridSettings::Get();

		DrawGridLines(World, *Grid, Settings);
		DrawCellStates(World, *Grid, Settings);
		DrawBlockedEdges(World, *Grid, Settings);

		if (Settings.bDrawOriginMarker)
		{
			DrawOriginMarker(World, *Grid, Settings);
		}
	}

	void DrawCoordLabels(UCanvas& Canvas, const UWorld& World)
	{
		if (Canvas.SceneView == nullptr || !ShouldDrawInWorld(World))
		{
			return;
		}

		const UBDGridSettings& Settings = UBDGridSettings::Get();
		if (!Settings.bDrawCellCoords || Settings.MaxCoordLabelsPerView <= 0)
		{
			return;
		}

		// The delegate fires for every viewport being rendered, including the PIE one
		// while the editor world is still loaded. Only draw into the world we were given.
		const FSceneInterface* Scene = Canvas.SceneView->Family != nullptr ? Canvas.SceneView->Family->Scene : nullptr;
		if (Scene == nullptr || Scene->GetWorld() != &World)
		{
			return;
		}

		const UBDGridSubsystem* Grid = GetDrawableGrid(World);
		if (Grid == nullptr)
		{
			return;
		}

		UFont* Font = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
		if (Font == nullptr)
		{
			return;
		}

		const FVector ViewLocation = Canvas.SceneView->ViewMatrices.GetViewOrigin();

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

				const FVector ScreenPosition = Canvas.Project(CellCenter);
				if (ScreenPosition.Z <= 0.0f)
				{
					continue;
				}

				const FString Label = Coord.ToString();
				const float TextWidth = Font->GetStringSize(*Label) * Settings.CoordTextScale;

				Canvas.SetDrawColor(Settings.CoordTextColor);
				Canvas.DrawText(Font, *Label,
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
}

namespace BDGridDebugCommands
{
	static constexpr int32 ArgCountSetCells = 5;

	/**
	 * Paints a rectangle of cells with one state, so the grid can be reshaped from the
	 * console while there is no placement UI yet. Debug tooling, not the game rule: the
	 * real placement path will go through the build validation instead.
	 */
	static void ExecSetCells(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("BD.Grid.SetCells needs a world."));
			return;
		}

		if (Args.Num() != ArgCountSetCells)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Grid.SetCells <x1> <y1> <x2> <y2> <State>"));
			return;
		}

		UBDGridSubsystem* Grid = World->GetSubsystem<UBDGridSubsystem>();
		if (Grid == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("BD.Grid.SetCells: no grid subsystem in this world."));
			return;
		}

		const UEnum* StateEnum = StaticEnum<EBDCellState>();
		const int64 StateValue = StateEnum != nullptr ? StateEnum->GetValueByNameString(Args[4]) : INDEX_NONE;
		if (StateValue == INDEX_NONE || StateValue >= static_cast<int64>(EBDCellState::Count))
		{
			UE_LOG(LogBDGrid, Error,
				TEXT("BD.Grid.SetCells: '%s' is not a cell state. Valid: Free, Tower, Platform, Blocked, Spawn, Goal."),
				*Args[4]);
			return;
		}

		const EBDCellState NewState = static_cast<EBDCellState>(StateValue);
		const int32 MinX = FMath::Min(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[2]));
		const int32 MaxX = FMath::Max(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[2]));
		const int32 MinY = FMath::Min(FCString::Atoi(*Args[1]), FCString::Atoi(*Args[3]));
		const int32 MaxY = FMath::Max(FCString::Atoi(*Args[1]), FCString::Atoi(*Args[3]));

		int32 ChangedCount = 0;
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			for (int32 X = MinX; X <= MaxX; ++X)
			{
				ChangedCount += Grid->SetCellState(FBDCellCoord(X, Y), NewState) ? 1 : 0;
			}
		}

		UE_LOG(LogBDGrid, Log, TEXT("BD.Grid.SetCells (%d,%d)-(%d,%d) to %s: %d cell(s) changed."),
			MinX, MinY, MaxX, MaxY, *StateEnum->GetNameStringByValue(StateValue), ChangedCount);
	}

	static constexpr int32 ArgCountSetEdge = 4;

	/** Blocks or frees one edge by hand, the edge counterpart of SetCells. */
	static void ExecSetEdge(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || Args.Num() != ArgCountSetEdge)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Grid.SetEdge <x> <y> <dir: 0=+X 1=+Y> <0|1>"));
			return;
		}

		UBDGridSubsystem* Grid = World->GetSubsystem<UBDGridSubsystem>();
		if (Grid == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("BD.Grid.SetEdge: no grid subsystem in this world."));
			return;
		}

		const int32 Direction = FCString::Atoi(*Args[2]);
		if (Direction < 0 || Direction >= FBDEdgeCoord::DirectionCount)
		{
			UE_LOG(LogBDGrid, Error, TEXT("BD.Grid.SetEdge: direction must be 0 (+X) or 1 (+Y), got '%s'."), *Args[2]);
			return;
		}

		const FBDEdgeCoord Edge(FBDCellCoord(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1])), static_cast<uint8>(Direction));
		const bool bBlocked = FCString::Atoi(*Args[3]) != 0;

		if (!Grid->CanBlockEdge(Edge))
		{
			UE_LOG(LogBDGrid, Error, TEXT("BD.Grid.SetEdge %s: not an edge between two cells of the grid."), *Edge.ToString());
			return;
		}

		UE_LOG(LogBDGrid, Log, TEXT("BD.Grid.SetEdge %s %s: %s."), *Edge.ToString(),
			bBlocked ? TEXT("blocked") : TEXT("freed"),
			Grid->SetEdgeBlocked(Edge, bBlocked) ? TEXT("changed") : TEXT("already so"));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSetEdge(
		TEXT("BD.Grid.SetEdge"),
		TEXT("BD.Grid.SetEdge <x> <y> <dir: 0=+X 1=+Y> <0|1>: blocks or frees the edge on that side of a cell."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSetEdge));

	static FAutoConsoleCommandWithWorldAndArgs CmdSetCells(
		TEXT("BD.Grid.SetCells"),
		TEXT("BD.Grid.SetCells <x1> <y1> <x2> <y2> <State>: paints a rectangle of cells with a state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSetCells));
}

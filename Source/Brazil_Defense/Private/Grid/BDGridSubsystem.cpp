// Brazil Defense. Logical gameplay grid.

#include "Grid/BDGridSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Grid/BDGridSettings.h"

void UBDGridSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RebuildFromSettings();

#if WITH_EDITOR
	// Tuning the layout in Project Settings must update the grid immediately, so the
	// debug visualization can be aligned against the level without restarting.
	SettingsChangedHandle = GetMutableDefault<UBDGridSettings>()->OnSettingChanged().AddUObject(
		this, &UBDGridSubsystem::HandleSettingsChanged);
#endif
}

void UBDGridSubsystem::Deinitialize()
{
#if WITH_EDITOR
	if (SettingsChangedHandle.IsValid())
	{
		GetMutableDefault<UBDGridSettings>()->OnSettingChanged().Remove(SettingsChangedHandle);
		SettingsChangedHandle.Reset();
	}
#endif

	Cells.Empty();
	BlockedEdges.Empty();

	Super::Deinitialize();
}

UBDGridSubsystem* UBDGridSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	return World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
}

void UBDGridSubsystem::RebuildFromSettings()
{
	const UBDGridSettings& Settings = UBDGridSettings::Get();

	if (ApplyLayout(Settings))
	{
		++Version;
		OnGridRebuilt.Broadcast();
	}
}

bool UBDGridSubsystem::ApplyLayout(const UBDGridSettings& Settings)
{
	const int32 NewSizeX = FMath::Max(1, Settings.GridSizeX);
	const int32 NewSizeY = FMath::Max(1, Settings.GridSizeY);

	const bool bDimensionsChanged = NewSizeX != SizeX || NewSizeY != SizeY;
	const bool bTransformChanged = !FMath::IsNearlyEqual(CellSize, Settings.CellSize) || !Origin.Equals(Settings.GridOrigin);

	SizeX = NewSizeX;
	SizeY = NewSizeY;
	CellSize = Settings.CellSize;
	Origin = Settings.GridOrigin;

	// Cell states only survive while the matrix keeps its dimensions. Moving or
	// scaling the grid keeps the states, since the indices stay meaningful.
	if (bDimensionsChanged || Cells.Num() != SizeX * SizeY)
	{
		Cells.Init(EBDCellState::Free, SizeX * SizeY);
		BlockedEdges.Init(false, SizeX * SizeY * FBDEdgeCoord::DirectionCount);
		return true;
	}

	return bTransformChanged;
}

void UBDGridSubsystem::ResetAllCells()
{
	for (EBDCellState& Cell : Cells)
	{
		Cell = EBDCellState::Free;
	}

	BlockedEdges.Init(false, Cells.Num() * FBDEdgeCoord::DirectionCount);

	++Version;
	OnGridRebuilt.Broadcast();
}

int32 UBDGridSubsystem::ApplyAuthoredLayout(const TMap<FBDCellCoord, EBDCellState>& Layout)
{
	int32 ChangedCount = 0;
	for (const TPair<FBDCellCoord, EBDCellState>& Entry : Layout)
	{
		ChangedCount += SetCellState(Entry.Key, Entry.Value) ? 1 : 0;
	}

	return ChangedCount;
}

int32 UBDGridSubsystem::ApplyAuthoredEdges(const TArray<FBDEdgeCoord>& Edges)
{
	int32 ChangedCount = 0;
	for (const FBDEdgeCoord& Edge : Edges)
	{
		ChangedCount += SetEdgeBlocked(Edge, true) ? 1 : 0;
	}

	return ChangedCount;
}

bool UBDGridSubsystem::IsValidCoord(const FBDCellCoord& Coord) const
{
	return Coord.X >= 0 && Coord.X < SizeX && Coord.Y >= 0 && Coord.Y < SizeY;
}

//~ Edges -----------------------------------------------------------------------

int32 UBDGridSubsystem::EdgeToIndex(const FBDEdgeCoord& Edge) const
{
	if (!IsValidCoord(Edge.Cell) || Edge.Direction >= FBDEdgeCoord::DirectionCount)
	{
		return INDEX_NONE;
	}

	return CoordToIndex(Edge.Cell) * FBDEdgeCoord::DirectionCount + Edge.Direction;
}

bool UBDGridSubsystem::CanBlockEdge(const FBDEdgeCoord& Edge) const
{
	// Both sides have to be cells of the grid: the border has nothing to separate.
	return IsValidCoord(Edge.Cell) && IsValidCoord(Edge.GetOtherCell())
		&& Edge.Direction < FBDEdgeCoord::DirectionCount;
}

bool UBDGridSubsystem::IsEdgeBlockedAt(const FBDEdgeCoord& Edge) const
{
	const int32 Index = EdgeToIndex(Edge);
	return Index != INDEX_NONE && BlockedEdges.IsValidIndex(Index) && BlockedEdges[Index];
}

bool UBDGridSubsystem::IsEdgeBlocked(const FBDCellCoord& From, const FBDCellCoord& To) const
{
	bool bAdjacent = false;
	const FBDEdgeCoord Edge = FBDEdgeCoord::Between(From, To, bAdjacent);

	// Cells that do not touch share no edge, so there is nothing to be blocked. Whether
	// the move itself is legal is the pathfinder's business, not this one's.
	return bAdjacent && IsEdgeBlockedAt(Edge);
}

bool UBDGridSubsystem::SetEdgeBlocked(const FBDEdgeCoord& Edge, const bool bBlocked)
{
	if (!CanBlockEdge(Edge))
	{
		return false;
	}

	const int32 Index = EdgeToIndex(Edge);
	if (!BlockedEdges.IsValidIndex(Index) || BlockedEdges[Index] == bBlocked)
	{
		return false;
	}

	BlockedEdges[Index] = bBlocked;

	// Same version as the cells: an edge changes what the pathfinder answers just as
	// much as a cell does, and its cache keys on this number.
	++Version;
	OnEdgeBlockedChanged.Broadcast(Edge, bBlocked);
	return true;
}

TArray<FBDEdgeCoord> UBDGridSubsystem::GetEdgesForSegment(const FBDCellCoord& Start, const int32 Length, const uint8 Direction) const
{
	TArray<FBDEdgeCoord> Edges;
	const int32 Count = FMath::Max(1, Length);
	Edges.Reserve(Count);

	// A +X edge is a line along Y, so a run of them advances along Y; and the other way
	// round for +Y. The segment continues the line, it does not step across it.
	const int32 StepX = Direction == FBDEdgeCoord::DirectionX ? 0 : 1;
	const int32 StepY = Direction == FBDEdgeCoord::DirectionX ? 1 : 0;

	for (int32 Step = 0; Step < Count; ++Step)
	{
		Edges.Emplace(FBDCellCoord(Start.X + StepX * Step, Start.Y + StepY * Step), Direction);
	}

	return Edges;
}

void UBDGridSubsystem::GetBlockedEdges(TArray<FBDEdgeCoord>& OutEdges) const
{
	OutEdges.Reset();

	for (TConstSetBitIterator<> It(BlockedEdges); It; ++It)
	{
		const int32 CellIndex = It.GetIndex() / FBDEdgeCoord::DirectionCount;
		const uint8 Direction = static_cast<uint8>(It.GetIndex() % FBDEdgeCoord::DirectionCount);
		OutEdges.Emplace(FBDCellCoord(CellIndex % SizeX, CellIndex / SizeX), Direction);
	}
}

EBDCellState UBDGridSubsystem::GetCellState(const FBDCellCoord& Coord) const
{
	// Reading outside the grid behaves like solid level geometry, so callers that
	// walk neighbours do not need a bounds check of their own.
	return IsValidCoord(Coord) ? Cells[CoordToIndex(Coord)] : EBDCellState::Blocked;
}

bool UBDGridSubsystem::SetCellState(const FBDCellCoord& Coord, const EBDCellState NewState)
{
	if (!IsValidCoord(Coord) || NewState == EBDCellState::Count)
	{
		return false;
	}

	EBDCellState& Cell = Cells[CoordToIndex(Coord)];
	if (Cell == NewState)
	{
		return false;
	}

	Cell = NewState;
	++Version;
	OnCellStateChanged.Broadcast(Coord, NewState);
	return true;
}

bool UBDGridSubsystem::IsWalkableState(const EBDCellState State)
{
	switch (State)
	{
	case EBDCellState::Free:
	// A tower occupies its cell but does not stand in the way: creeps walk through it.
	case EBDCellState::Tower:
	case EBDCellState::Spawn:
	case EBDCellState::Goal:
		return true;

	// Platform and Blocked both stop movement. What separates them is whether the
	// player can take them back, which is IsPlayerPlacedState. Dividers are not here
	// at all: they block edges, not cells.
	default:
		return false;
	}
}

bool UBDGridSubsystem::IsBuildableState(const EBDCellState State)
{
	return State == EBDCellState::Free;
}

bool UBDGridSubsystem::IsPlayerPlacedState(const EBDCellState State)
{
	switch (State)
	{
	case EBDCellState::Tower:
	case EBDCellState::Platform:
		return true;

	default:
		return false;
	}
}

bool UBDGridSubsystem::IsWalkable(const FBDCellCoord& Coord) const
{
	return IsWalkableState(GetCellState(Coord));
}

bool UBDGridSubsystem::IsBuildable(const FBDCellCoord& Coord) const
{
	return IsBuildableState(GetCellState(Coord));
}

bool UBDGridSubsystem::IsPlayerPlaced(const FBDCellCoord& Coord) const
{
	return IsPlayerPlacedState(GetCellState(Coord));
}

FVector UBDGridSubsystem::CellCornerToWorld(const FBDCellCoord& Coord) const
{
	return Origin + FVector(Coord.X * CellSize, Coord.Y * CellSize, 0.0f);
}

FVector UBDGridSubsystem::CellToWorld(const FBDCellCoord& Coord) const
{
	const float HalfCell = CellSize * 0.5f;
	return CellCornerToWorld(Coord) + FVector(HalfCell, HalfCell, 0.0f);
}

FBDCellCoord UBDGridSubsystem::WorldToCellUnclamped(const FVector& WorldLocation) const
{
	if (CellSize <= 0.0f)
	{
		return FBDCellCoord();
	}

	const FVector Local = WorldLocation - Origin;
	return FBDCellCoord(
		FMath::FloorToInt32(Local.X / CellSize),
		FMath::FloorToInt32(Local.Y / CellSize));
}

bool UBDGridSubsystem::WorldToCell(const FVector& WorldLocation, FBDCellCoord& OutCoord) const
{
	OutCoord = WorldToCellUnclamped(WorldLocation);
	return IsValidCoord(OutCoord);
}

FVector UBDGridSubsystem::EdgeToWorld(const FBDEdgeCoord& Edge) const
{
	return (CellToWorld(Edge.Cell) + CellToWorld(Edge.GetOtherCell())) * 0.5f;
}

void UBDGridSubsystem::EdgeEndpointsToWorld(const FBDEdgeCoord& Edge, FVector& OutStart, FVector& OutEnd) const
{
	// The far corner of the cell in the edge direction, then along the other axis.
	const FVector Corner = CellCornerToWorld(Edge.Cell);
	if (Edge.Direction == FBDEdgeCoord::DirectionX)
	{
		OutStart = Corner + FVector(CellSize, 0.0f, 0.0f);
		OutEnd = Corner + FVector(CellSize, CellSize, 0.0f);
	}
	else
	{
		OutStart = Corner + FVector(0.0f, CellSize, 0.0f);
		OutEnd = Corner + FVector(CellSize, CellSize, 0.0f);
	}
}

#if WITH_EDITOR
void UBDGridSubsystem::HandleSettingsChanged(UObject* ChangedObject, FPropertyChangedEvent& PropertyChangedEvent)
{
	RebuildFromSettings();
}
#endif

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

	OnGridRebuilt.Broadcast();
}

bool UBDGridSubsystem::IsValidCoord(const FBDCellCoord& Coord) const
{
	return Coord.X >= 0 && Coord.X < SizeX && Coord.Y >= 0 && Coord.Y < SizeY;
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

	// Divider, Platform and Blocked all stop movement. What separates them is whether
	// the player can take them back, which is IsPlayerPlacedState.
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
	case EBDCellState::Divider:
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

#if WITH_EDITOR
void UBDGridSubsystem::HandleSettingsChanged(UObject* ChangedObject, FPropertyChangedEvent& PropertyChangedEvent)
{
	RebuildFromSettings();
}
#endif

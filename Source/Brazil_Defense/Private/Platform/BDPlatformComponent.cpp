// Brazil Defense. Platforms that hold towers above the ground.

#include "Platform/BDPlatformComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "BDLog.h"
#include "Grid/BDGridDebug.h"
#include "Grid/BDGridSettings.h"
#include "Grid/BDGridSubsystem.h"
#include "Tower/BDTowerBase.h"

UBDPlatformComponent::UBDPlatformComponent()
{
	// Ticks to keep the footprint in sync while the platform is being placed and to
	// draw the slot markers. Both are cheap and both are needed in the editor.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;
}

void UBDPlatformComponent::OnRegister()
{
	Super::OnRegister();

	if (UBDGridSubsystem* Grid = GetGrid())
	{
		GridRebuiltHandle = Grid->OnGridRebuilt.AddUObject(this, &UBDPlatformComponent::HandleGridRebuilt);
	}

	ApplyFootprintToGrid();
	UpdateStampedTransform();
}

void UBDPlatformComponent::OnUnregister()
{
	if (UBDGridSubsystem* Grid = GetGrid())
	{
		Grid->OnGridRebuilt.Remove(GridRebuiltHandle);
	}
	GridRebuiltHandle.Reset();

	ClearFootprintFromGrid();

	Super::OnUnregister();
}

void UBDPlatformComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (Owner == nullptr || World == nullptr)
	{
		return;
	}

	// Dragging the platform in the editor, or moving it at runtime, has to move its
	// footprint with it.
	if (!Owner->GetActorTransform().Equals(StampedTransform))
	{
		RefreshFootprint();
	}

	if (!BDGridDebug::IsEnabled())
	{
		return;
	}

	const UBDGridSettings& Settings = UBDGridSettings::Get();
	if (World->IsGameWorld() ? !Settings.bDrawInGame : !Settings.bDrawInEditor)
	{
		return;
	}

	if (Settings.bDrawPlatformSlots)
	{
		DrawDebugSlots(Settings);
	}

	if (Settings.bDrawPlatformFootprint)
	{
		DrawDebugFootprint(Settings);
	}
}

UBDGridSubsystem* UBDPlatformComponent::GetGrid() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
}

//~ Slots ---------------------------------------------------------------------

bool UBDPlatformComponent::IsSlotFree(const int32 SlotIndex) const
{
	// A stale weak pointer means the tower is gone, which frees the slot.
	return Slots.IsValidIndex(SlotIndex) && !Slots[SlotIndex].Occupant.IsValid();
}

int32 UBDPlatformComponent::GetFreeSlotCount() const
{
	int32 FreeCount = 0;
	for (const FBDPlatformSlot& Slot : Slots)
	{
		FreeCount += Slot.Occupant.IsValid() ? 0 : 1;
	}

	return FreeCount;
}

int32 UBDPlatformComponent::FindFirstFreeSlot() const
{
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!Slots[Index].Occupant.IsValid())
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

ABDTowerBase* UBDPlatformComponent::GetSlotOccupant(const int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex].Occupant.Get() : nullptr;
}

FTransform UBDPlatformComponent::GetSlotWorldTransform(const int32 SlotIndex) const
{
	const AActor* Owner = GetOwner();
	if (!Slots.IsValidIndex(SlotIndex) || Owner == nullptr)
	{
		return FTransform::Identity;
	}

	const FBDPlatformSlot& Slot = Slots[SlotIndex];
	const FTransform LocalTransform(Slot.LocalRotation, Slot.LocalOffset);
	return LocalTransform * Owner->GetActorTransform();
}

bool UBDPlatformComponent::TryOccupy(const int32 SlotIndex, ABDTowerBase* Tower)
{
	if (Tower == nullptr || !IsSlotFree(SlotIndex) || Tower->IsOnPlatform())
	{
		return false;
	}

	Slots[SlotIndex].Occupant = Tower;

	const FTransform SlotTransform = GetSlotWorldTransform(SlotIndex);
	Tower->SetActorLocationAndRotation(SlotTransform.GetLocation(), SlotTransform.GetRotation());
	Tower->NotifyOccupiedSlot(this, SlotIndex);

	OnSlotChanged.Broadcast(SlotIndex);
	return true;
}

bool UBDPlatformComponent::Release(const int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	FBDPlatformSlot& Slot = Slots[SlotIndex];
	if (!Slot.Occupant.IsValid())
	{
		// Already free, possibly because the tower was destroyed. Drop the stale pointer.
		Slot.Occupant.Reset();
		return false;
	}

	ABDTowerBase* Tower = Slot.Occupant.Get();
	Slot.Occupant.Reset();
	Tower->NotifyReleasedSlot();

	OnSlotChanged.Broadcast(SlotIndex);
	return true;
}

//~ Grid footprint ------------------------------------------------------------

void UBDPlatformComponent::ClearPlacedFootprint()
{
	bLifted = true;
	ClearFootprintFromGrid();
	UpdateStampedTransform();
}

void UBDPlatformComponent::SetPlacedFootprint(const FBDCellCoord& Origin, const FIntPoint& Footprint)
{
	bLifted = false;
	bHasPlacedFootprint = true;
	PlacedOrigin = Origin;
	PlacedFootprint = FIntPoint(FMath::Max(1, Footprint.X), FMath::Max(1, Footprint.Y));
	RefreshFootprint();
}

bool UBDPlatformComponent::GetFootprintOrigin(FBDCellCoord& OutCoord) const
{
	const UBDGridSubsystem* Grid = GetGrid();
	const AActor* Owner = GetOwner();
	if (Grid == nullptr || Owner == nullptr)
	{
		return false;
	}

	if (bHasPlacedFootprint)
	{
		OutCoord = PlacedOrigin;
		return Grid->IsValidCoord(OutCoord);
	}

	return Grid->WorldToCell(Owner->GetActorLocation(), OutCoord);
}

void UBDPlatformComponent::GetFootprintCells(TArray<FBDCellCoord>& OutCells) const
{
	OutCells.Reset();

	const UBDGridSubsystem* Grid = GetGrid();
	FBDCellCoord OriginCoord;
	if (Grid == nullptr || !GetFootprintOrigin(OriginCoord))
	{
		return;
	}

	// The footprint grows from the origin cell towards +X and +Y. For an authored
	// platform the owner pivot is that cell; a placed one was told its cells outright.
	const FIntPoint Span = bHasPlacedFootprint ? PlacedFootprint : GridFootprint;
	const int32 SpanX = FMath::Max(1, Span.X);
	const int32 SpanY = FMath::Max(1, Span.Y);
	OutCells.Reserve(SpanX * SpanY);

	for (int32 Y = 0; Y < SpanY; ++Y)
	{
		for (int32 X = 0; X < SpanX; ++X)
		{
			const FBDCellCoord Coord(OriginCoord.X + X, OriginCoord.Y + Y);
			if (Grid->IsValidCoord(Coord))
			{
				OutCells.Add(Coord);
			}
		}
	}
}

void UBDPlatformComponent::ApplyFootprintToGrid()
{
	UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr || bLifted)
	{
		return;
	}

	if (!bInsideBattleArea)
	{
		// The platform stands outside the battle area, on the surrounding terrain.
		// WorldToCell finds no cell under it, so there is nothing to stamp and no
		// ambiguity about what its cells mean. Reaching into the grid is a mistake.
		ValidateOutsideBattleArea();
		return;
	}

	TArray<FBDCellCoord> FootprintCells;
	GetFootprintCells(FootprintCells);

	StampedCells.Reserve(FootprintCells.Num());
	for (const FBDCellCoord& Coord : FootprintCells)
	{
		StampedCells.Emplace(Coord, Grid->GetCellState(Coord));
		Grid->SetCellState(Coord, EBDCellState::Platform);
	}
}

void UBDPlatformComponent::ValidateOutsideBattleArea()
{
	TArray<FBDCellCoord> FootprintCells;
	GetFootprintCells(FootprintCells);

	if (FootprintCells.Num() == 0)
	{
		// Correctly placed: reset the guard so a later mistake is reported again.
		bWarnedOutsideBattleArea = false;
		return;
	}

	if (!bWarnedOutsideBattleArea)
	{
		bWarnedOutsideBattleArea = true;
		UE_LOG(LogBDGrid, Warning,
			TEXT("Platform '%s' is marked as outside the battle area but its footprint covers %d grid ")
			TEXT("cell(s) starting at %s. Anything on the playable area must set bInsideBattleArea. ")
			TEXT("Move the platform off the grid, or enable bInsideBattleArea."),
			*GetNameSafe(GetOwner()), FootprintCells.Num(), *FootprintCells[0].ToString());
	}

	ensureMsgf(false, TEXT("Platform outside the battle area overlaps the grid. See the LogBDGrid warning."));
}

void UBDPlatformComponent::ClearFootprintFromGrid()
{
	if (UBDGridSubsystem* Grid = GetGrid())
	{
		for (const TPair<FBDCellCoord, EBDCellState>& Stamped : StampedCells)
		{
			Grid->SetCellState(Stamped.Key, Stamped.Value);
		}
	}

	StampedCells.Reset();
}

void UBDPlatformComponent::RefreshFootprint()
{
	ClearFootprintFromGrid();
	ApplyFootprintToGrid();
	UpdateStampedTransform();
}

void UBDPlatformComponent::HandleGridRebuilt()
{
	// The grid was resized or reset, so the remembered states no longer describe
	// anything. Drop them instead of writing them back over fresh cells.
	StampedCells.Reset();
	ApplyFootprintToGrid();
	UpdateStampedTransform();
}

void UBDPlatformComponent::UpdateStampedTransform()
{
	if (const AActor* Owner = GetOwner())
	{
		StampedTransform = Owner->GetActorTransform();
	}
}

//~ Debug drawing -------------------------------------------------------------

void UBDPlatformComponent::DrawDebugSlots(const UBDGridSettings& Settings) const
{
	const UWorld* World = GetWorld();

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FTransform SlotTransform = GetSlotWorldTransform(Index);
		const FVector SlotLocation = SlotTransform.GetLocation();
		const FColor SlotColor = Slots[Index].Occupant.IsValid() ? Settings.SlotOccupiedColor : Settings.SlotFreeColor;

		DrawDebugSphere(World, SlotLocation, Settings.SlotMarkerRadius, Settings.SlotMarkerSegments, SlotColor,
			BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, Settings.LineThickness);

		// Shows which way a tower mounted here would face.
		const FVector Forward = SlotTransform.GetRotation().GetForwardVector();
		DrawDebugDirectionalArrow(World, SlotLocation, SlotLocation + Forward * Settings.SlotForwardLength,
			Settings.OriginMarkerArrowSize, SlotColor, BDGridDebug::bPersistentLines,
			BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, Settings.LineThickness);
	}
}

void UBDPlatformComponent::DrawDebugFootprint(const UBDGridSettings& Settings) const
{
	const UBDGridSubsystem* Grid = GetGrid();
	FBDCellCoord OriginCoord;
	if (Grid == nullptr || !GetFootprintOrigin(OriginCoord))
	{
		return;
	}

	const float CellSize = Grid->GetCellSize();
	const float SpanX = FMath::Max(1, GridFootprint.X) * CellSize;
	const float SpanY = FMath::Max(1, GridFootprint.Y) * CellSize;

	const FVector Corner = Grid->CellCornerToWorld(OriginCoord) + FVector(0.0f, 0.0f, Settings.DrawHeightOffset);
	const FVector Center = Corner + FVector(SpanX * 0.5f, SpanY * 0.5f, Settings.PlatformFootprintHeight * 0.5f);
	const FVector Extent(SpanX * 0.5f, SpanY * 0.5f, Settings.PlatformFootprintHeight * 0.5f);

	// A platform outside the battle area has no cell under it, so this only ever draws
	// in the error color when one is marked as outside yet still overlaps the grid.
	const FColor FootprintColor = bInsideBattleArea ? Settings.PlatformFootprintColor : Settings.PlatformOutsideFootprintColor;

	DrawDebugBox(GetWorld(), Center, Extent, FootprintColor, BDGridDebug::bPersistentLines,
		BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, Settings.BorderThickness);
}

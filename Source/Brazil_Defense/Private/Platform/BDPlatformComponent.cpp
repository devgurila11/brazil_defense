// Brazil Defense. Platforms that hold towers above the ground.

#include "Platform/BDPlatformComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "BDLog.h"
#include "Grid/BDGridDebug.h"
#include "Grid/BDGridSettings.h"
#include "Grid/BDGridSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Tower/BDTowerBase.h"
#include "Tower/BDTowerData.h"

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

	// The rank it wears follows the people on it. Read every tick rather than pushed
	// from the upgrade, the removal and the load separately: three places to forget,
	// against one cheap min over a handful of slots.
	RefreshVisualLevel();

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

//~ Climbing as a block -------------------------------------------------------------

bool UBDPlatformComponent::IsFullyManned() const
{
	if (Slots.Num() == 0)
	{
		return false;
	}

	for (const FBDPlatformSlot& Slot : Slots)
	{
		if (!Slot.Occupant.IsValid())
		{
			return false;
		}
	}
	return true;
}

int32 UBDPlatformComponent::GetBlockLevel() const
{
	// Not fully manned is not a level: a scaffold with a hole in it is not a storey.
	if (!IsFullyManned())
	{
		return 0;
	}

	int32 Lowest = MAX_int32;
	for (const FBDPlatformSlot& Slot : Slots)
	{
		if (const ABDTowerBase* Occupant = Slot.Occupant.Get())
		{
			Lowest = FMath::Min(Lowest, Occupant->GetTowerLevel());
		}
	}
	return Lowest == MAX_int32 ? 0 : FMath::Max(0, Lowest);
}

bool UBDPlatformComponent::CanOccupantEvolve(const ABDTowerBase& Occupant, FString& OutReason) const
{
	if (!IsFullyManned())
	{
		OutReason = FString::Printf(TEXT("the platform has %d empty slot(s): nobody on it evolves until every slot is filled"),
			GetFreeSlotCount());
		return false;
	}

	// Only the lowest may buy the next level, which is what makes them climb together:
	// everyone at N, then one at a time to N+1, and the last one to pay finishes the
	// floor. Nobody gets two levels ahead of the shooter beside them.
	const int32 Block = GetBlockLevel();
	if (Occupant.GetTowerLevel() > Block)
	{
		OutReason = FString::Printf(TEXT("it is already at level %d and the platform is on floor %d: the shooters at level %d buy the next one first"),
			Occupant.GetTowerLevel(), Block, Block);
		return false;
	}

	return true;
}

//~ The floors ------------------------------------------------------------------------

float UBDPlatformComponent::GetLiftForLevel(const int32 Level) const
{
	// Level 1 is the ground floor: a platform that is merely manned stands where it was
	// authored. The height is the rank of the shooters on it, not the fact of their
	// being there, so it is the storeys ABOVE the first that lift the deck: level 5 is
	// five floors, four of them stacked under the authored one.
	return FMath::Max(0.0f, FloorHeight) * FMath::Max(0, Level - 1);
}

float UBDPlatformComponent::GetDeckLift() const
{
	return GetLiftForLevel(AppliedVisualLevel);
}

void UBDPlatformComponent::CacheAuthoredMeshTransforms()
{
	if (bAuthoredMeshTransformsCached)
	{
		return;
	}
	bAuthoredMeshTransformsCached = true;

	const AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	TArray<UStaticMeshComponent*> Meshes;
	Owner->GetComponents<UStaticMeshComponent>(Meshes);
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		// The storeys are ours and are placed from scratch every time; only the authored
		// meshes are lifted, and only from where they were authored.
		if (Mesh != Floors)
		{
			AuthoredMeshTransforms.Emplace(Mesh, Mesh->GetRelativeTransform());
		}
	}
}

void UBDPlatformComponent::RefreshVisualLevel()
{
	// Never in the editor: building storeys onto a placed actor would dirty the map for
	// something that is only ever true while a match runs.
	const UWorld* World = GetWorld();
	if (World == nullptr || !World->IsGameWorld())
	{
		return;
	}

	const int32 Level = GetBlockLevel();
	if (Level == AppliedVisualLevel)
	{
		return;
	}

	const int32 Previous = AppliedVisualLevel;
	AppliedVisualLevel = Level;
	ApplyVisualLevel(Level);

	// Not on the first pass, which only settles the platform at the height it already has.
	UE_CLOG(Previous >= 0, LogBDGrid, Log, TEXT("%s stands at level %d of %d (was %d): every one of its %d slot(s) reached it, so the deck is %.0f cm up on %d storey(s). Visual only - no range, no damage, no parameter."),
		*GetNameSafe(GetOwner()), Level, UBDTowerData::MaxLevels, Previous, Slots.Num(), GetLiftForLevel(Level), FMath::Max(0, Level - 1));
}

void UBDPlatformComponent::ApplyVisualLevel(const int32 Level)
{
	AActor* Owner = GetOwner();
	if (Owner == nullptr)
	{
		return;
	}

	CacheAuthoredMeshTransforms();

	const float Lift = GetLiftForLevel(Level);
	const int32 Storeys = FMath::Max(0, Level - 1);

	// 1. The deck goes up, from where it was authored rather than from where the last
	//    floor left it, so the height is always the level times the floor and never a
	//    sum of rounding.
	for (const TPair<TWeakObjectPtr<UStaticMeshComponent>, FTransform>& Authored : AuthoredMeshTransforms)
	{
		if (UStaticMeshComponent* Mesh = Authored.Key.Get())
		{
			FTransform Raised = Authored.Value;
			Raised.SetLocation(Authored.Value.GetLocation() + FVector(0.0f, 0.0f, Lift));
			Mesh->SetRelativeTransform(Raised);
		}
	}

	// 2. The storeys fill the space under it, one per floor. One instanced component for
	//    the lot: a platform at level 5 is five instances, not five components.
	UStaticMesh* Storey = FloorMesh.LoadSynchronous();
	if (Storey == nullptr && AuthoredMeshTransforms.Num() > 0)
	{
		const UStaticMeshComponent* First = AuthoredMeshTransforms[0].Key.Get();
		Storey = First != nullptr ? First->GetStaticMesh() : nullptr;
	}

	if (Storeys > 0 && Storey != nullptr)
	{
		if (Floors == nullptr)
		{
			Floors = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("PlatformFloors"));
			Floors->SetupAttachment(Owner->GetRootComponent());
			Floors->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Floors->SetGenerateOverlapEvents(false);
			Floors->SetCastShadow(false);
			Floors->RegisterComponent();
		}
		Floors->SetStaticMesh(Storey);
		Floors->ClearInstances();
		for (int32 Floor = 0; Floor < Storeys; ++Floor)
		{
			const FTransform Instance(FRotator::ZeroRotator, FVector(0.0f, 0.0f, FMath::Max(0.0f, FloorHeight) * Floor), FloorMeshScale);
			Floors->AddInstance(Instance);
		}
	}
	else if (Floors != nullptr)
	{
		Floors->ClearInstances();
	}

	// 3. The shooters ride up with the deck they stand on. Their range is measured on the
	//    flat (DistSquared2D), so standing higher changes nothing they do.
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (ABDTowerBase* Occupant = Slots[Index].Occupant.Get())
		{
			const FTransform SlotTransform = GetSlotWorldTransform(Index);
			Occupant->SetActorLocationAndRotation(SlotTransform.GetLocation(), SlotTransform.GetRotation());
		}
	}

	// 4. And a quieter reading of the same rank, for a board seen from above: white at
	//    level 0, so a platform that has earned nothing looks exactly as authored.
	const float Alpha = FMath::Clamp(static_cast<float>(Level) / FMath::Max(1, UBDTowerData::MaxLevels), 0.0f, 1.0f);
	const FLinearColor Tint = FMath::Lerp(FLinearColor::White, TopLevelTint, Alpha);
	const float Glow = FMath::Max(0.0f, TopLevelGlow) * Alpha;
	for (const TPair<TWeakObjectPtr<UStaticMeshComponent>, FTransform>& Authored : AuthoredMeshTransforms)
	{
		UStaticMeshComponent* Mesh = Authored.Key.Get();
		if (Mesh == nullptr)
		{
			continue;
		}

		for (int32 Index = 0; Index < Mesh->GetNumMaterials(); ++Index)
		{
			if (Mesh->GetMaterial(Index) == nullptr)
			{
				continue;
			}

			// Returns the instance already in place when there is one, so this does not
			// stack a new material every time a floor goes up.
			if (UMaterialInstanceDynamic* Dynamic = Mesh->CreateAndSetMaterialInstanceDynamic(Index))
			{
				Dynamic->SetVectorParameterValue(TEXT("TintColor"), Tint);
				Dynamic->SetScalarParameterValue(TEXT("TintGlow"), Glow);
			}
		}
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

	// The floors under the deck raise the slots with it, so a shooter mounted on a
	// platform at level 3 stands three storeys up. Range is measured on the flat, so
	// nothing it does changes.
	const FBDPlatformSlot& Slot = Slots[SlotIndex];
	const FTransform LocalTransform(Slot.LocalRotation, Slot.LocalOffset + FVector(0.0f, 0.0f, GetDeckLift()));
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

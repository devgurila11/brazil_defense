// Brazil Defense. The gesture of putting a piece on the board and taking it back.

#include "Placement/BDPlacementComponent.h"

#include "BDLog.h"
#include "CollisionQueryParams.h"
#include "Components/InputComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/PlayerController.h"
#include "Grid/BDGridSubsystem.h"
#include "Objective/BDObjectiveSubsystem.h"
#include "Match/BDGameBalanceSettings.h"
#include "Match/BDMatchManager.h"
#include "Path/BDPathfinder.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementPreview.h"
#include "Placement/BDPlacementSettings.h"
#include "Platform/BDPlatformComponent.h"
#include "Objective/BDObjectiveSettings.h"
#include "Save/BDMatchSave.h"
#include "Tower/BDTowerBase.h"
#include "Tower/BDTowerData.h"
#include "UI/BDUISubsystem.h"
#include "Engine/GameInstance.h"
#include "UObject/UObjectIterator.h"
#include "Grid/BDGridSettings.h"

UBDPlacementComponent::UBDPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBDPlacementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwningController() == nullptr)
	{
		UE_LOG(LogBDGrid, Error,
			TEXT("UBDPlacementComponent is on '%s', which is not a player controller. Hovering needs a cursor to project."),
			*GetNameSafe(GetOwner()));
	}
}

void UBDPlacementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Preview != nullptr)
	{
		Preview->Destroy();
		Preview = nullptr;
	}

	if (ABDMatchManager* Match = BoundMatch.Get())
	{
		Match->OnPhaseChanged.Remove(PhaseChangedHandle);
	}
	BoundMatch.Reset();

	Super::EndPlay(EndPlayReason);
}

void UBDPlacementComponent::EnsureMatchBinding()
{
	if (BoundMatch.IsValid())
	{
		return;
	}

	ABDMatchManager* Match = GetMatch();
	if (Match == nullptr)
	{
		return;
	}

	PhaseChangedHandle = Match->OnPhaseChanged.AddUObject(this, &UBDPlacementComponent::HandleMatchPhaseChanged);
	BoundMatch = Match;
}

void UBDPlacementComponent::HandleMatchPhaseChanged(const EBDMatchPhase NewPhase)
{
	if (NewPhase == EBDMatchPhase::Building || CurrentSelection == nullptr)
	{
		return;
	}

	UE_LOG(LogBDGrid, Log, TEXT("Selection of '%s' cleared: the match left the building phase (%s)."),
		*GetNameSafe(CurrentSelection),
		*StaticEnum<EBDMatchPhase>()->GetNameStringByValue(static_cast<int64>(NewPhase)));

	CancelSelection();
}

void UBDPlacementComponent::CancelSelectionIfBudgetExhausted()
{
	const ABDMatchManager* Match = GetMatch();
	if (CurrentSelection == nullptr || Match == nullptr)
	{
		return;
	}

	// Only a counted kind can run out. A defender is dropped by the hand when the money
	// or the board say no, and both of those are answered a frame at a time by the ghost,
	// not by taking the piece away from the player.
	const EBDPieceKind Kind = CurrentSelection->GetPieceKind();
	if (!ABDMatchManager::HasBudgetCeiling(Kind) || Match->GetBudgetRemaining(Kind) > 0)
	{
		return;
	}

	UE_LOG(LogBDGrid, Log, TEXT("Selection of '%s' cleared: no %s left to place."),
		*GetNameSafe(CurrentSelection),
		*StaticEnum<EBDPieceKind>()->GetNameStringByValue(static_cast<int64>(Kind)));

	CancelSelection();
}

void UBDPlacementComponent::BindInput(UInputComponent* InputComponent)
{
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (EnhancedInput == nullptr)
	{
		UE_LOG(LogBDGrid, Error,
			TEXT("Placement input needs a UEnhancedInputComponent. Check the default input component class ")
			TEXT("in Project Settings > Input."));
		return;
	}

	const UBDPlacementSettings& Settings = UBDPlacementSettings::Get();

	// Every action is optional so a half configured project still runs: what is missing
	// simply does nothing.
	if (UInputAction* Action = Settings.PlaceAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, &UBDPlacementComponent::HandlePlaceInput);
		// A move is a drag: press on a placed piece lifts it, release drops it.
		EnhancedInput->BindAction(Action, ETriggerEvent::Completed, this, &UBDPlacementComponent::HandlePlaceReleased);
	}

	if (UInputAction* Action = Settings.RemoveAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, &UBDPlacementComponent::HandleRemoveInput);
	}

	if (UInputAction* Action = Settings.CancelAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Action, ETriggerEvent::Started, this, &UBDPlacementComponent::HandleCancelInput);
	}

	// The rotate action is not bound any more: the mapping context put it on the wheel,
	// which the camera needs for its height. The controller turns the piece with R and
	// with a click of the middle button (a drag of it turns the view instead).

	AddMappingContext();
}

void UBDPlacementComponent::AddMappingContext()
{
	const APlayerController* Controller = GetOwningController();
	if (Controller == nullptr)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());
	if (InputSubsystem == nullptr)
	{
		return;
	}

	const UBDPlacementSettings& Settings = UBDPlacementSettings::Get();
	if (UInputMappingContext* Context = Settings.GameplayMappingContext.LoadSynchronous())
	{
		InputSubsystem->AddMappingContext(Context, Settings.GameplayMappingPriority);
	}
	else
	{
		UE_LOG(LogBDGrid, Error,
			TEXT("No gameplay mapping context configured. Set it in Project Settings > Game > Brazil Defense - Placement."));
	}
}

APlayerController* UBDPlacementComponent::GetOwningController() const
{
	return Cast<APlayerController>(GetOwner());
}

UBDGridSubsystem* UBDPlacementComponent::GetGrid() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
}

ABDMatchManager* UBDPlacementComponent::GetMatch() const
{
	if (!CachedMatch.IsValid())
	{
		CachedMatch = ABDMatchManager::Get(this);
	}

	return CachedMatch.Get();
}

const UBDPathfinder* UBDPlacementComponent::GetPathfinder() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDPathfinder>() : nullptr;
}

UBDObjectiveSubsystem* UBDPlacementComponent::GetObjectives() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDObjectiveSubsystem>() : nullptr;
}

bool UBDPlacementComponent::IsEdgeSelection() const
{
	return CurrentSelection != nullptr && CurrentSelection->bOccupiesEdge;
}

bool UBDPlacementComponent::IsObjectiveSelection() const
{
	return CurrentSelection != nullptr && CurrentSelection->GetPieceKind() == EBDPieceKind::Objective;
}

bool UBDPlacementComponent::IsTowerSelection() const
{
	return CurrentSelection != nullptr && CurrentSelection->IsDefender();
}

UBDPlatformComponent* UBDPlacementComponent::FindPlatformAt(const FBDCellCoord& Coord) const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	// Every platform in the world, placed by the player or authored in the level. There
	// are a handful; walking them per hover frame is cheaper than keeping a registry.
	for (TObjectIterator<UBDPlatformComponent> It; It; ++It)
	{
		UBDPlatformComponent* Platform = *It;
		if (Platform->GetWorld() != World || !Platform->bInsideBattleArea || !IsValid(Platform->GetOwner()))
		{
			continue;
		}

		TArray<FBDCellCoord> Cells;
		Platform->GetFootprintCells(Cells);
		if (Cells.Contains(Coord))
		{
			return Platform;
		}
	}

	return nullptr;
}

int32 UBDPlacementComponent::CountFreeSlots() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return 0;
	}

	int32 Free = 0;
	for (TObjectIterator<UBDPlatformComponent> It; It; ++It)
	{
		const UBDPlatformComponent* Platform = *It;
		if (Platform->GetWorld() != World || !Platform->bInsideBattleArea || !IsValid(Platform->GetOwner()))
		{
			continue;
		}

		for (int32 Slot = 0; Slot < Platform->Slots.Num(); ++Slot)
		{
			Free += Platform->IsSlotFree(Slot) ? 1 : 0;
		}
	}

	return Free;
}

int32 UBDPlacementComponent::FindNearestSlot(const UBDPlatformComponent& Platform, const FVector& Point)
{
	int32 Nearest = INDEX_NONE;
	float NearestDistance = TNumericLimits<float>::Max();

	for (int32 Index = 0; Index < Platform.Slots.Num(); ++Index)
	{
		const float Distance = FVector::DistSquared2D(Platform.GetSlotWorldTransform(Index).GetLocation(), Point);
		if (Distance < NearestDistance)
		{
			Nearest = Index;
			NearestDistance = Distance;
		}
	}

	return Nearest;
}

void UBDPlacementComponent::ResolveSlotHover()
{
	HoveredPlatform.Reset();
	HoveredSlotIndex = INDEX_NONE;

	const UBDGridSubsystem* Grid = GetGrid();
	if (!IsTowerSelection() || Grid == nullptr || !bHoveringGrid || Grid->GetCellState(HoveredCell) != EBDCellState::Platform)
	{
		return;
	}

	UBDPlatformComponent* Platform = FindPlatformAt(HoveredCell);
	if (Platform == nullptr)
	{
		return;
	}

	// The slot nearest the cursor, taken or not: a taken one is refused as SlotTaken,
	// which tells the player more than silently snapping to another slot would.
	HoveredPlatform = Platform;
	HoveredSlotIndex = FindNearestSlot(*Platform, HoverPoint);
}

UClass* UBDPlacementComponent::ResolveActorClass() const
{
	if (CurrentSelection == nullptr)
	{
		return nullptr;
	}

	if (UClass* ActorClass = CurrentSelection->ActorClass.LoadSynchronous())
	{
		return ActorClass;
	}

	if (IsTowerSelection())
	{
		const UBDTowerData* TowerData = CurrentSelection->TowerData.LoadSynchronous();
		if (TowerData != nullptr)
		{
			return TowerData->TowerClass.IsNull() ? ABDTowerBase::StaticClass() : TowerData->TowerClass.LoadSynchronous();
		}
	}

	return nullptr;
}

//~ Selection ------------------------------------------------------------------

EBDPlacementRefusal UBDPlacementComponent::GetHandRefusal(const UBDPlaceableData* Piece) const
{
	if (Piece == nullptr)
	{
		return EBDPlacementRefusal::NoSelection;
	}

	// Without a match every placement is free, as everywhere else in this component.
	const ABDMatchManager* Match = GetMatch();
	if (Match == nullptr)
	{
		return EBDPlacementRefusal::None;
	}

	// Asked in the order the player would hit them, and the money last: running out of
	// public money is the ordinary answer, so the unusual ones have to be able to speak
	// first. A piece still locked says so before anything else: nothing else matters yet.
	const EBDPieceKind Kind = Piece->GetPieceKind();
	if (Match->IsMatchOver())
	{
		return EBDPlacementRefusal::MatchRefused;
	}
	if (!Match->IsUnlocked(Piece))
	{
		return EBDPlacementRefusal::NotUnlocked;
	}
	if (ABDMatchManager::HasBudgetCeiling(Kind) && Match->GetBudgetRemaining(Kind) <= 0)
	{
		return EBDPlacementRefusal::NoBudgetLeft;
	}
	if (Kind == EBDPieceKind::Character && Match->GetFreeCharacterSlots() <= 0)
	{
		return EBDPlacementRefusal::NoFreeSlot;
	}
	if (!Match->CanPlace(Kind))
	{
		return EBDPlacementRefusal::MatchRefused;
	}
	if (!Match->CanAffordPublicMoney(Match->GetBuildPrice(Piece)))
	{
		return EBDPlacementRefusal::NoFunds;
	}
	return EBDPlacementRefusal::None;
}

bool UBDPlacementComponent::TakeIntoHand(UBDPlaceableData* Piece)
{
	const EBDPlacementRefusal Refusal = GetHandRefusal(Piece);
	if (Refusal != EBDPlacementRefusal::None)
	{
		UE_LOG(LogBDGrid, Log, TEXT("'%s' not taken into the hand: %s."), *GetNameSafe(Piece),
			*StaticEnum<EBDPlacementRefusal>()->GetNameStringByValue(static_cast<int64>(Refusal)));
		return false;
	}

	SelectPlaceable(Piece);
	return CurrentSelection == Piece;
}

void UBDPlacementComponent::SelectPlaceable(UBDPlaceableData* Placeable)
{
	FString SetupError;
	if (Placeable != nullptr && !Placeable->IsValidSetup(SetupError))
	{
		UE_LOG(LogBDGrid, Error, TEXT("Placeable '%s' cannot be selected: %s"), *GetNameSafe(Placeable), *SetupError);
		return;
	}

	// Picking a new piece while one is lifted sends the lifted one home first.
	if (bMoving)
	{
		CancelMove();
	}

	CurrentSelection = Placeable;
	RotationSteps = 0;

	if (CurrentSelection == nullptr)
	{
		CancelSelection();
		return;
	}

	// Whoever holds a piece has to hear the phase change, tick or no tick yet.
	EnsureMatchBinding();

	EnsurePreview();
	if (Preview != nullptr)
	{
		Preview->SetPlaceable(CurrentSelection);
	}

	// The hovered edge depends on the direction the fence runs, so it has to be picked
	// again for the new selection before anything is validated.
	if (bHoveringGrid)
	{
		ResolveHover(HoverPoint);
	}

	EvaluatePlacement();
}

FIntPoint UBDPlacementComponent::GetEffectiveFootprint() const
{
	if (CurrentSelection == nullptr || IsEdgeSelection())
	{
		return FIntPoint(1, 1);
	}

	const FIntPoint Base(FMath::Max(1, CurrentSelection->Footprint.X), FMath::Max(1, CurrentSelection->Footprint.Y));
	return IsSelectionRotated() ? FIntPoint(Base.Y, Base.X) : Base;
}

float UBDPlacementComponent::GetPlacementYaw() const
{
	return RotationSteps * DegreesPerRotationStep;
}

uint8 UBDPlacementComponent::GetSegmentDirection() const
{
	// Unrotated, a fence is a run of +Y edges: a line along X. One turn makes it +X edges.
	return IsSelectionRotated() ? FBDEdgeCoord::DirectionX : FBDEdgeCoord::DirectionY;
}

void UBDPlacementComponent::RotateSelection(const bool bClockwise)
{
	if (CurrentSelection == nullptr)
	{
		return;
	}

	const int32 StepCount = IsEdgeSelection() ? EdgeRotationStepCount : CellRotationStepCount;
	RotationSteps = (RotationSteps + (bClockwise ? 1 : StepCount - 1)) % StepCount;

	// A spot that was legal for a 3x1 may well be refused for a 1x3, so the answer has
	// to be recomputed before the player can click. A 1x1 only turns; the validation
	// runs again anyway and lands on the same answer. A fence changes which edges the
	// cursor is choosing between, so the hover is redone as well.
	if (bHoveringGrid)
	{
		ResolveHover(HoverPoint);
	}

	EvaluatePlacement();
}

void UBDPlacementComponent::CancelSelection()
{
	if (bMoving)
	{
		// The lifted piece is not dropped on the floor: it goes home first.
		CancelMove();
		return;
	}

	CurrentSelection = nullptr;
	bCurrentPlacementValid = false;
	CurrentRefusal = EBDPlacementRefusal::NoSelection;
	LastReportedRefusal = EBDPlacementRefusal::NoSelection;

	// The ghost goes with the selection; the next selection spawns a fresh one.
	if (Preview != nullptr)
	{
		Preview->Destroy();
		Preview = nullptr;
	}
}

void UBDPlacementComponent::EnsurePreview()
{
	if (Preview != nullptr)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.ObjectFlags |= RF_Transient;
	Preview = World->SpawnActor<ABDPlacementPreview>(ABDPlacementPreview::StaticClass(), FTransform::Identity, SpawnParams);
}

//~ Hover ----------------------------------------------------------------------

bool UBDPlacementComponent::TraceGridPlane(FVector& OutHitPoint) const
{
	const APlayerController* Controller = GetOwningController();
	const UBDGridSubsystem* Grid = GetGrid();
	if (Controller == nullptr || Grid == nullptr)
	{
		return false;
	}

	FVector RayOrigin;
	FVector RayDirection;
	if (!Controller->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;
	}

	// The board is a plane, so no collision geometry is needed to hover it. That also
	// means the hover works over a hole in the terrain, which a trace would miss.
	const double PlaneZ = Grid->GetOrigin().Z;
	if (FMath::IsNearlyZero(RayDirection.Z))
	{
		return false;
	}

	const double DistanceAlongRay = (PlaneZ - RayOrigin.Z) / RayDirection.Z;
	const UBDPlacementSettings& Settings = UBDPlacementSettings::Get();
	if (DistanceAlongRay <= 0.0 || DistanceAlongRay > Settings.MaxHoverDistance)
	{
		// Behind the camera, or so far away that the camera is looking at the horizon.
		return false;
	}

	OutHitPoint = RayOrigin + RayDirection * DistanceAlongRay;
	return true;
}

FBDEdgeCoord UBDPlacementComponent::PickNearestEdge(const UBDGridSubsystem& Grid, const FBDCellCoord& Cell, const FVector& Point) const
{
	const bool bFilterDirection = IsEdgeSelection();
	const uint8 WantedDirection = GetSegmentDirection();

	// The four neighbours, each giving one side of the cell.
	static constexpr int32 SideOffsetX[] = { 1, -1, 0, 0 };
	static constexpr int32 SideOffsetY[] = { 0, 0, 1, -1 };

	FBDEdgeCoord Nearest;
	float NearestDistanceSquared = TNumericLimits<float>::Max();

	for (int32 Side = 0; Side < UE_ARRAY_COUNT(SideOffsetX); ++Side)
	{
		bool bAdjacent = false;
		const FBDEdgeCoord Edge = FBDEdgeCoord::Between(Cell,
			FBDCellCoord(Cell.X + SideOffsetX[Side], Cell.Y + SideOffsetY[Side]), bAdjacent);

		if (bFilterDirection && Edge.Direction != WantedDirection)
		{
			continue;
		}

		// Height is irrelevant: the cursor point sits on the grid plane, and so does the edge.
		const float DistanceSquared = FVector::DistSquared2D(Grid.EdgeToWorld(Edge), Point);
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			Nearest = Edge;
		}
	}

	return Nearest;
}

void UBDPlacementComponent::ResolveHover(const FVector& PlanePoint)
{
	const UBDGridSubsystem* Grid = GetGrid();
	FBDCellCoord Cell;
	if (Grid == nullptr || !Grid->WorldToCell(PlanePoint, Cell))
	{
		bHoveringGrid = false;
		return;
	}

	HoverPoint = PlanePoint;
	HoveredCell = Cell;
	HoveredEdge = PickNearestEdge(*Grid, Cell, PlanePoint);
	bHoveringGrid = true;
}

void UBDPlacementComponent::SetHoveredCellDirect(const FBDCellCoord Coord)
{
	if (const UBDGridSubsystem* Grid = GetGrid())
	{
		ResolveHover(Grid->CellToWorld(Coord));
	}

	// Outside the grid the hover still lands on the cell, so a console test of an off
	// board placement gets a refusal rather than a silent no-op.
	HoveredCell = Coord;
	bHoveringGrid = true;
	EvaluatePlacement();
}

void UBDPlacementComponent::SetHoveredSlotDirect(UBDPlatformComponent* Platform, const int32 SlotIndex)
{
	const UBDGridSubsystem* Grid = GetGrid();
	if (Platform == nullptr || Grid == nullptr || !Platform->Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	// The slot's own location as the cursor point, so the nearest slot is that slot; the
	// cell under it is what the slot lookup starts from.
	HoverPoint = Platform->GetSlotWorldTransform(SlotIndex).GetLocation();
	FBDCellCoord Cell;
	if (!Grid->WorldToCell(HoverPoint, Cell))
	{
		TArray<FBDCellCoord> Cells;
		Platform->GetFootprintCells(Cells);
		if (Cells.Num() == 0)
		{
			return;
		}
		Cell = Cells[0];
	}

	HoveredCell = Cell;
	bHoveringGrid = true;
	EvaluatePlacement();
}

void UBDPlacementComponent::SetRotationSteps(const int32 Steps)
{
	if (CurrentSelection == nullptr)
	{
		return;
	}

	const int32 StepCount = IsEdgeSelection() ? EdgeRotationStepCount : CellRotationStepCount;
	RotationSteps = ((Steps % StepCount) + StepCount) % StepCount;

	if (bHoveringGrid)
	{
		ResolveHover(HoverPoint);
	}
	EvaluatePlacement();
}

void UBDPlacementComponent::DebugRemoveAll()
{
	UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr)
	{
		return;
	}

	if (bMoving)
	{
		CancelMove();
	}

	ABDMatchManager* Match = GetMatch();
	int32 Removed = 0;

	// Slot pieces first, so a platform does not take them down without their own refund.
	while (PlacedOnSlots.Num() > 0)
	{
		const FBDPlacedPiece Piece = PlacedOnSlots[0];
		ForgetPiece(*Grid, Piece);
		if (Match != nullptr && Piece.Data != nullptr)
		{
			Match->RefundRemoval(Piece.Data->GetPieceKind());
		}
		++Removed;
	}

	while (PlacedByCell.Num() > 0)
	{
		const FBDPlacedPiece Piece = PlacedByCell.CreateConstIterator()->Value;
		ForgetPiece(*Grid, Piece);
		if (Match != nullptr && Piece.Data != nullptr)
		{
			Match->RefundRemoval(Piece.Data->GetPieceKind());
		}
		++Removed;
	}

	while (PlacedByEdge.Num() > 0)
	{
		const FBDPlacedPiece Piece = PlacedByEdge.CreateConstIterator()->Value;
		ForgetPiece(*Grid, Piece);
		if (Match != nullptr && Piece.Data != nullptr)
		{
			Match->RefundRemoval(Piece.Data->GetPieceKind());
		}
		++Removed;
	}

	CancelSelection();
	UE_LOG(LogBDGrid, Log, TEXT("Every placed piece removed: %d piece(s)."), Removed);
}

//~ Saving --------------------------------------------------------------------------

void UBDPlacementComponent::CaptureBoard(TArray<FBDSavedPiece>& OutPieces) const
{
	OutPieces.Reset();

	// The urn is not a piece of the maps: it lives in the objective subsystem. It goes
	// first because nothing else may be placed before it.
	const UBDObjectiveSubsystem* Objectives = GetObjectives();
	if (Objectives != nullptr && Objectives->IsPlaced())
	{
		FBDSavedPiece& Urn = OutPieces.AddDefaulted_GetRef();
		Urn.Data = UBDObjectiveSettings::Get().ObjectivePlaceable.ToSoftObjectPath();
		Urn.Origin = Objectives->GetGoalCell();
	}

	const auto Write = [&OutPieces](const FBDPlacedPiece& Piece, const FBDCellCoord& PlatformCell)
	{
		if (Piece.Data == nullptr || Piece.Data->HasAnyFlags(RF_Transient) || !FSoftObjectPath(Piece.Data).IsValid())
		{
			UE_LOG(LogBDGrid, Warning, TEXT("Piece '%s' at %s has no asset behind it and is not saved."),
				*GetNameSafe(Piece.Data), *Piece.Origin.ToString());
			return;
		}

		FBDSavedPiece& Saved = OutPieces.AddDefaulted_GetRef();
		Saved.Data = FSoftObjectPath(Piece.Data);
		Saved.Edges = Piece.Edges;
		Saved.SlotIndex = Piece.SlotIndex;
		Saved.PaidCost = Piece.PaidCost;
		if (Piece.IsOnSlot())
		{
			Saved.Origin = PlatformCell;
		}
		else
		{
			Saved.Origin = Piece.Origin;
			// The same recovery TryBeginMoveAtHovered does: a fence's turn is its direction,
			// a cell piece's turn is in its yaw.
			Saved.RotationSteps = Piece.Edges.Num() > 0
				? (Piece.Edges[0].Direction == FBDEdgeCoord::DirectionX ? 1 : 0)
				: FMath::RoundToInt(Piece.Yaw / DegreesPerRotationStep) % CellRotationStepCount;
		}

		const ABDTowerBase* Tower = Piece.Actors.Num() > 0 ? Cast<ABDTowerBase>(Piece.Actors[0]) : nullptr;
		Saved.Level = Tower != nullptr ? Tower->GetTowerLevel() : 1;
		Saved.EvolutionSpent = Tower != nullptr ? Tower->GetEvolutionSpent() : 0;
	};

	// A wide piece is in the map once per cell it covers; its first actor tells the copies apart.
	TSet<const AActor*> Seen;
	for (const TPair<FBDCellCoord, FBDPlacedPiece>& Pair : PlacedByCell)
	{
		const AActor* Key = Pair.Value.Actors.Num() > 0 ? Pair.Value.Actors[0] : nullptr;
		if (Key == nullptr || Seen.Contains(Key))
		{
			continue;
		}
		Seen.Add(Key);
		Write(Pair.Value, FBDCellCoord());
	}

	for (const TPair<FBDEdgeCoord, FBDPlacedPiece>& Pair : PlacedByEdge)
	{
		const AActor* Key = Pair.Value.Actors.Num() > 0 ? Pair.Value.Actors[0] : nullptr;
		if (Key == nullptr || Seen.Contains(Key))
		{
			continue;
		}
		Seen.Add(Key);
		Write(Pair.Value, FBDCellCoord());
	}

	// Mounted pieces come last so their platform is back before they are. The platform
	// is found by its owner among the cell pieces.
	for (const FBDPlacedPiece& Piece : PlacedOnSlots)
	{
		const UBDPlatformComponent* Platform = Piece.Platform.Get();
		const FBDPlacedPiece* PlatformPiece = Platform != nullptr ? FindPieceOfActor(Platform->GetOwner()) : nullptr;
		if (PlatformPiece == nullptr)
		{
			UE_LOG(LogBDGrid, Warning, TEXT("Mounted piece '%s' has no platform on the board and is not saved."), *GetNameSafe(Piece.Data));
			continue;
		}
		Write(Piece, PlatformPiece->Origin);
	}
}

int32 UBDPlacementComponent::RestoreBoard(const TArray<FBDSavedPiece>& Pieces)
{
	if (bMoving)
	{
		CancelMove();
	}

	const bool bWasLogging = bRefusalLogging;
	bRefusalLogging = false;
	// Restoring is not building: these pieces were bought in the match that was saved, and
	// the balance they left behind comes back from the save further up. Asked to pay again
	// out of the opening capital, a late board would lose whatever that capital no longer
	// covers - and nothing caps how big a board gets any more.
	TGuardValue<bool> RestoringGuard(bRestoring, true);
	int32 Restored = 0;

	for (const FBDSavedPiece& Saved : Pieces)
	{
		UBDPlaceableData* Data = Cast<UBDPlaceableData>(Saved.Data.TryLoad());
		if (Data == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Saved piece '%s' could not be loaded; skipped."), *Saved.Data.ToString());
			continue;
		}

		SelectPlaceable(Data);
		if (CurrentSelection != Data)
		{
			continue;
		}

		// A save from before prices were kept has none: it takes the opening price, which
		// is what a restored match would charge for it on its first building phase.
		const ABDMatchManager* Match = GetMatch();
		RestoringPaidCost = Saved.PaidCost >= 0 ? Saved.PaidCost : (Match != nullptr ? Match->GetBuildPriceOnWave(Data, 1) : 0);

		if (Saved.IsOnSlot())
		{
			const FBDPlacedPiece* PlatformPiece = PlacedByCell.Find(Saved.Origin);
			AActor* PlatformActor = PlatformPiece != nullptr && PlatformPiece->Actors.Num() > 0 ? PlatformPiece->Actors[0] : nullptr;
			UBDPlatformComponent* Platform = PlatformActor != nullptr ? PlatformActor->FindComponentByClass<UBDPlatformComponent>() : nullptr;
			if (Platform == nullptr)
			{
				UE_LOG(LogBDGrid, Error, TEXT("Saved '%s' wants slot %d of a platform at %s, but no platform is there; skipped."),
					*Data->GetName(), Saved.SlotIndex, *Saved.Origin.ToString());
				continue;
			}
			SetHoveredSlotDirect(Platform, Saved.SlotIndex);
		}
		else if (Saved.IsOnEdge())
		{
			SetHoveredEdgeDirect(Saved.Edges[0]);
		}
		else
		{
			SetRotationSteps(Saved.RotationSteps);
			SetHoveredCellDirect(Saved.Origin);
		}

		if (!TryPlaceAtHovered())
		{
			UE_LOG(LogBDGrid, Error, TEXT("Saved '%s' at %s refused on restore: %s."),
				*Data->GetName(), Saved.IsOnEdge() ? *Saved.Edges[0].ToString() : *Saved.Origin.ToString(), *DescribeCurrentRefusal());
			continue;
		}
		++Restored;

		// The level goes straight on: the upgrades were paid for in the saved match.
		if (Saved.Level > 1)
		{
			const FBDPlacedPiece* Placed = Saved.IsOnSlot()
				? (PlacedOnSlots.Num() > 0 ? &PlacedOnSlots.Last() : nullptr)
				: PlacedByCell.Find(Saved.Origin);
			ABDTowerBase* Tower = Placed != nullptr && Placed->Actors.Num() > 0 ? Cast<ABDTowerBase>(Placed->Actors[0]) : nullptr;
			if (Tower != nullptr)
			{
				// A save from before the spend was kept takes the wave 1 prices of its levels.
				const UBDTowerData* TowerData = Tower->GetData();
				const int32 Spent = Saved.EvolutionSpent >= 0 || TowerData == nullptr
					? Saved.EvolutionSpent
					: UBDGameBalanceSettings::Get().GetEvolutionSpent(TowerData->UpgradeCostBase, Saved.Level);
				Tower->RestoreEvolution(Saved.Level, Spent);
			}
		}
	}

	CancelSelection();
	bRefusalLogging = bWasLogging;

	UE_LOG(LogBDGrid, Log, TEXT("Board restored: %d of %d saved piece(s) back."), Restored, Pieces.Num());
	return Restored;
}

void UBDPlacementComponent::SetHoveredEdgeDirect(const FBDEdgeCoord Edge)
{
	if (const UBDGridSubsystem* Grid = GetGrid())
	{
		HoverPoint = Grid->EdgeToWorld(Edge);
	}

	HoveredCell = Edge.Cell;
	HoveredEdge = Edge;
	bHoveringGrid = true;
	EvaluatePlacement();
}

void UBDPlacementComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	EnsureMatchBinding();

	const FBDCellCoord PreviousCell = HoveredCell;
	const bool bWasHovering = bHoveringGrid;
	const bool bWasValid = bCurrentPlacementValid;

	FVector PlanePoint;
	if (TraceGridPlane(PlanePoint))
	{
		ResolveHover(PlanePoint);
	}
	else
	{
		bHoveringGrid = false;
	}

	EvaluatePlacement();

	if (bHoveringGrid != bWasHovering || HoveredCell != PreviousCell || bCurrentPlacementValid != bWasValid)
	{
		OnHoverChanged.Broadcast(HoveredCell, bCurrentPlacementValid);
	}

	// With the urn in hand the zone it may go in is painted on the floor, hovering or
	// not: without it the player would find the zone by trial and error.
	if (IsObjectiveSelection())
	{
		if (const UBDObjectiveSubsystem* Objectives = GetObjectives())
		{
			Objectives->DrawZone();
		}
	}
}

//~ Validation -----------------------------------------------------------------

void UBDPlacementComponent::GetFootprintCells(const FBDCellCoord Origin, TArray<FBDCellCoord>& OutCells) const
{
	OutCells.Reset();
	if (CurrentSelection == nullptr || IsEdgeSelection())
	{
		return;
	}

	const FIntPoint Span = GetEffectiveFootprint();
	OutCells.Reserve(Span.X * Span.Y);

	for (int32 Y = 0; Y < Span.Y; ++Y)
	{
		for (int32 X = 0; X < Span.X; ++X)
		{
			OutCells.Add(FBDCellCoord(Origin.X + X, Origin.Y + Y));
		}
	}
}

void UBDPlacementComponent::GetSelectionSegmentEdges(TArray<FBDEdgeCoord>& OutEdges) const
{
	OutEdges.Reset();

	const UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr || !IsEdgeSelection())
	{
		return;
	}

	OutEdges = Grid->GetEdgesForSegment(HoveredEdge.Cell, CurrentSelection->SegmentLength, HoveredEdge.Direction);
}

EBDPlacementRefusal UBDPlacementComponent::EvaluateObjectivePlacement() const
{
	const UBDObjectiveSubsystem* Objectives = GetObjectives();
	if (Objectives == nullptr)
	{
		return EBDPlacementRefusal::NotHoveringGrid;
	}

	switch (Objectives->EvaluateCell(HoveredCell))
	{
	case EBDObjectiveRefusal::None:
		return EBDPlacementRefusal::None;

	case EBDObjectiveRefusal::OutOfZone:
		return EBDPlacementRefusal::ObjectiveOutOfZone;

	case EBDObjectiveRefusal::CellTaken:
		return EBDPlacementRefusal::CellTaken;

	case EBDObjectiveRefusal::Unreachable:
		return EBDPlacementRefusal::WouldBlockPath;

	default:
		return EBDPlacementRefusal::OffGrid;
	}
}

bool UBDPlacementComponent::CanSelectionStandOnGround() const
{
	const UBDTowerData* TowerData = IsTowerSelection() ? CurrentSelection->TowerData.LoadSynchronous() : nullptr;
	// A tower piece with no data is treated as ground equipment: that is what a plain ABDTowerBase is.
	return TowerData == nullptr || TowerData->bCanPlaceOnGround;
}

bool UBDPlacementComponent::CanSelectionStandOnSlot() const
{
	const UBDTowerData* TowerData = IsTowerSelection() ? CurrentSelection->TowerData.LoadSynchronous() : nullptr;
	return TowerData != nullptr && TowerData->bCanPlaceOnSlot;
}

EBDPlacementRefusal UBDPlacementComponent::EvaluateSlotPlacement() const
{
	const UBDPlatformComponent* Platform = HoveredPlatform.Get();
	if (Platform == nullptr || HoveredSlotIndex == INDEX_NONE)
	{
		// A platform with no slots at all is just a taken cell.
		return EBDPlacementRefusal::CellTaken;
	}

	if (!CanSelectionStandOnSlot())
	{
		return EBDPlacementRefusal::TowerCannotGoOnSlot;
	}

	return Platform->IsSlotFree(HoveredSlotIndex) ? EBDPlacementRefusal::None : EBDPlacementRefusal::SlotTaken;
}

EBDPlacementRefusal UBDPlacementComponent::EvaluateCellPlacement(const UBDGridSubsystem& Grid) const
{
	TArray<FBDCellCoord> FootprintCells;
	GetFootprintCells(HoveredCell, FootprintCells);

	for (const FBDCellCoord& Coord : FootprintCells)
	{
		// The whole footprint has to fit, so a piece hanging off the edge is refused
		// rather than silently clipped.
		if (!Grid.IsValidCoord(Coord))
		{
			return EBDPlacementRefusal::OffGrid;
		}

		if (!Grid.IsBuildable(Coord))
		{
			return EBDPlacementRefusal::CellTaken;
		}
	}

	// Only a piece that obstructs can cut the creeps off, so a tower skips the search
	// entirely. For the rest this runs every frame, which is exactly what the
	// pathfinder cache is for.
	const UBDPathfinder* Pathfinder = GetPathfinder();
	const bool bWouldBlock = CurrentSelection->BlocksMovement()
		&& Pathfinder != nullptr
		&& Pathfinder->WouldBlockPath(&Grid, HoveredCell, GetEffectiveFootprint());

	return bWouldBlock ? EBDPlacementRefusal::WouldBlockPath : EBDPlacementRefusal::None;
}

EBDPlacementRefusal UBDPlacementComponent::EvaluateEdgePlacement(const UBDGridSubsystem& Grid) const
{
	TArray<FBDEdgeCoord> Edges;
	GetSelectionSegmentEdges(Edges);
	if (Edges.Num() == 0)
	{
		return EBDPlacementRefusal::OffGrid;
	}

	for (const FBDEdgeCoord& Edge : Edges)
	{
		// Every edge of the run has to be fenceable and still free. A run that would
		// double up on a fence already standing is refused whole, like a footprint
		// hanging off the board.
		if (!Grid.IsValidCoord(Edge.Cell))
		{
			return EBDPlacementRefusal::OffGrid;
		}

		if (!Grid.CanBlockEdge(Edge))
		{
			return EBDPlacementRefusal::EdgeOnBorder;
		}

		if (Grid.IsEdgeBlockedAt(Edge))
		{
			return EBDPlacementRefusal::EdgeTaken;
		}
	}

	const UBDPathfinder* Pathfinder = GetPathfinder();
	const bool bWouldBlock = Pathfinder != nullptr && Pathfinder->WouldBlockPathEdges(&Grid, Edges);
	return bWouldBlock ? EBDPlacementRefusal::WouldBlockPath : EBDPlacementRefusal::None;
}

FString UBDPlacementComponent::DescribeCurrentRefusal() const
{
	FString Text = StaticEnum<EBDPlacementRefusal>()->GetNameStringByValue(static_cast<int64>(CurrentRefusal));

	if (CurrentRefusal == EBDPlacementRefusal::NoBudgetLeft && CurrentSelection != nullptr)
	{
		Text += FString::Printf(TEXT(" (no %s left)"),
			*StaticEnum<EBDPieceKind>()->GetNameStringByValue(static_cast<int64>(CurrentSelection->GetPieceKind())));
	}

	// The match is the one refusal with a cause outside this component, so it says why.
	if (CurrentRefusal == EBDPlacementRefusal::MatchRefused)
	{
		if (const ABDMatchManager* Match = GetMatch())
		{
			Text += FString::Printf(TEXT(" (phase %s, wave %d%s)"),
				*StaticEnum<EBDMatchPhase>()->GetNameStringByValue(static_cast<int64>(Match->GetPhase())),
				Match->GetCurrentWave(),
				Match->IsBuildLocked() ? TEXT(", build locked") : TEXT(""));
		}
	}

	return Text;
}

void UBDPlacementComponent::ReportRefusalChange()
{
	if (CurrentRefusal == LastReportedRefusal || !bRefusalLogging)
	{
		return;
	}

	LastReportedRefusal = CurrentRefusal;

	// One line per change of answer, not per frame: a hover across the board that stays
	// refused for the same reason says so once.
	// A move says what it will cost and what it leaves, so the drop is decided knowing.
	FString CostText;
	if (bMoving)
	{
		const ABDMatchManager* Match = GetMatch();
		const int32 Cost = GetMoveCost();
		const int32 Money = Match != nullptr ? Match->GetPublicMoney() : 0;
		CostText = IsHoveringMoveOrigin()
			? TEXT(" (back where it was: no charge)")
			: FString::Printf(TEXT(" (move tax %d public money at %.0f%%: %d -> %d)"),
				Cost, Match != nullptr ? Match->GetMoveTaxRate() * 100.0f : 0.0f, Money, Money - Cost);
	}

	const TCHAR* Verb = bMoving ? TEXT("Move") : TEXT("Placement");

	if (CurrentRefusal == EBDPlacementRefusal::None)
	{
		UE_LOG(LogBDGrid, Log, TEXT("%s of '%s': valid at %s%s%s."),
			Verb, *GetNameSafe(CurrentSelection),
			IsEdgeSelection() ? *HoveredEdge.ToString() : *HoveredCell.ToString(),
			IsHoveringSlot() ? *FString::Printf(TEXT(" slot %d of %s"), HoveredSlotIndex, *GetNameSafe(HoveredPlatform->GetOwner())) : TEXT(""),
			*CostText);
		return;
	}

	UE_LOG(LogBDGrid, Log, TEXT("%s of '%s' refused at %s%s: %s%s."),
		Verb, *GetNameSafe(CurrentSelection),
		bHoveringGrid ? (IsEdgeSelection() ? *HoveredEdge.ToString() : *HoveredCell.ToString()) : TEXT("no cell"),
		IsHoveringSlot() ? *FString::Printf(TEXT(" slot %d"), HoveredSlotIndex) : TEXT(""),
		*DescribeCurrentRefusal(), *CostText);
}

void UBDPlacementComponent::EvaluatePlacement()
{
	bCurrentPlacementValid = false;
	ResolveSlotHover();

	const UBDGridSubsystem* Grid = GetGrid();
	if (CurrentSelection == nullptr)
	{
		CurrentRefusal = EBDPlacementRefusal::NoSelection;
	}
	else if (Grid == nullptr || !bHoveringGrid)
	{
		CurrentRefusal = EBDPlacementRefusal::NotHoveringGrid;
	}
	else
	{
		// Nothing is built before the urn: the maze is built around it, so it has to be
		// there first. Checked ahead of the budget so the reason names the urn.
		const UBDObjectiveSubsystem* Objectives = GetObjectives();
		const bool bObjectiveMissing = !IsObjectiveSelection() && Objectives != nullptr && !Objectives->IsPlaced();

		// A piece the player cannot afford, or cannot place in this phase, is refused
		// before the pathfinding: the budget is cheaper to check than the board. A lifted
		// piece is already paid for and already counted, so it asks the match a different
		// question: may things move right now, and can the tax be paid.
		const ABDMatchManager* Match = GetMatch();
		if (bMoving && Match != nullptr && !Match->CanMove())
		{
			CurrentRefusal = EBDPlacementRefusal::MatchRefused;
		}
		else if (bMoving && Match != nullptr && !IsHoveringMoveOrigin() && !Match->CanAffordPublicMoney(GetMoveCost()))
		{
			CurrentRefusal = EBDPlacementRefusal::CannotAffordMove;
		}
		else if (!bMoving && bObjectiveMissing)
		{
			CurrentRefusal = EBDPlacementRefusal::ObjectiveMissing;
		}
		else if (!bMoving && !bRestoring && Match != nullptr && !Match->IsUnlocked(CurrentSelection))
		{
			// Not in the hand yet. A restore puts back what the saved match had already
			// unlocked, on a board rewound to wave 0, so it is not asked.
			CurrentRefusal = EBDPlacementRefusal::NotUnlocked;
		}
		else if (!bMoving && Match != nullptr && ABDMatchManager::HasBudgetCeiling(CurrentSelection->GetPieceKind())
			&& Match->GetBudgetRemaining(CurrentSelection->GetPieceKind()) <= 0)
		{
			// Out of pieces is not a phase problem, and it must not read like one. Only a
			// counted kind can be out: a defender is never "none left".
			CurrentRefusal = EBDPlacementRefusal::NoBudgetLeft;
		}
		else if (!bMoving && Match != nullptr && CurrentSelection->GetPieceKind() == EBDPieceKind::Character
			&& Match->GetFreeCharacterSlots() <= 0)
		{
			// Nor is a full board. The player needs another platform, not another phase.
			CurrentRefusal = EBDPlacementRefusal::NoFreeSlot;
		}
		else if (!bMoving && Match != nullptr && !Match->CanPlace(CurrentSelection->GetPieceKind()))
		{
			CurrentRefusal = EBDPlacementRefusal::MatchRefused;
		}
		else if (!bMoving && !bRestoring && Match != nullptr && !Match->CanAffordPublicMoney(Match->GetBuildPrice(CurrentSelection)))
		{
			// Paid for in public money, like everything else: the ghost goes red before
			// the click, so the player is never surprised by a refusal. A restore is not
			// asked to pay: that board was bought in the match that was saved.
			CurrentRefusal = EBDPlacementRefusal::NoFunds;
		}
		else if (IsObjectiveSelection())
		{
			CurrentRefusal = EvaluateObjectivePlacement();
		}
		else if (IsHoveringSlot())
		{
			CurrentRefusal = EvaluateSlotPlacement();
		}
		else if (IsTowerSelection() && !CanSelectionStandOnGround())
		{
			CurrentRefusal = EBDPlacementRefusal::CharacterNeedsPlatform;
		}
		else
		{
			CurrentRefusal = IsEdgeSelection() ? EvaluateEdgePlacement(*Grid) : EvaluateCellPlacement(*Grid);
		}
	}

	bCurrentPlacementValid = CurrentRefusal == EBDPlacementRefusal::None;

	// Silent while nothing is held: an idle cursor is not a refusal worth a line.
	if (CurrentSelection != nullptr)
	{
		ReportRefusalChange();
	}

	if (CurrentSelection == nullptr || Grid == nullptr || !bHoveringGrid)
	{
		if (Preview != nullptr)
		{
			Preview->HidePreview();
		}
		return;
	}

	if (Preview != nullptr)
	{
		TArray<FTransform> InstanceTransforms;
		BuildInstanceTransforms(InstanceTransforms);

		FVector OutlineCenter;
		FVector OutlineExtent;
		ComputeOutline(OutlineCenter, OutlineExtent);

		Preview->UpdatePlacement(OutlineCenter, OutlineExtent, InstanceTransforms, bCurrentPlacementValid);
	}
}

//~ Where things go ------------------------------------------------------------

float UBDPlacementComponent::ResolveGroundZ(const FVector& Point) const
{
	const UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = GetGrid();
	const float PlaneZ = Grid != nullptr ? Grid->GetOrigin().Z : Point.Z;
	if (World == nullptr)
	{
		return PlaneZ;
	}

	const UBDPlacementSettings& Settings = UBDPlacementSettings::Get();
	const FVector Start(Point.X, Point.Y, PlaneZ + Settings.GroundTraceDistance);
	const FVector End(Point.X, Point.Y, PlaneZ - Settings.GroundTraceDistance);

	// The ghost has no collision, but the pieces already standing do, and a piece must
	// never be measured against the roof of its neighbour.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BDPlacementGround), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(GetOwner());
	Params.AddIgnoredActor(Preview);
	for (const TPair<FBDCellCoord, FBDPlacedPiece>& Entry : PlacedByCell)
	{
		Params.AddIgnoredActors(Entry.Value.Actors);
	}
	for (const TPair<FBDEdgeCoord, FBDPlacedPiece>& Entry : PlacedByEdge)
	{
		Params.AddIgnoredActors(Entry.Value.Actors);
	}
	for (const FBDPlacedPiece& Piece : PlacedOnSlots)
	{
		Params.AddIgnoredActors(Piece.Actors);
	}

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, Settings.GroundTraceChannel, Params))
	{
		return Hit.ImpactPoint.Z;
	}

	return PlaneZ;
}

void UBDPlacementComponent::BuildInstanceTransforms(TArray<FTransform>& OutTransforms) const
{
	OutTransforms.Reset();

	const UBDGridSubsystem* Grid = GetGrid();
	if (CurrentSelection == nullptr || Grid == nullptr)
	{
		return;
	}

	const float CellSize = Grid->GetCellSize();

	if (IsHoveringSlot())
	{
		// The slot says where and which way; the platform already stands on its floor.
		OutTransforms.Add(HoveredPlatform->GetSlotWorldTransform(HoveredSlotIndex));
		return;
	}

	if (IsEdgeSelection())
	{
		// Every edge of the run gets its copies, laid out around the middle of that edge.
		TArray<FBDEdgeCoord> Edges;
		GetSelectionSegmentEdges(Edges);

		TArray<FTransform> PerEdge;
		for (const FBDEdgeCoord& Edge : Edges)
		{
			const FVector EdgeCenter = Grid->EdgeToWorld(Edge);
			CurrentSelection->GetEdgeInstanceTransforms(Edge.Direction, CellSize, PerEdge);
			for (const FTransform& Relative : PerEdge)
			{
				OutTransforms.Emplace(Relative.GetRotation(), EdgeCenter + Relative.GetLocation());
			}
		}
	}
	else
	{
		const FIntPoint Span = GetEffectiveFootprint();
		const FVector FootprintCenter = Grid->CellCornerToWorld(HoveredCell) + FVector(
			Span.X * CellSize * 0.5f,
			Span.Y * CellSize * 0.5f,
			0.0f);

		OutTransforms.Emplace(FRotator(0.0f, GetPlacementYaw(), 0.0f), FootprintCenter);
	}

	// Every actor finds its own floor, so a fence across a kerb does not float on one
	// side or sink on the other.
	for (FTransform& Transform : OutTransforms)
	{
		FVector Location = Transform.GetLocation();
		Location.Z = ResolveGroundZ(Location);
		Transform.SetLocation(Location);
	}
}

void UBDPlacementComponent::ComputeOutline(FVector& OutCenter, FVector& OutExtent) const
{
	OutCenter = FVector::ZeroVector;
	OutExtent = FVector::ZeroVector;

	const UBDGridSubsystem* Grid = GetGrid();
	if (CurrentSelection == nullptr || Grid == nullptr)
	{
		return;
	}

	const UBDPlacementSettings& Settings = UBDPlacementSettings::Get();
	const float CellSize = Grid->GetCellSize();

	if (IsHoveringSlot())
	{
		// The slot itself is what gets highlighted, at the size the debug slot markers use.
		const float Radius = UBDGridSettings::Get().SlotMarkerRadius;
		OutCenter = HoveredPlatform->GetSlotWorldTransform(HoveredSlotIndex).GetLocation();
		OutExtent = FVector(Radius, Radius, Settings.OutlineHeight * 0.5f);
		OutCenter.Z += OutExtent.Z;
		return;
	}

	if (IsEdgeSelection())
	{
		// A strip along the whole run, from the start of the first edge to the end of
		// the last, as wide as the settings say a fence outline is.
		TArray<FBDEdgeCoord> Edges;
		GetSelectionSegmentEdges(Edges);
		if (Edges.Num() == 0)
		{
			return;
		}

		FVector FirstStart;
		FVector FirstEnd;
		FVector LastStart;
		FVector LastEnd;
		Grid->EdgeEndpointsToWorld(Edges[0], FirstStart, FirstEnd);
		Grid->EdgeEndpointsToWorld(Edges.Last(), LastStart, LastEnd);

		OutCenter = (FirstStart + LastEnd) * 0.5f;

		const float HalfLength = Edges.Num() * CellSize * 0.5f;
		const float HalfWidth = Settings.EdgeOutlineWidth * 0.5f;
		OutExtent = Edges[0].Direction == FBDEdgeCoord::DirectionX
			? FVector(HalfWidth, HalfLength, 0.0f)
			: FVector(HalfLength, HalfWidth, 0.0f);
	}
	else
	{
		const FIntPoint Span = GetEffectiveFootprint();
		const FVector Corner = Grid->CellCornerToWorld(HoveredCell);
		OutExtent = FVector(Span.X * CellSize * 0.5f, Span.Y * CellSize * 0.5f, 0.0f);
		OutCenter = Corner + OutExtent;
	}

	// The outline stands on the floor of its middle, so it is never buried under the
	// pavement the way a box on the grid plane would be.
	OutCenter.Z = ResolveGroundZ(OutCenter);
	OutExtent.Z = Settings.OutlineHeight * 0.5f;
	OutCenter.Z += OutExtent.Z;
}

//~ Place and remove -----------------------------------------------------------

void UBDPlacementComponent::SpawnPieceActors(const TArray<FTransform>& Transforms, FBDPlacedPiece& Piece) const
{
	UClass* ActorClass = ResolveActorClass();
	if (ActorClass == nullptr)
	{
		return;
	}

	// A lifted piece brings its actors with it: they are moved, not remade, so a tower
	// keeps its level and its tally across the move.
	if (Piece.Actors.Num() > 0)
	{
		for (int32 Index = 0; Index < Piece.Actors.Num(); ++Index)
		{
			AActor* Actor = Piece.Actors[Index];
			if (Actor == nullptr)
			{
				continue;
			}

			if (Transforms.IsValidIndex(Index))
			{
				Actor->SetActorTransform(Transforms[Index]);
			}
		}
		SetActorsHidden(Piece.Actors, false);
		return;
	}

	const UBDTowerData* TowerData = IsTowerSelection() ? CurrentSelection->TowerData.LoadSynchronous() : nullptr;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Piece.Actors.Reserve(Transforms.Num());

	for (const FTransform& SpawnTransform : Transforms)
	{
		AActor* Spawned = GetWorld()->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);
		if (Spawned == nullptr)
		{
			continue;
		}

		Piece.Actors.Add(Spawned);

		// A tower is told what it is before its first tick, ground or slot alike.
		if (ABDTowerBase* Tower = Cast<ABDTowerBase>(Spawned))
		{
			Tower->InitializeTower(TowerData);
		}
	}
}

bool UBDPlacementComponent::PlaceSlotPiece(FBDPlacedPiece& Piece)
{
	UBDPlatformComponent* Platform = HoveredPlatform.Get();
	if (Platform == nullptr || HoveredSlotIndex == INDEX_NONE)
	{
		return false;
	}

	TArray<FTransform> InstanceTransforms;
	BuildInstanceTransforms(InstanceTransforms);
	SpawnPieceActors(InstanceTransforms, Piece);

	ABDTowerBase* Tower = Piece.Actors.Num() > 0 ? Cast<ABDTowerBase>(Piece.Actors[0]) : nullptr;
	if (Tower == nullptr || !Platform->TryOccupy(HoveredSlotIndex, Tower))
	{
		UE_LOG(LogBDGrid, Error, TEXT("Slot %d of %s refused '%s' after the preview said it was free."),
			HoveredSlotIndex, *GetNameSafe(Platform->GetOwner()), *GetNameSafe(CurrentSelection));
		for (AActor* Actor : Piece.Actors)
		{
			if (Actor != nullptr)
			{
				Actor->Destroy();
			}
		}
		return false;
	}

	Piece.Platform = Platform;
	Piece.SlotIndex = HoveredSlotIndex;
	Piece.Yaw = Tower->GetActorRotation().Yaw;
	PlacedOnSlots.Add(Piece);

	UE_LOG(LogBDGrid, Verbose, TEXT("Placed '%s' on slot %d of %s."),
		*GetNameSafe(CurrentSelection), HoveredSlotIndex, *GetNameSafe(Platform->GetOwner()));

	return true;
}

bool UBDPlacementComponent::PlaceCellPiece(UBDGridSubsystem& Grid, FBDPlacedPiece& Piece)
{
	TArray<FBDCellCoord> FootprintCells;
	GetFootprintCells(HoveredCell, FootprintCells);
	if (FootprintCells.Num() == 0)
	{
		return false;
	}

	Piece.Origin = HoveredCell;
	// The rotated footprint is what gets stored, so removal frees exactly the cells that
	// were taken rather than the unrotated shape. The yaw goes with it: the footprint
	// alone cannot tell 0 from 180.
	Piece.Footprint = GetEffectiveFootprint();
	Piece.Yaw = GetPlacementYaw();

	// The same layout the ghost showed, on the floor under it.
	TArray<FTransform> InstanceTransforms;
	BuildInstanceTransforms(InstanceTransforms);
	SpawnPieceActors(InstanceTransforms, Piece);

	for (AActor* Actor : Piece.Actors)
	{
		if (ABDTowerBase* Tower = Cast<ABDTowerBase>(Actor))
		{
			Tower->SetGroundCoord(HoveredCell);
		}

		// A platform stamps its own cells from its pivot, which is the middle of the
		// footprint here, not its corner: hand it the cells about to be written. Before
		// the grid write, so what it remembers under its stamp is the Free board, and
		// what it puts back when it goes is Free too.
		if (UBDPlatformComponent* PlatformComponent = Actor != nullptr ? Actor->FindComponentByClass<UBDPlatformComponent>() : nullptr)
		{
			PlatformComponent->SetPlacedFootprint(Piece.Origin, Piece.Footprint);
		}
	}

	// The grid is written last, so a failed spawn cannot leave cells marked as taken
	// by a piece that does not exist.
	for (const FBDCellCoord& Coord : FootprintCells)
	{
		Grid.SetCellState(Coord, CurrentSelection->OccupiesAs);
		PlacedByCell.Add(Coord, Piece);
	}

	UE_LOG(LogBDGrid, Verbose, TEXT("Placed '%s' at %s covering %d cell(s) with %d actor(s), yaw %.0f."),
		*GetNameSafe(CurrentSelection), *HoveredCell.ToString(), FootprintCells.Num(), Piece.Actors.Num(), Piece.Yaw);

	return true;
}

bool UBDPlacementComponent::PlaceEdgePiece(UBDGridSubsystem& Grid, FBDPlacedPiece& Piece)
{
	GetSelectionSegmentEdges(Piece.Edges);
	if (Piece.Edges.Num() == 0)
	{
		return false;
	}

	Piece.Yaw = CurrentSelection->GetYawForEdge(Piece.Edges[0].Direction);

	TArray<FTransform> InstanceTransforms;
	BuildInstanceTransforms(InstanceTransforms);
	SpawnPieceActors(InstanceTransforms, Piece);

	for (const FBDEdgeCoord& Edge : Piece.Edges)
	{
		Grid.SetEdgeBlocked(Edge, true);
		PlacedByEdge.Add(Edge, Piece);
	}

	UE_LOG(LogBDGrid, Verbose, TEXT("Placed '%s' along %d edge(s) from %s with %d actor(s)."),
		*GetNameSafe(CurrentSelection), Piece.Edges.Num(), *Piece.Edges[0].ToString(), Piece.Actors.Num());

	return true;
}

bool UBDPlacementComponent::PlaceObjectivePiece()
{
	UBDObjectiveSubsystem* Objectives = GetObjectives();
	if (Objectives == nullptr)
	{
		return false;
	}

	// The ghost mesh doubles as the urn mesh when the actor class brings none, so what
	// the player saw while placing is what stands there.
	UClass* ActorClass = CurrentSelection->ActorClass.LoadSynchronous();
	UStaticMesh* Mesh = CurrentSelection->PreviewMesh.LoadSynchronous();

	EBDObjectiveRefusal Refusal;
	return Objectives->PlaceObjective(HoveredCell, ActorClass, Mesh, Refusal);
}

bool UBDPlacementComponent::TryPlaceAtHovered()
{
	UBDGridSubsystem* Grid = GetGrid();
	if (CurrentSelection == nullptr || Grid == nullptr || !bHoveringGrid || !bCurrentPlacementValid)
	{
		return false;
	}

	if (bMoving)
	{
		if (IsHoveringMoveOrigin())
		{
			// Dropped where it was picked up: a change of mind, not a move.
			CancelMove();
			return true;
		}

		// Charged before the board is touched, so a refused payment leaves everything lifted.
		ABDMatchManager* Match = GetMatch();
		const int32 Cost = GetMoveCost();
		if (Match != nullptr && !Match->SpendPublicMoney(Cost, EBDFundsUse::Move))
		{
			UE_LOG(LogBDGrid, Warning, TEXT("Move of '%s' refused: %d public money needed."), *GetNameSafe(CurrentSelection), Cost);
			return false;
		}

		if (!DropMovingPiece())
		{
			// The board said yes a frame ago and no now. Give the money back and go home.
			if (Match != nullptr)
			{
				Match->ReturnPublicMoney(Cost, EBDFundsUse::Move, TEXT("a move the board refused"));
			}
			CancelMove();
			return false;
		}

		UE_LOG(LogBDGrid, Log, TEXT("Moved '%s' to %s for %d public money."),
			*GetNameSafe(MovingPiece.Data),
			IsEdgeSelection() ? *HoveredEdge.ToString() : *HoveredCell.ToString(), Cost);

		bMoving = false;
		MovingPiece = FBDPlacedPiece();
		MovingMounted.Reset();
		CancelSelection();
		return true;
	}

	// Charged before anything is spawned or written, so a refused budget leaves no trace.
	// Two charges, in this order: the hand of the kind, when the kind is held at all, then
	// the public money. The money goes last because it is the one that can be handed
	// straight back, and for a defender it is the only charge there is. The votes are
	// never touched: they are the score.
	ABDMatchManager* PayingMatch = GetMatch();
	const int32 BuildCost = PayingMatch != nullptr ? PayingMatch->GetBuildPrice(CurrentSelection) : 0;
	if (ABDMatchManager* Match = PayingMatch)
	{
		if (!Match->ConsumeBudget(CurrentSelection->GetPieceKind()))
		{
			UE_LOG(LogBDGrid, Verbose, TEXT("Placement of '%s' refused: nothing left in the budget."),
				*GetNameSafe(CurrentSelection));
			return false;
		}

		// The urn is free and is not a piece of the defense: it is not paid for at all.
		if (!bRestoring && !IsObjectiveSelection() && !Match->SpendPublicMoney(BuildCost, EBDFundsUse::Build))
		{
			// The hand was already charged, so it goes back before leaving.
			Match->RefundRemoval(CurrentSelection->GetPieceKind());
			UE_LOG(LogBDGrid, Verbose, TEXT("Placement of '%s' refused: %d public money needed, %d held."),
				*GetNameSafe(CurrentSelection), BuildCost, Match->GetPublicMoney());
			return false;
		}
	}

	if (IsObjectiveSelection())
	{
		if (!PlaceObjectivePiece())
		{
			RefundRefusedPlacement(BuildCost);
			return false;
		}

		EvaluatePlacement();
		CancelSelectionIfBudgetExhausted();
		return true;
	}

	FBDPlacedPiece Piece;
	Piece.Data = CurrentSelection;
	Piece.PaidCost = bRestoring ? RestoringPaidCost : BuildCost;

	const bool bPlaced = IsHoveringSlot()
		? PlaceSlotPiece(Piece)
		: (IsEdgeSelection() ? PlaceEdgePiece(*Grid, Piece) : PlaceCellPiece(*Grid, Piece));
	if (!bPlaced)
	{
		// The board said yes a frame ago and no now: nothing was built, so nothing is paid.
		RefundRefusedPlacement(BuildCost);
		return false;
	}

	// A platform built is room for characters, and a platform sold takes that room away.
	// Nothing has to be told: the free slots are counted off the board whenever anybody
	// asks, so what stands there is the only bookkeeping there is.
	UE_CLOG(CountPlatformSlots(Piece) > 0, LogBDGrid, Verbose, TEXT("'%s' built with %d slot(s); %d free on the board."),
		*GetNameSafe(Piece.Data), CountPlatformSlots(Piece), CountFreeSlots());
	UE_CLOG(!bRestoring && PayingMatch != nullptr, LogBDMatch, Log, TEXT("BUILT '%s' on wave %d for %d public money (a candidate of this wave drops %d): %d left. Votes untouched at %d blue / %d red."),
		*GetNameSafe(Piece.Data), PayingMatch->GetCurrentWave(), BuildCost, PayingMatch->GetCandidateFunds(PayingMatch->GetPriceWave()),
		PayingMatch->GetPublicMoney(), PayingMatch->GetVotesBlue(), PayingMatch->GetVotesRed());

	EvaluatePlacement();
	CancelSelectionIfBudgetExhausted();
	return true;
}

const FBDPlacedPiece* UBDPlacementComponent::FindPieceUnderHover() const
{
	const UBDGridSubsystem* Grid = GetGrid();

	// A tower on a slot comes before the platform under it, when the cursor is nearer
	// to that slot than to any other slot of the platform: the player is pointing at
	// the tower, not at the truck.
	if (Grid != nullptr && Grid->GetCellState(HoveredCell) == EBDCellState::Platform)
	{
		if (const UBDPlatformComponent* Platform = FindPlatformAt(HoveredCell))
		{
			const int32 NearestSlot = FindNearestSlot(*Platform, HoverPoint);
			for (const FBDPlacedPiece& Piece : PlacedOnSlots)
			{
				if (Piece.Platform.Get() == Platform && Piece.SlotIndex == NearestSlot)
				{
					return &Piece;
				}
			}
		}
	}

	const FBDPlacedPiece* CellPiece = PlacedByCell.Find(HoveredCell);
	const FBDPlacedPiece* EdgePiece = PlacedByEdge.Find(HoveredEdge);

	if (CellPiece == nullptr || EdgePiece == nullptr || Grid == nullptr)
	{
		return CellPiece != nullptr ? CellPiece : EdgePiece;
	}

	// Both a platform and a fence on its side: whichever the cursor is nearer to.
	const float ToEdge = FVector::DistSquared2D(Grid->EdgeToWorld(HoveredEdge), HoverPoint);
	const float ToCell = FVector::DistSquared2D(Grid->CellToWorld(HoveredCell), HoverPoint);
	return ToEdge < ToCell ? EdgePiece : CellPiece;
}

void UBDPlacementComponent::ForgetSlotPiecesOn(const FBDPlacedPiece& PlatformPiece)
{
	for (const AActor* Actor : PlatformPiece.Actors)
	{
		const UBDPlatformComponent* Platform = Actor != nullptr ? Actor->FindComponentByClass<UBDPlatformComponent>() : nullptr;
		if (Platform == nullptr)
		{
			continue;
		}

		// Walked on a copy: forgetting a slot piece edits PlacedOnSlots.
		const TArray<FBDPlacedPiece> Mounted = PlacedOnSlots.FilterByPredicate([Platform](const FBDPlacedPiece& Piece)
		{
			return Piece.Platform.Get() == Platform;
		});

		for (const FBDPlacedPiece& Piece : Mounted)
		{
			// Same rule as a defender sold on its own: the levels were paid for in public
			// money, so part of that public money comes back rather than going down with
			// the platform.
			RefundEvolution(Piece);
			ForgetPiece(*GetGrid(), Piece);
			// A defender that rode down with its platform is not sold, so nothing is paid
			// for it - and there is no hand for it to go back to either: what it cost went
			// when it was built. The player loses the passengers with the truck, which is
			// what makes selling a full platform a decision.
			UE_CLOG(Piece.Data != nullptr, LogBDMatch, Log, TEXT("'%s' went down with its platform, nothing paid back."),
				*GetNameSafe(Piece.Data));
		}
	}
}

void UBDPlacementComponent::ForgetPiece(UBDGridSubsystem& Grid, const FBDPlacedPiece& Piece)
{
	if (Piece.IsOnSlot())
	{
		if (UBDPlatformComponent* Platform = Piece.Platform.Get())
		{
			Platform->Release(Piece.SlotIndex);
		}

		const TWeakObjectPtr<UBDPlatformComponent> PlatformKey = Piece.Platform;
		const int32 SlotKey = Piece.SlotIndex;
		for (AActor* Actor : Piece.Actors)
		{
			if (Actor != nullptr)
			{
				Actor->Destroy();
			}
		}
		PlacedOnSlots.RemoveAll([&PlatformKey, SlotKey](const FBDPlacedPiece& Other)
		{
			return Other.Platform == PlatformKey && Other.SlotIndex == SlotKey;
		});
		return;
	}

	// A platform goes with everything mounted on it.
	ForgetSlotPiecesOn(Piece);

	for (const FBDEdgeCoord& Edge : Piece.Edges)
	{
		Grid.SetEdgeBlocked(Edge, false);
		PlacedByEdge.Remove(Edge);
	}

	if (Piece.Edges.Num() == 0)
	{
		const int32 SpanX = FMath::Max(1, Piece.Footprint.X);
		const int32 SpanY = FMath::Max(1, Piece.Footprint.Y);

		for (int32 Y = 0; Y < SpanY; ++Y)
		{
			for (int32 X = 0; X < SpanX; ++X)
			{
				const FBDCellCoord Coord(Piece.Origin.X + X, Piece.Origin.Y + Y);
				Grid.SetCellState(Coord, EBDCellState::Free);
				PlacedByCell.Remove(Coord);
			}
		}
	}

	for (AActor* Actor : Piece.Actors)
	{
		if (Actor != nullptr)
		{
			Actor->Destroy();
		}
	}
}

bool UBDPlacementComponent::TryRemoveAtHovered()
{
	UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr || !bHoveringGrid)
	{
		return false;
	}

	const FBDPlacedPiece* Found = FindPieceUnderHover();
	if (Found == nullptr)
	{
		return false;
	}

	// Copied before touching the maps, which are about to drop the entry this points at.
	const FBDPlacedPiece Piece = *Found;
	return SellPiece(Piece);
}

const FBDPlacedPiece* UBDPlacementComponent::FindPieceOfActor(const AActor* Actor) const
{
	if (Actor == nullptr)
	{
		return nullptr;
	}

	for (const FBDPlacedPiece& Piece : PlacedOnSlots)
	{
		if (Piece.Actors.Contains(Actor))
		{
			return &Piece;
		}
	}

	for (const TPair<FBDCellCoord, FBDPlacedPiece>& Entry : PlacedByCell)
	{
		if (Entry.Value.Actors.Contains(Actor))
		{
			return &Entry.Value;
		}
	}

	for (const TPair<FBDEdgeCoord, FBDPlacedPiece>& Entry : PlacedByEdge)
	{
		if (Entry.Value.Actors.Contains(Actor))
		{
			return &Entry.Value;
		}
	}

	return nullptr;
}

const UBDPlaceableData* UBDPlacementComponent::FindPlaceableOfActor(const AActor* Actor) const
{
	const FBDPlacedPiece* Piece = FindPieceOfActor(Actor);
	return Piece != nullptr ? Piece->Data.Get() : nullptr;
}

int32 UBDPlacementComponent::FindPaidCostOfActor(const AActor* Actor) const
{
	const FBDPlacedPiece* Piece = FindPieceOfActor(Actor);
	return Piece != nullptr ? Piece->PaidCost : 0;
}

bool UBDPlacementComponent::TrySellActor(AActor* Actor)
{
	const FBDPlacedPiece* Found = FindPieceOfActor(Actor);
	if (Found == nullptr || GetGrid() == nullptr)
	{
		return false;
	}

	// A lifted piece is not on the board to be sold.
	if (bMoving)
	{
		CancelMove();
	}

	const FBDPlacedPiece Piece = *Found;
	if (SelectedDefender.Get() == Actor)
	{
		SelectedDefender.Reset();
	}

	return SellPiece(Piece);
}

void UBDPlacementComponent::RefundRefusedPlacement(const int32 BuildCost)
{
	// Charged before the board was touched; if the board then refused, both charges come
	// straight back. Never a partial payment for a piece that does not exist - and never
	// a payment for one that was never charged, which is what a restore is.
	if (ABDMatchManager* Match = GetMatch())
	{
		if (!bRestoring && !IsObjectiveSelection())
		{
			Match->ReturnPublicMoney(BuildCost, EBDFundsUse::Build, TEXT("a placement the board refused"));
		}
		if (CurrentSelection != nullptr)
		{
			Match->RefundRemoval(CurrentSelection->GetPieceKind());
		}
	}
}

int32 UBDPlacementComponent::CountPlatformSlots(const FBDPlacedPiece& Piece)
{
	int32 Slots = 0;
	for (const AActor* Actor : Piece.Actors)
	{
		if (const UBDPlatformComponent* Platform = Actor != nullptr ? Actor->FindComponentByClass<UBDPlatformComponent>() : nullptr)
		{
			Slots += Platform->Slots.Num();
		}
	}
	return Slots;
}

int32 UBDPlacementComponent::RefundEvolution(const FBDPlacedPiece& Piece)
{
	ABDMatchManager* Match = GetMatch();
	const ABDTowerBase* Tower = Piece.Actors.Num() > 0 ? Cast<ABDTowerBase>(Piece.Actors[0]) : nullptr;
	const UBDTowerData* TowerData = Tower != nullptr ? Tower->GetData() : nullptr;
	if (Match == nullptr || TowerData == nullptr || Tower->GetTowerLevel() <= 1)
	{
		return 0;
	}

	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
	const int32 Level = Tower->GetTowerLevel();
	const int32 Spent = Tower->GetEvolutionSpent();
	const int32 Back = Balance.GetEvolutionRefund(Spent);
	if (Back <= 0)
	{
		return 0;
	}

	Match->PayRefund(Back, FString::Printf(TEXT("'%s' at level %d taken off the board"), *GetNameSafe(Piece.Data), Level));
	UE_LOG(LogBDBribe, Log, TEXT("EVOLUTION REFUND for '%s' at level %d: %d of the %d public money it cost comes back (%.0f%%), %d lost."),
		*GetNameSafe(Piece.Data), Level, Back, Spent, Balance.EvolutionRefundRatio * 100.0f, Spent - Back);
	return Back;
}

bool UBDPlacementComponent::SellPiece(const FBDPlacedPiece& Piece)
{
	UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr)
	{
		return false;
	}

	const EBDPieceKind Kind = Piece.Data != nullptr ? Piece.Data->GetPieceKind() : EBDPieceKind::Platform;
	ABDMatchManager* Match = GetMatch();
	if (Match != nullptr && !Match->CanRemove(Kind))
	{
		UE_LOG(LogBDGrid, Verbose, TEXT("Removal of '%s' refused: the urn is not taken back."),
			*GetNameSafe(Piece.Data));
		return false;
	}

	FString Where;
	if (Piece.IsOnSlot())
	{
		const UBDPlatformComponent* Platform = Piece.Platform.Get();
		Where = FString::Printf(TEXT("slot %d of %s"), Piece.SlotIndex, *GetNameSafe(Platform != nullptr ? Platform->GetOwner() : nullptr));
	}
	else
	{
		Where = Piece.Edges.Num() > 0 ? Piece.Edges[0].ToString() : Piece.Origin.ToString();
	}

	// What the levels cost comes back first, while the tower is still there to be asked
	// what level it reached. The platform's passengers pay their own back inside
	// ForgetPiece, on the same rule.
	const int32 EvolutionBack = RefundEvolution(Piece);

	// Its slots leave the board with it, and nothing has to be told: the free ones are
	// counted off the platforms still standing. Read now only so the log can say what
	// the sale took away with it.
	const int32 SlotsLeaving = CountPlatformSlots(Piece);

	// The platform's passengers come off first, inside ForgetPiece, and go back to the
	// hand; only the piece under the cursor is sold.
	ForgetPiece(*Grid, Piece);

	if (Match != nullptr)
	{
		Match->RefundRemoval(Kind);
		UE_CLOG(SlotsLeaving > 0, LogBDGrid, Verbose, TEXT("'%s' sold with %d slot(s); %d free on the board."),
			*GetNameSafe(Piece.Data), SlotsLeaving, CountFreeSlots());

		// Selling pays back a share of what the piece was bought for, in public money. Never
		// of today's price, and never in votes: the count is the score and nothing else.
		const int32 Refund = Match->RefundSale(Kind, Piece.PaidCost);
		UE_LOG(LogBDMatch, Log, TEXT("Sold '%s' at %s for %d public money (%.0f%% of the %d paid) and %d back from its levels: now %d public money, votes untouched at %d blue / %d red."),
			*GetNameSafe(Piece.Data), *Where, Refund, Match->GetSellRefundRatio(Kind) * 100.0f, Piece.PaidCost, EvolutionBack,
			Match->GetPublicMoney(), Match->GetVotesBlue(), Match->GetVotesRed());
	}
	else
	{
		UE_LOG(LogBDGrid, Verbose, TEXT("Removed '%s' from %s."), *GetNameSafe(Piece.Data), *Where);
	}

	EvaluatePlacement();
	return true;
}

//~ Moving ------------------------------------------------------------------------

void UBDPlacementComponent::SetActorsHidden(const TArray<TObjectPtr<AActor>>& Actors, const bool bHidden)
{
	for (AActor* Actor : Actors)
	{
		if (Actor != nullptr)
		{
			Actor->SetActorHiddenInGame(bHidden);
			Actor->SetActorEnableCollision(!bHidden);
		}
	}
}

int32 UBDPlacementComponent::GetMoveCost() const
{
	const ABDMatchManager* Match = GetMatch();
	if (!bMoving || Match == nullptr || MovingPiece.Data == nullptr)
	{
		return 0;
	}

	return Match->GetMoveCost(MovingPiece.PaidCost);
}

bool UBDPlacementComponent::IsHoveringMoveOrigin() const
{
	if (!bMoving || !bHoveringGrid || RotationSteps != MoveOriginRotationSteps)
	{
		return false;
	}

	if (MoveOriginSlot != INDEX_NONE)
	{
		return IsHoveringSlot() && HoveredPlatform == MoveOriginPlatform && HoveredSlotIndex == MoveOriginSlot;
	}

	if (IsEdgeSelection())
	{
		return HoveredEdge == MoveOriginEdge;
	}

	return !IsHoveringSlot() && HoveredCell == MoveOriginCell;
}

void UBDPlacementComponent::LiftSlotPiecesOn(const FBDPlacedPiece& PlatformPiece)
{
	for (const AActor* Actor : PlatformPiece.Actors)
	{
		UBDPlatformComponent* Platform = Actor != nullptr ? Actor->FindComponentByClass<UBDPlatformComponent>() : nullptr;
		if (Platform == nullptr)
		{
			continue;
		}

		for (int32 Index = PlacedOnSlots.Num() - 1; Index >= 0; --Index)
		{
			if (PlacedOnSlots[Index].Platform.Get() != Platform)
			{
				continue;
			}

			FBDPlacedPiece Mounted = PlacedOnSlots[Index];
			PlacedOnSlots.RemoveAt(Index);
			Platform->Release(Mounted.SlotIndex);
			SetActorsHidden(Mounted.Actors, true);
			MovingMounted.Add(Mounted);
		}
	}
}

void UBDPlacementComponent::RemountLiftedPieces(const FBDPlacedPiece& PlatformPiece)
{
	UBDPlatformComponent* Platform = nullptr;
	for (const AActor* Actor : PlatformPiece.Actors)
	{
		Platform = Actor != nullptr ? Actor->FindComponentByClass<UBDPlatformComponent>() : nullptr;
		if (Platform != nullptr)
		{
			break;
		}
	}

	for (FBDPlacedPiece& Mounted : MovingMounted)
	{
		ABDTowerBase* Tower = Mounted.Actors.Num() > 0 ? Cast<ABDTowerBase>(Mounted.Actors[0]) : nullptr;
		SetActorsHidden(Mounted.Actors, false);

		if (Platform == nullptr || Tower == nullptr || !Platform->TryOccupy(Mounted.SlotIndex, Tower))
		{
			UE_LOG(LogBDGrid, Error, TEXT("Could not remount '%s' on slot %d after moving its platform."),
				*GetNameSafe(Mounted.Data), Mounted.SlotIndex);
			continue;
		}

		Mounted.Platform = Platform;
		PlacedOnSlots.Add(Mounted);
	}

	MovingMounted.Reset();
}

bool UBDPlacementComponent::TryBeginMoveAtHovered()
{
	UBDGridSubsystem* Grid = GetGrid();
	if (bMoving || Grid == nullptr || !bHoveringGrid)
	{
		return false;
	}

	const ABDMatchManager* Match = GetMatch();
	if (Match != nullptr && !Match->CanMove())
	{
		UE_LOG(LogBDGrid, Log, TEXT("Nothing moves outside the building phase."));
		return false;
	}

	const FBDPlacedPiece* Found = FindPieceUnderHover();
	if (Found == nullptr || Found->Data == nullptr)
	{
		// A click on nothing of the player's ends whatever was selected for an upgrade.
		SelectDefender(nullptr);
		return false;
	}

	// Copied before the maps drop the entry this points at.
	MovingPiece = *Found;

	// Remember where it came from. The origin hover is what a cancel points back at.
	MoveOriginSlot = MovingPiece.SlotIndex;
	MoveOriginPlatform = MovingPiece.Platform;
	MoveOriginCell = MovingPiece.IsOnSlot() ? HoveredCell : MovingPiece.Origin;
	MoveOriginEdge = MovingPiece.Edges.Num() > 0 ? MovingPiece.Edges[0] : FBDEdgeCoord();
	MoveOriginRotationSteps = MovingPiece.Edges.Num() > 0
		? (MovingPiece.Edges[0].Direction == FBDEdgeCoord::DirectionX ? 1 : 0)
		: FMath::RoundToInt(MovingPiece.Yaw / DegreesPerRotationStep) % CellRotationStepCount;

	// Off the board, actors kept but out of sight. A platform takes its passengers along.
	if (MovingPiece.IsOnSlot())
	{
		if (UBDPlatformComponent* Platform = MovingPiece.Platform.Get())
		{
			Platform->Release(MovingPiece.SlotIndex);
		}
		PlacedOnSlots.RemoveAll([this](const FBDPlacedPiece& Other)
		{
			return Other.Platform == MovingPiece.Platform && Other.SlotIndex == MovingPiece.SlotIndex;
		});
	}
	else
	{
		LiftSlotPiecesOn(MovingPiece);

		for (const FBDEdgeCoord& Edge : MovingPiece.Edges)
		{
			Grid->SetEdgeBlocked(Edge, false);
			PlacedByEdge.Remove(Edge);
		}

		if (MovingPiece.Edges.Num() == 0)
		{
			for (int32 Y = 0; Y < FMath::Max(1, MovingPiece.Footprint.Y); ++Y)
			{
				for (int32 X = 0; X < FMath::Max(1, MovingPiece.Footprint.X); ++X)
				{
					const FBDCellCoord Coord(MovingPiece.Origin.X + X, MovingPiece.Origin.Y + Y);
					PlacedByCell.Remove(Coord);
					Grid->SetCellState(Coord, EBDCellState::Free);
				}
			}

			// The platform component would put its stamp back on the next tick: tell it
			// the piece is off the board by clearing its placed footprint into nothing.
			for (const AActor* Actor : MovingPiece.Actors)
			{
				if (UBDPlatformComponent* PlatformComponent = Actor != nullptr ? Actor->FindComponentByClass<UBDPlatformComponent>() : nullptr)
				{
					PlatformComponent->ClearPlacedFootprint();
				}
			}
		}
	}

	SetActorsHidden(MovingPiece.Actors, true);
	bMoving = true;

	// Held like a fresh selection, facing the way it faced. The data is not written to;
	// the selection slot simply is not const.
	CurrentSelection = const_cast<UBDPlaceableData*>(MovingPiece.Data.Get());
	RotationSteps = MoveOriginRotationSteps;
	EnsureMatchBinding();
	EnsurePreview();
	if (Preview != nullptr)
	{
		Preview->SetPlaceable(CurrentSelection);
	}

	if (bHoveringGrid)
	{
		ResolveHover(HoverPoint);
	}
	EvaluatePlacement();

	UE_LOG(LogBDGrid, Log, TEXT("Lifted '%s' from %s%s; move tax %d public money at %.0f%%."),
		*GetNameSafe(MovingPiece.Data),
		MovingPiece.Edges.Num() > 0 ? *MoveOriginEdge.ToString() : *MoveOriginCell.ToString(),
		MoveOriginSlot != INDEX_NONE ? *FString::Printf(TEXT(" slot %d"), MoveOriginSlot) : TEXT(""),
		GetMoveCost(), Match != nullptr ? Match->GetMoveTaxRate() * 100.0f : 0.0f);

	return true;
}

bool UBDPlacementComponent::DropMovingPiece()
{
	UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr)
	{
		return false;
	}

	// The same placement paths a fresh piece takes, with the lifted actors riding along.
	FBDPlacedPiece Piece = MovingPiece;
	Piece.Edges.Reset();
	Piece.Platform.Reset();
	Piece.SlotIndex = INDEX_NONE;

	bool bPlaced;
	if (IsHoveringSlot())
	{
		bPlaced = PlaceSlotPiece(Piece);
	}
	else if (IsEdgeSelection())
	{
		bPlaced = PlaceEdgePiece(*Grid, Piece);
	}
	else
	{
		bPlaced = PlaceCellPiece(*Grid, Piece);
		if (bPlaced)
		{
			RemountLiftedPieces(Piece);
		}
	}

	return bPlaced;
}

void UBDPlacementComponent::HoverMoveOrigin()
{
	RotationSteps = MoveOriginRotationSteps;

	if (MoveOriginSlot != INDEX_NONE)
	{
		// The slot's own location as the cursor point, so the nearest slot is that slot.
		if (const UBDPlatformComponent* Platform = MoveOriginPlatform.Get())
		{
			HoverPoint = Platform->GetSlotWorldTransform(MoveOriginSlot).GetLocation();
		}
		HoveredCell = MoveOriginCell;
		bHoveringGrid = true;
		EvaluatePlacement();
		return;
	}

	if (IsEdgeSelection())
	{
		SetHoveredEdgeDirect(MoveOriginEdge);
		return;
	}

	SetHoveredCellDirect(MoveOriginCell);
}

void UBDPlacementComponent::CancelMove()
{
	if (!bMoving)
	{
		return;
	}

	// Home is where it stood a moment ago, so the board takes it back as it was. The
	// validation still runs, because nothing else is allowed to write the board.
	HoverMoveOrigin();
	const bool bRestored = bCurrentPlacementValid && DropMovingPiece();
	if (!bRestored)
	{
		UE_LOG(LogBDGrid, Error, TEXT("Could not put '%s' back at %s (%s); the piece is lost."),
			*GetNameSafe(MovingPiece.Data), *MoveOriginCell.ToString(), *DescribeCurrentRefusal());
		for (AActor* Actor : MovingPiece.Actors)
		{
			if (Actor != nullptr)
			{
				Actor->Destroy();
			}
		}
		for (const FBDPlacedPiece& Mounted : MovingMounted)
		{
			for (AActor* Actor : Mounted.Actors)
			{
				if (Actor != nullptr)
				{
					Actor->Destroy();
				}
			}
		}
	}
	else
	{
		UE_LOG(LogBDGrid, Log, TEXT("'%s' put back at %s, nothing charged."),
			*GetNameSafe(MovingPiece.Data), MovingPiece.Edges.Num() > 0 ? *MoveOriginEdge.ToString() : *MoveOriginCell.ToString());
	}

	bMoving = false;
	MovingPiece = FBDPlacedPiece();
	MovingMounted.Reset();
	CancelSelection();
}

//~ Input ----------------------------------------------------------------------

void UBDPlacementComponent::HandlePlaceInput()
{
	// With nothing in hand a press on a placed piece picks it up; otherwise it places.
	// While a wave is out nothing moves, but a defender can still be picked and evolved:
	// that is the reaction the battle asks for, and the lift is not the way to it then.
	if (CurrentSelection == nullptr && !bMoving)
	{
		const ABDMatchManager* Match = GetMatch();
		if (Match != nullptr && Match->GetPhase() == EBDMatchPhase::WaveActive)
		{
			ClickDefenderAtHovered();
			return;
		}
		TryBeginMoveAtHovered();
		return;
	}

	TryPlaceAtHovered();
}

void UBDPlacementComponent::HandlePlaceReleased()
{
	if (!bMoving)
	{
		return;
	}

	// Released where it was pressed: a click, not a drag. The piece goes home and, when
	// it is a defender, it becomes the one selected for an upgrade; a second click on
	// the same one buys the level.
	if (IsHoveringMoveOrigin())
	{
		ABDTowerBase* Clicked = MovingPiece.Actors.Num() > 0 ? Cast<ABDTowerBase>(MovingPiece.Actors[0]) : nullptr;
		CancelMove();

		if (Clicked != nullptr && Clicked == SelectedDefender.Get())
		{
			UpgradeSelectedDefender();
		}
		else
		{
			SelectDefender(Clicked);
		}
		return;
	}

	// Released over a bad spot: back where it was, nothing charged.
	if (!TryPlaceAtHovered())
	{
		CancelMove();
	}
}

void UBDPlacementComponent::SelectDefender(ABDTowerBase* Tower)
{
	SelectedDefender = Tower;
	if (Tower == nullptr)
	{
		return;
	}

	// The deal is stated at selection, so the second click is made knowing.
	UE_LOG(LogBDGrid, Log, TEXT("Selected %s. Click it again to buy. %s"), *Tower->GetName(), *Tower->DescribeUpgrade());
}

void UBDPlacementComponent::ClickDefenderAtHovered()
{
	if (!bHoveringGrid)
	{
		return;
	}

	// Only the defender itself: a click on the truck under a shooter, or on a fence,
	// ends the selection like a click on empty ground does.
	const FBDPlacedPiece* Found = FindPieceUnderHover();
	ABDTowerBase* Clicked = Found != nullptr && Found->Actors.Num() > 0 ? Cast<ABDTowerBase>(Found->Actors[0]) : nullptr;
	if (Clicked != nullptr && Clicked == SelectedDefender.Get())
	{
		UpgradeSelectedDefender();
		return;
	}
	SelectDefender(Clicked);
}

void UBDPlacementComponent::DebugClick()
{
	HandlePlaceInput();
	HandlePlaceReleased();
}

bool UBDPlacementComponent::UpgradeSelectedDefender()
{
	ABDTowerBase* Tower = SelectedDefender.Get();
	if (Tower == nullptr)
	{
		return false;
	}

	const bool bUpgraded = Tower->Upgrade();
	if (bUpgraded && !Tower->IsMaxLevel())
	{
		// Still selected: the next level is on offer straight away.
		UE_LOG(LogBDGrid, Log, TEXT("%s"), *Tower->DescribeUpgrade());
	}
	return bUpgraded;
}

void UBDPlacementComponent::HandleRemoveInput()
{
	TryRemoveAtHovered();
}

void UBDPlacementComponent::HandleCancelInput()
{
	if (bMoving)
	{
		CancelMove();
		return;
	}

	if (CurrentSelection != nullptr || SelectedDefender.IsValid())
	{
		CancelSelection();
		SelectDefender(nullptr);
		return;
	}

	// Nothing in hand and nothing selected: Escape is the game menu.
	const UWorld* World = GetWorld();
	UBDUISubsystem* UI = World != nullptr && World->GetGameInstance() != nullptr ? World->GetGameInstance()->GetSubsystem<UBDUISubsystem>() : nullptr;
	if (UI != nullptr)
	{
		UI->TogglePauseMenu();
	}
}

void UBDPlacementComponent::HandleRotateInput(const FInputActionValue& Value)
{
	// The wheel reports a signed step: one notch is one quarter turn, and the direction
	// decides which way, so the player can undo a turn by scrolling back.
	const float Step = Value.Get<float>();
	if (!FMath::IsNearlyZero(Step))
	{
		RotateSelection(Step > 0.0f);
	}
}

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
#include "Match/BDMatchManager.h"
#include "Path/BDPathfinder.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementPreview.h"
#include "Placement/BDPlacementSettings.h"
#include "Platform/BDPlatformComponent.h"
#include "Tower/BDTowerBase.h"
#include "Tower/BDTowerData.h"
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

	const EBDPieceKind Kind = CurrentSelection->GetPieceKind();
	if (Match->GetBudgetRemaining(Kind) > 0)
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

	// The wheel is an axis, so this one arrives as Triggered with a value rather than
	// as a one shot Started.
	if (UInputAction* Action = Settings.RotateAction.LoadSynchronous())
	{
		EnhancedInput->BindAction(Action, ETriggerEvent::Triggered, this, &UBDPlacementComponent::HandleRotateInput);
	}

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
	return CurrentSelection != nullptr && CurrentSelection->GetPieceKind() == EBDPieceKind::Tower;
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
	if (CurrentRefusal == LastReportedRefusal)
	{
		return;
	}

	LastReportedRefusal = CurrentRefusal;

	// One line per change of answer, not per frame: a hover across the board that stays
	// refused for the same reason says so once.
	// A move says what it will cost and what the score becomes, so the drop is decided knowing.
	FString CostText;
	if (bMoving)
	{
		const ABDMatchManager* Match = GetMatch();
		const int32 Cost = GetMoveCost();
		const int32 Blue = Match != nullptr ? Match->GetVotesBlue() : 0;
		CostText = IsHoveringMoveOrigin()
			? TEXT(" (back where it was: no charge)")
			: FString::Printf(TEXT(" (move tax %d vote(s) at %.0f%%: blue %d -> %d)"),
				Cost, Match != nullptr ? Match->GetMoveTaxRate() * 100.0f : 0.0f, Blue, Blue - Cost);
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
		else if (bMoving && Match != nullptr && !IsHoveringMoveOrigin() && !Match->CanAffordVotesBlue(GetMoveCost()))
		{
			CurrentRefusal = EBDPlacementRefusal::CannotAffordMove;
		}
		else if (!bMoving && bObjectiveMissing)
		{
			CurrentRefusal = EBDPlacementRefusal::ObjectiveMissing;
		}
		else if (!bMoving && Match != nullptr && !Match->CanPlace(CurrentSelection->GetPieceKind()))
		{
			CurrentRefusal = EBDPlacementRefusal::MatchRefused;
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
		if (Match != nullptr && !Match->SpendVotesBlue(Cost))
		{
			UE_LOG(LogBDGrid, Warning, TEXT("Move of '%s' refused: %d blue vote(s) needed."), *GetNameSafe(CurrentSelection), Cost);
			return false;
		}

		if (!DropMovingPiece())
		{
			// The board said yes a frame ago and no now. Give the votes back and go home.
			if (Match != nullptr)
			{
				Match->AddVotesBlue(Cost);
			}
			CancelMove();
			return false;
		}

		UE_LOG(LogBDGrid, Log, TEXT("Moved '%s' to %s for %d blue vote(s)."),
			*GetNameSafe(MovingPiece.Data),
			IsEdgeSelection() ? *HoveredEdge.ToString() : *HoveredCell.ToString(), Cost);

		bMoving = false;
		MovingPiece = FBDPlacedPiece();
		MovingMounted.Reset();
		CancelSelection();
		return true;
	}

	// Charged before anything is spawned or written, so a refused budget leaves no trace.
	if (ABDMatchManager* Match = GetMatch())
	{
		if (!Match->ConsumeBudget(CurrentSelection->GetPieceKind()))
		{
			UE_LOG(LogBDGrid, Verbose, TEXT("Placement of '%s' refused: nothing left in the budget."),
				*GetNameSafe(CurrentSelection));
			return false;
		}
	}

	if (IsObjectiveSelection())
	{
		if (!PlaceObjectivePiece())
		{
			return false;
		}

		EvaluatePlacement();
		CancelSelectionIfBudgetExhausted();
		return true;
	}

	FBDPlacedPiece Piece;
	Piece.Data = CurrentSelection;

	const bool bPlaced = IsHoveringSlot()
		? PlaceSlotPiece(Piece)
		: (IsEdgeSelection() ? PlaceEdgePiece(*Grid, Piece) : PlaceCellPiece(*Grid, Piece));
	if (!bPlaced)
	{
		return false;
	}

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
	ABDMatchManager* Match = GetMatch();

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
			ForgetPiece(*GetGrid(), Piece);
			if (Match != nullptr)
			{
				// A tower taken back is a tower held again, even when it is the platform that went.
				Match->RefundRemoval(EBDPieceKind::Tower);
			}
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

	const EBDPieceKind Kind = Piece.Data != nullptr ? Piece.Data->GetPieceKind() : EBDPieceKind::Platform;
	ABDMatchManager* Match = GetMatch();
	if (Match != nullptr && !Match->CanRemove(Kind))
	{
		UE_LOG(LogBDGrid, Verbose, TEXT("Removal of '%s' refused: the maze is locked in."),
			*GetNameSafe(Piece.Data));
		return false;
	}

	ForgetPiece(*Grid, Piece);

	if (Match != nullptr)
	{
		Match->RefundRemoval(Kind);
	}

	UE_LOG(LogBDGrid, Verbose, TEXT("Removed '%s' from %s."), *GetNameSafe(Piece.Data),
		Piece.Edges.Num() > 0 ? *Piece.Edges[0].ToString() : *Piece.Origin.ToString());

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

	return Match->GetMoveCost(MovingPiece.Data->GetBuildCost());
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

	UE_LOG(LogBDGrid, Log, TEXT("Lifted '%s' from %s%s; move tax %d blue vote(s) at %.0f%%."),
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
	if (CurrentSelection == nullptr && !bMoving)
	{
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

	// Released over a bad spot: back where it was, nothing charged.
	if (!TryPlaceAtHovered())
	{
		CancelMove();
	}
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

	CancelSelection();
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

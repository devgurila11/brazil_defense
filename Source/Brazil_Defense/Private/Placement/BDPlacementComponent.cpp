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
#include "Match/BDMatchManager.h"
#include "Path/BDPathfinder.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementPreview.h"
#include "Placement/BDPlacementSettings.h"

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

	Super::EndPlay(EndPlayReason);
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

bool UBDPlacementComponent::IsEdgeSelection() const
{
	return CurrentSelection != nullptr && CurrentSelection->bOccupiesEdge;
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

	CurrentSelection = Placeable;
	RotationSteps = 0;

	if (CurrentSelection == nullptr)
	{
		CancelSelection();
		return;
	}

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
	CurrentSelection = nullptr;
	bCurrentPlacementValid = false;

	if (Preview != nullptr)
	{
		Preview->SetPlaceable(nullptr);
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
	if (CurrentRefusal == EBDPlacementRefusal::None)
	{
		UE_LOG(LogBDGrid, Log, TEXT("Placement of '%s': valid at %s."),
			*GetNameSafe(CurrentSelection),
			IsEdgeSelection() ? *HoveredEdge.ToString() : *HoveredCell.ToString());
		return;
	}

	UE_LOG(LogBDGrid, Log, TEXT("Placement of '%s' refused at %s: %s."),
		*GetNameSafe(CurrentSelection),
		bHoveringGrid ? (IsEdgeSelection() ? *HoveredEdge.ToString() : *HoveredCell.ToString()) : TEXT("no cell"),
		*DescribeCurrentRefusal());
}

void UBDPlacementComponent::EvaluatePlacement()
{
	bCurrentPlacementValid = false;

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
		// A piece the player cannot afford, or cannot place in this phase, is refused
		// before the pathfinding: the budget is cheaper to check than the board.
		const ABDMatchManager* Match = GetMatch();
		if (Match != nullptr && !Match->CanPlace(CurrentSelection->GetPieceKind()))
		{
			CurrentRefusal = EBDPlacementRefusal::MatchRefused;
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
	UClass* ActorClass = CurrentSelection != nullptr ? CurrentSelection->ActorClass.LoadSynchronous() : nullptr;
	if (ActorClass == nullptr)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Piece.Actors.Reserve(Transforms.Num());

	for (const FTransform& SpawnTransform : Transforms)
	{
		if (AActor* Spawned = GetWorld()->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams))
		{
			Piece.Actors.Add(Spawned);
		}
	}
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

bool UBDPlacementComponent::TryPlaceAtHovered()
{
	UBDGridSubsystem* Grid = GetGrid();
	if (CurrentSelection == nullptr || Grid == nullptr || !bHoveringGrid || !bCurrentPlacementValid)
	{
		return false;
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

	FBDPlacedPiece Piece;
	Piece.Data = CurrentSelection;

	const bool bPlaced = IsEdgeSelection() ? PlaceEdgePiece(*Grid, Piece) : PlaceCellPiece(*Grid, Piece);
	if (!bPlaced)
	{
		return false;
	}

	EvaluatePlacement();
	return true;
}

const FBDPlacedPiece* UBDPlacementComponent::FindPieceUnderHover() const
{
	const UBDGridSubsystem* Grid = GetGrid();
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

void UBDPlacementComponent::ForgetPiece(UBDGridSubsystem& Grid, const FBDPlacedPiece& Piece)
{
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

//~ Input ----------------------------------------------------------------------

void UBDPlacementComponent::HandlePlaceInput()
{
	TryPlaceAtHovered();
}

void UBDPlacementComponent::HandleRemoveInput()
{
	TryRemoveAtHovered();
}

void UBDPlacementComponent::HandleCancelInput()
{
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

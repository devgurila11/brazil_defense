// Brazil Defense. Definition of something the player can put on the board.

#include "Placement/BDPlaceableData.h"

#include "Grid/BDGridSubsystem.h"
#include "Tower/BDTowerData.h"

const FPrimaryAssetType UBDPlaceableData::PlaceableAssetType = TEXT("BDPlaceable");

FPrimaryAssetId UBDPlaceableData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PlaceableAssetType, GetFName());
}

EBDPieceKind UBDPlaceableData::GetPieceKind() const
{
	if (bOccupiesEdge)
	{
		return EBDPieceKind::Divider;
	}

	switch (OccupiesAs)
	{
	case EBDCellState::Tower:
		if (const UBDTowerData* Data = TowerData.LoadSynchronous())
		{
			if (Data->bCanPlaceOnSlot && !Data->bCanPlaceOnGround)
			{
				return EBDPieceKind::Character;
			}
		}
		return EBDPieceKind::Tower;

	case EBDCellState::Goal:
		return EBDPieceKind::Objective;

	default:
		return EBDPieceKind::Platform;
	}
}

bool UBDPlaceableData::IsDefender() const
{
	const EBDPieceKind Kind = GetPieceKind();
	return Kind == EBDPieceKind::Tower || Kind == EBDPieceKind::Character;
}

int32 UBDPlaceableData::GetBuildCost() const
{
	if (IsDefender())
	{
		if (const UBDTowerData* Data = TowerData.LoadSynchronous())
		{
			return Data->BuildCost;
		}
	}

	return Cost;
}

bool UBDPlaceableData::BlocksMovement() const
{
	// A fence always obstructs the crossing. A tower stands on its cell without
	// obstructing it, so placing one can never cut the creeps off and never needs the
	// blocking check.
	return bOccupiesEdge || !UBDGridSubsystem::IsWalkableState(OccupiesAs);
}

bool UBDPlaceableData::IsValidSetup(FString& OutError) const
{
	if (bOccupiesEdge)
	{
		if (SegmentLength < 1)
		{
			OutError = TEXT("SegmentLength must be at least 1.");
			return false;
		}
		return true;
	}

	if (OccupiesAs == EBDCellState::Goal)
	{
		// The urn stands on one cell: that cell is the Goal, and a goal is a point.
		if (Footprint != FIntPoint(1, 1))
		{
			OutError = TEXT("The objective piece must have a 1x1 footprint.");
			return false;
		}
		return true;
	}

	if (!UBDGridSubsystem::IsPlayerPlacedState(OccupiesAs))
	{
		OutError = TEXT("OccupiesAs is a state the player cannot place. Use Platform, Tower or Goal, or make it an edge piece.");
		return false;
	}

	return true;
}

float UBDPlaceableData::GetYawForEdge(const uint8 EdgeDirection) const
{
	// The fence line runs along Y for a +X edge and along X for a +Y edge. Local X
	// turned by 90 lands on world Y; local Y turned by 270 lands on world X.
	const bool bLineAlongWorldY = EdgeDirection == FBDEdgeCoord::DirectionX;
	if (EdgeAxis == EBDPlaceableAxis::X)
	{
		return bLineAlongWorldY ? 90.0f : 0.0f;
	}

	return bLineAlongWorldY ? 0.0f : 270.0f;
}

void UBDPlaceableData::GetEdgeInstanceTransforms(const uint8 EdgeDirection, const float CellSize,
	TArray<FTransform>& OutTransforms) const
{
	OutTransforms.Reset();

	const FRotator Rotation(0.0f, GetYawForEdge(EdgeDirection), 0.0f);
	const int32 Copies = FMath::Max(1, InstancesPerEdge);

	// Copies are spread along the fence line itself, so a rotated axis is not needed:
	// the line is a world axis.
	const FVector LineDirection = EdgeDirection == FBDEdgeCoord::DirectionX ? FVector::RightVector : FVector::ForwardVector;
	const FVector Step = LineDirection * (CellSize / Copies);

	OutTransforms.Reserve(Copies);
	for (int32 Copy = 0; Copy < Copies; ++Copy)
	{
		// Each copy sits at the center of its slot: with two, at -1/4 and +1/4 of the edge.
		const float Slot = Copy - (Copies - 1) * 0.5f;
		OutTransforms.Emplace(Rotation, Step * Slot);
	}
}

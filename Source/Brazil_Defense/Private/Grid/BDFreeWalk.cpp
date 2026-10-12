// Brazil Defense. Walking free over the board: the rules shared by everyone who is not a creep.

#include "Grid/BDFreeWalk.h"

#include "Algo/Reverse.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "Grid/BDGridSubsystem.h"
#include "Wave/BDWaveSettings.h"

namespace BDFreeWalkPrivate
{
	static const FIntPoint Steps[4] = { FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1) };
}

FBDFreeWalk::FBDFreeWalk(const UWorld* InWorld, const UBDGridSubsystem* InGrid, TArray<const AActor*> InIgnored, const bool bInOverDividers)
	: World(InWorld)
	, Grid(InGrid)
	, Ignored(MoveTemp(InIgnored))
	, bOverDividers(bInOverDividers)
{
}

bool FBDFreeWalk::CanStand(const FBDCellCoord& Coord) const
{
	return Grid != nullptr && Grid->IsValidCoord(Coord) && Grid->GetCellState(Coord) != EBDCellState::Blocked;
}

bool FBDFreeWalk::CanStep(const FBDCellCoord& From, const FBDCellCoord& To) const
{
	return Grid != nullptr && CanStand(To) && (bOverDividers || !Grid->IsEdgeBlocked(From, To));
}

bool FBDFreeWalk::IsLineClear(const FVector& From, const FVector& To, const float Margin) const
{
	if (Grid == nullptr)
	{
		return false;
	}

	const FVector Along = (To - From).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Along) * Margin;
	const int32 Lines = Margin > 0.0f && !Along.IsNearlyZero() ? 3 : 1;
	const float Step = Grid->GetCellSize() * LineCheckStep;
	const int32 Samples = FMath::Max(1, FMath::CeilToInt(FVector::Dist2D(From, To) / Step));

	for (int32 Line = 0; Line < Lines; ++Line)
	{
		const FVector Offset = Line == 0 ? FVector::ZeroVector : (Line == 1 ? Side : -Side);
		FBDCellCoord Previous;
		if (!Grid->WorldToCell(From + Offset, Previous) || !CanStand(Previous))
		{
			return false;
		}

		for (int32 Sample = 1; Sample <= Samples; ++Sample)
		{
			FBDCellCoord Current;
			if (!Grid->WorldToCell(FMath::Lerp(From, To, static_cast<float>(Sample) / Samples) + Offset, Current))
			{
				return false;
			}
			if (Current == Previous)
			{
				continue;
			}

			const int32 DX = Current.X - Previous.X;
			const int32 DY = Current.Y - Previous.Y;
			if (FMath::Abs(DX) + FMath::Abs(DY) == 1)
			{
				if (!CanStep(Previous, Current))
				{
					return false;
				}
			}
			else if (FMath::Abs(DX) == 1 && FMath::Abs(DY) == 1)
			{
				// Across a corner: open if either way round it is, as a man slips past the
				// end of a fence. A divider running on through the corner closes both.
				const FBDCellCoord ByX(Current.X, Previous.Y);
				const FBDCellCoord ByY(Previous.X, Current.Y);
				if (!(CanStep(Previous, ByX) && CanStep(ByX, Current)) && !(CanStep(Previous, ByY) && CanStep(ByY, Current)))
				{
					return false;
				}
			}
			else
			{
				return false;
			}
			Previous = Current;
		}
	}
	return true;
}

TArray<FBDCellCoord> FBDFreeWalk::FindWay(const FBDCellCoord& From, const FBDCellCoord& To) const
{
	TArray<FBDCellCoord> Way;
	if (From == To)
	{
		Way.Add(From);
		return Way;
	}

	TMap<FBDCellCoord, FBDCellCoord> CameFrom;
	TArray<FBDCellCoord> Frontier;
	Frontier.Add(From);
	CameFrom.Add(From, From);

	for (int32 Head = 0; Head < Frontier.Num(); ++Head)
	{
		const FBDCellCoord Current = Frontier[Head];
		for (const FIntPoint& Step : BDFreeWalkPrivate::Steps)
		{
			const FBDCellCoord Next(Current.X + Step.X, Current.Y + Step.Y);
			if (CameFrom.Contains(Next) || !CanStep(Current, Next))
			{
				continue;
			}
			CameFrom.Add(Next, Current);
			if (Next == To)
			{
				for (FBDCellCoord Back = To; Back != From; Back = CameFrom[Back])
				{
					Way.Add(Back);
				}
				Way.Add(From);
				Algo::Reverse(Way);
				return Way;
			}
			Frontier.Add(Next);
		}
	}
	return Way;
}

TArray<FVector> FBDFreeWalk::PlanWay(const FVector& Here, const FVector& Goal) const
{
	TArray<FVector> Points;
	if (Grid == nullptr)
	{
		return Points;
	}
	if (IsLineClear(Here, Goal, 0.0f))
	{
		Points.Add(Goal);
		return Points;
	}

	// Around by the cells, then pulled straight: from each point, the furthest one on
	// that is in plain sight. Two points in a row always are, so the pull never sticks.
	FBDCellCoord From;
	FBDCellCoord To;
	if (!Grid->WorldToCell(Here, From) || !Grid->WorldToCell(Goal, To))
	{
		return Points;
	}
	const TArray<FBDCellCoord> Cells = FindWay(From, To);
	if (Cells.Num() == 0)
	{
		return Points;
	}

	TArray<FVector> Way;
	Way.Add(Here);
	for (int32 Index = 1; Index < Cells.Num() - 1; ++Index)
	{
		Way.Add(CellPoint(Cells[Index]));
	}
	Way.Add(Goal);

	const float Margin = Grid->GetCellSize() * BodyMargin;
	for (int32 At = 0; At < Way.Num() - 1;)
	{
		int32 Far = Way.Num() - 1;
		while (Far > At + 1 && !IsLineClear(Way[At], Way[Far], Margin))
		{
			--Far;
		}
		Points.Add(Way[Far]);
		At = Far;
	}
	return Points;
}

FVector FBDFreeWalk::CellPoint(const FBDCellCoord& Coord) const
{
	if (Grid == nullptr)
	{
		return FVector::ZeroVector;
	}

	// On the floor, not on the board plane: the street stands above the plane, and on the
	// plane his feet were under the asphalt walking and half his body lying down.
	FVector Point = Grid->CellToWorld(Coord);
	float GroundZ = 0.0f;
	if (TraceGround(Point, GroundZ))
	{
		Point.Z = GroundZ;
	}
	return Point;
}

bool FBDFreeWalk::TraceGround(const FVector& Point, float& OutGroundZ) const
{
	if (World == nullptr || Grid == nullptr)
	{
		return false;
	}

	const UBDWaveSettings& Settings = UBDWaveSettings::Get();
	const float PlaneZ = Grid->GetOrigin().Z;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BDFreeWalkGround), /*bTraceComplex*/ false);
	for (const AActor* Actor : Ignored)
	{
		if (Actor != nullptr)
		{
			Params.AddIgnoredActor(Actor);
		}
	}

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, FVector(Point.X, Point.Y, PlaneZ + Settings.GroundTraceDistance),
		FVector(Point.X, Point.Y, PlaneZ - Settings.GroundTraceDistance), Settings.GroundTraceChannel, Params))
	{
		OutGroundZ = Hit.ImpactPoint.Z;
		return true;
	}
	return false;
}

// Brazil Defense. The buses parked over the mouths, following them along the edge.

#include "Wave/BDBusSubsystem.h"

#include "BDLog.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Grid/BDGridSubsystem.h"
#include "Wave/BDWaveSettings.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDBusPrivate
{
	/** Seconds between two looks for a bus an anchor is missing. */
	static constexpr float RebindInterval = 1.0f;
}

bool UBDBusSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UBDBusSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBDBusSubsystem, STATGROUP_Tickables);
}

FBDCellCoord UBDBusSubsystem::CellUnder(const FVector& Location) const
{
	const UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
	if (Grid == nullptr)
	{
		return FBDCellCoord();
	}

	// The buses are parked off the board, just past the edge: pulled back onto it, the
	// cell is the border cell of the mouth they serve.
	const FBDCellCoord Cell = Grid->WorldToCellUnclamped(Location);
	return FBDCellCoord(
		FMath::Clamp(Cell.X, 0, Grid->GetSizeX() - 1),
		FMath::Clamp(Cell.Y, 0, Grid->GetSizeY() - 1));
}

FBDBus* UBDBusSubsystem::Bind(const FBDCellCoord& Anchor)
{
	UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
	const UStaticMesh* BusMesh = UBDWaveSettings::Get().BusMesh.LoadSynchronous();
	if (World == nullptr || Grid == nullptr || BusMesh == nullptr)
	{
		return nullptr;
	}

	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* Actor = *It;
		UStaticMeshComponent* Body = Actor != nullptr ? Actor->GetStaticMeshComponent() : nullptr;
		if (Body == nullptr || Body->GetStaticMesh() != BusMesh || CellUnder(Actor->GetActorLocation()) != Anchor)
		{
			continue;
		}

		// Placed in the level as scenery, so static: it has to be told it may move.
		Body->SetMobility(EComponentMobility::Movable);

		// The edge it is parked on runs along X on the top and bottom rows, along Y on the
		// side columns; its length along that edge is what the gap is measured past.
		const bool bAlongX = Anchor.Y == 0 || Anchor.Y == Grid->GetSizeY() - 1;
		const FBox Box = Actor->GetComponentsBoundingBox();
		const FVector Extent = Box.GetExtent();

		FBDBus Bus;
		Bus.Actor = Actor;
		Bus.Anchor = Anchor;
		Bus.Home = Actor->GetActorLocation();
		Bus.From = Bus.Home;
		Bus.To = Bus.Home;
		Bus.HalfLength = bAlongX ? Extent.X : Extent.Y;
		Bus.Extent = FVector2D(Extent.X, Extent.Y);
		Bus.BoundsOffset = FVector2D(Box.GetCenter() - Bus.Home);

		// A bus that streamed back in stands at home; its mouth may be elsewhere by now.
		if (const FBDCellCoord* Exit = ExitByAnchor.Find(Anchor))
		{
			Bus.To = Bus.From = SpotFor(Bus, *Exit);
			Actor->SetActorLocation(Bus.To);
		}

		UE_LOG(LogBDWave, Log, TEXT("Bus %s bound to the mouth anchored at %s (%.0f cm long along its edge)."),
			*Actor->GetName(), *Anchor.ToString(), Bus.HalfLength * 2.0f);
		return &Buses.Add(Anchor, Bus);
	}

	return nullptr;
}

FVector UBDBusSubsystem::SpotFor(const FBDBus& Bus, const FBDCellCoord& Exit) const
{
	const UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
	const float CellSize = Grid != nullptr ? Grid->GetCellSize() : 0.0f;

	// The mouth only ever slides along its edge, so this is a move along the bus's own
	// length, forwards or back. Height and heading stay as placed.
	return Bus.Home + FVector((Exit.X - Bus.Anchor.X) * CellSize, (Exit.Y - Bus.Anchor.Y) * CellSize, 0.0f);
}

bool UBDBusSubsystem::FollowMouths(const TArray<FBDSpawnPoint>& Points)
{
	const float Duration = FMath::Max(0.0f, UBDWaveSettings::Get().BusMoveSeconds);
	bool bAnyMoving = false;

	for (const FBDSpawnPoint& Point : Points)
	{
		ExitByAnchor.Add(Point.AnchorExit, Point.ExitCell);

		FBDBus* Bus = Buses.Find(Point.AnchorExit);
		if (Bus == nullptr || !Bus->Actor.IsValid())
		{
			Buses.Remove(Point.AnchorExit);
			Bus = Bind(Point.AnchorExit);
		}
		if (Bus == nullptr)
		{
			continue;
		}

		const FVector Spot = SpotFor(*Bus, Point.ExitCell);
		if (Spot.Equals(Bus->To, 1.0f))
		{
			continue;
		}

		// SOUND: the bus starts moving here - the engine, a horn before the wave.
		Bus->From = Bus->Actor->GetActorLocation();
		Bus->To = Spot;
		Bus->Elapsed = 0.0f;
		Bus->Duration = Duration;
		if (Duration <= 0.0f)
		{
			Bus->Actor->SetActorLocation(Spot);
		}
		bAnyMoving |= Duration > 0.0f;

		UE_LOG(LogBDWave, Log, TEXT("Bus %s pulls up from %s to %s over %.1fs."),
			*Bus->Actor->GetName(), *CellUnder(Bus->From).ToString(), *Point.ExitCell.ToString(), Duration);
	}

	return bAnyMoving;
}

const FBDBus* UBDBusSubsystem::FindOrBind(const FBDCellCoord& Anchor) const
{
	const FBDBus* Bus = Buses.Find(Anchor);
	if (Bus != nullptr && Bus->Actor.IsValid())
	{
		return Bus;
	}

	// Asked before the first wave bound anything: the mouths slide before the buses follow.
	UBDBusSubsystem* Self = const_cast<UBDBusSubsystem*>(this);
	Self->Buses.Remove(Anchor);
	return Self->Bind(Anchor);
}

float UBDBusSubsystem::GetGapCells(const FBDCellCoord& Anchor, const FBDCellCoord& Exit, const FBDCellCoord& OtherAnchor, const FBDCellCoord& OtherExit) const
{
	const UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
	// Both bound before either is looked up: binding adds to the map, and a pointer taken
	// into it before that may not survive.
	FindOrBind(Anchor);
	FindOrBind(OtherAnchor);
	const FBDBus* Bus = Buses.Find(Anchor);
	const FBDBus* Other = Buses.Find(OtherAnchor);
	if (Grid == nullptr || Grid->GetCellSize() <= 0.0f || Bus == nullptr || Other == nullptr || !Bus->Actor.IsValid() || !Other->Actor.IsValid())
	{
		return TNumericLimits<float>::Max();
	}

	// Two boxes on the floor, both upright to the world axes: a bus is parked along an
	// edge of the board and never turns. The gap is along whichever axis separates them;
	// overlapping on both, it is how deep they overlap, negative.
	const FVector2D Center = FVector2D(SpotFor(*Bus, Exit)) + Bus->BoundsOffset;
	const FVector2D OtherCenter = FVector2D(SpotFor(*Other, OtherExit)) + Other->BoundsOffset;
	const float GapX = FMath::Abs(Center.X - OtherCenter.X) - (Bus->Extent.X + Other->Extent.X);
	const float GapY = FMath::Abs(Center.Y - OtherCenter.Y) - (Bus->Extent.Y + Other->Extent.Y);
	return FMath::Max(GapX, GapY) / Grid->GetCellSize();
}

float UBDBusSubsystem::GetParkRemaining() const
{
	float Remaining = 0.0f;
	for (const TPair<FBDCellCoord, FBDBus>& Pair : Buses)
	{
		if (Pair.Value.Actor.IsValid() && Pair.Value.IsMoving())
		{
			Remaining = FMath::Max(Remaining, Pair.Value.Duration - Pair.Value.Elapsed);
		}
	}
	return Remaining;
}

float UBDBusSubsystem::GetMinGapCells(const TArray<FBDSpawnPoint>& Points) const
{
	float Smallest = TNumericLimits<float>::Max();
	for (int32 First = 0; First < Points.Num(); ++First)
	{
		for (int32 Second = First + 1; Second < Points.Num(); ++Second)
		{
			Smallest = FMath::Min(Smallest, GetGapCells(Points[First].AnchorExit, Points[First].ExitCell, Points[Second].AnchorExit, Points[Second].ExitCell));
		}
	}
	return Smallest;
}

void UBDBusSubsystem::CountBuses(const TArray<FBDSpawnPoint>& Points, int32& OutBound, int32& OutAstray) const
{
	OutBound = 0;
	OutAstray = 0;
	for (const FBDSpawnPoint& Point : Points)
	{
		const FBDBus* Bus = Buses.Find(Point.AnchorExit);
		if (Bus == nullptr || !Bus->Actor.IsValid())
		{
			continue;
		}
		++OutBound;
		if (CellUnder(Bus->To) != Point.ExitCell)
		{
			++OutAstray;
		}
	}
}

void UBDBusSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	for (TPair<FBDCellCoord, FBDBus>& Pair : Buses)
	{
		FBDBus& Bus = Pair.Value;
		AStaticMeshActor* Actor = Bus.Actor.Get();
		if (Actor == nullptr || !Bus.IsMoving())
		{
			continue;
		}

		// Eased at both ends, on the game clock: it pulls away, rolls and parks.
		Bus.Elapsed = FMath::Min(Bus.Duration, Bus.Elapsed + DeltaTime);
		const float Alpha = FMath::SmoothStep(0.0f, 1.0f, Bus.Elapsed / Bus.Duration);
		Actor->SetActorLocation(FMath::Lerp(Bus.From, Bus.To, Alpha));
	}

	// A bus that streamed out and back in is a new actor at its home spot: found again by
	// its anchor and put back with its mouth.
	RebindTimer -= DeltaTime;
	if (RebindTimer <= 0.0f && ExitByAnchor.Num() > 0)
	{
		RebindTimer = BDBusPrivate::RebindInterval;
		for (const TPair<FBDCellCoord, FBDCellCoord>& Pair : ExitByAnchor)
		{
			const FBDBus* Bus = Buses.Find(Pair.Key);
			if (Bus == nullptr || !Bus->Actor.IsValid())
			{
				Buses.Remove(Pair.Key);
				Bind(Pair.Key);
			}
		}
	}
}

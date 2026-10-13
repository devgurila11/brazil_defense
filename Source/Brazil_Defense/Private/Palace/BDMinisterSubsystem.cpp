// Brazil Defense. The STF and the ministers it lets out.

#include "Palace/BDMinisterSubsystem.h"

#include "BDLog.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Grid/BDFreeWalk.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Palace/BDAgent.h"
#include "Palace/BDMinister.h"
#include "Palace/BDMinisterData.h"
#include "Palace/BDMinisterSettings.h"
#include "Wave/BDWaveSubsystem.h"

UBDMinisterSubsystem* UBDMinisterSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	return World != nullptr ? World->GetSubsystem<UBDMinisterSubsystem>() : nullptr;
}

bool UBDMinisterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

const AActor* UBDMinisterSubsystem::FindStf()
{
	if (Stf.IsValid() || bStfSearched)
	{
		return Stf.Get();
	}
	bStfSearched = true;
	const UStaticMesh* Wanted = UBDMinisterSettings::Get().StfMesh.LoadSynchronous();
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It && Wanted != nullptr; ++It)
	{
		const UStaticMeshComponent* Mesh = It->GetStaticMeshComponent();
		if (Mesh != nullptr && Mesh->GetStaticMesh() == Wanted)
		{
			Stf = *It;
			UE_LOG(LogBDMinister, Log, TEXT("STF found: %s at %s."), *It->GetName(), *It->GetActorLocation().ToCompactString());
			break;
		}
	}
	if (!Stf.IsValid())
	{
		UE_LOG(LogBDMinister, Warning, TEXT("No STF in the world (an actor with %s): the ministers use the edge no mouth opens on."),
			*UBDMinisterSettings::Get().StfMesh.ToString());
	}
	return Stf.Get();
}

bool UBDMinisterSubsystem::FindDoor(FBDCellCoord& OutCell, FVector& OutOutside, const AActor*& OutStf)
{
	UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
	if (Grid == nullptr || Grid->GetSizeX() <= 0 || Grid->GetSizeY() <= 0)
	{
		return false;
	}
	OutStf = FindStf();
	const FBDFreeWalk Rules(World, Grid, { OutStf });
	const float Cell = Grid->GetCellSize();

	// The cell of the board nearest the building, or the middle of the edge no mouth opens on.
	FVector Toward;
	if (OutStf != nullptr)
	{
		Toward = OutStf->GetComponentsBoundingBox().GetCenter();
	}
	else
	{
		bool bEdges[4] = { false, false, false, false };
		if (UBDWaveSubsystem* Waves = World->GetSubsystem<UBDWaveSubsystem>())
		{
			for (const FBDSpawnPoint& Mouth : Waves->GetSpawnPoints())
			{
				bEdges[0] |= Mouth.AnchorExit.X == 0;
				bEdges[1] |= Mouth.AnchorExit.X == Grid->GetSizeX() - 1;
				bEdges[2] |= Mouth.AnchorExit.Y == 0;
				bEdges[3] |= Mouth.AnchorExit.Y == Grid->GetSizeY() - 1;
			}
		}
		const FVector Middle = Grid->CellToWorld(FBDCellCoord(Grid->GetSizeX() / 2, Grid->GetSizeY() / 2));
		const FVector Outward = !bEdges[1] ? FVector(1, 0, 0) : !bEdges[0] ? FVector(-1, 0, 0) : !bEdges[3] ? FVector(0, 1, 0) : FVector(0, -1, 0);
		Toward = Middle + Outward * Cell * (Grid->GetSizeX() + Grid->GetSizeY());
	}
	const FBDCellCoord Raw = Grid->WorldToCellUnclamped(Toward);
	const FBDCellCoord Edge(FMath::Clamp(Raw.X, 0, Grid->GetSizeX() - 1), FMath::Clamp(Raw.Y, 0, Grid->GetSizeY() - 1));

	// The nearest cell to that one a man may stand on, ring by ring.
	bool bFound = false;
	for (int32 Ring = 0; Ring < FMath::Max(Grid->GetSizeX(), Grid->GetSizeY()) && !bFound; ++Ring)
	{
		for (int32 DX = -Ring; DX <= Ring && !bFound; ++DX)
		{
			for (int32 DY = -Ring; DY <= Ring && !bFound; ++DY)
			{
				const FBDCellCoord Candidate(Edge.X + DX, Edge.Y + DY);
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) == Ring && Rules.CanStand(Candidate))
				{
					OutCell = Candidate;
					bFound = true;
				}
			}
		}
	}
	if (!bFound)
	{
		return false;
	}

	// Outside the door: against the building on the side facing the board, or a cell and a half off the edge.
	const FVector DoorPoint = Rules.CellPoint(OutCell);
	if (OutStf != nullptr)
	{
		const FBox Box = OutStf->GetComponentsBoundingBox();
		const FVector Near = Box.GetClosestPointTo(FVector(DoorPoint.X, DoorPoint.Y, Box.GetCenter().Z));
		OutOutside = FVector(Near.X, Near.Y, DoorPoint.Z) + (DoorPoint - Near).GetSafeNormal2D() * Cell * 0.5f;
	}
	else
	{
		OutOutside = DoorPoint + (DoorPoint - Grid->CellToWorld(FBDCellCoord(Grid->GetSizeX() / 2, Grid->GetSizeY() / 2))).GetSafeNormal2D() * Cell * 1.5f;
	}
	float GroundZ = 0.0f;
	if (Rules.TraceGround(OutOutside, GroundZ))
	{
		OutOutside.Z = GroundZ;
	}
	return true;
}

int32 UBDMinisterSubsystem::SpawnMinisters(const int32 Count)
{
	UWorld* World = GetWorld();
	FBDCellCoord DoorCell;
	FVector Outside;
	const AActor* Building = nullptr;
	if (World == nullptr || !FindDoor(DoorCell, Outside, Building))
	{
		UE_LOG(LogBDMinister, Error, TEXT("No STF door: no board to come out onto."));
		return 0;
	}

	// Side by side along the front of the building, a body apart.
	const UBDGridSubsystem* Grid = World->GetSubsystem<UBDGridSubsystem>();
	const FVector Front = (FBDFreeWalk(World, Grid, { Building }).CellPoint(DoorCell) - Outside).GetSafeNormal2D();
	const FVector Along = FVector::CrossProduct(FVector::UpVector, Front);
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		if (GetLine().Num() == 0)
		{
			UE_LOG(LogBDMinister, Log, TEXT("The STF is empty: all %d ministers are out."), UBDMinisterSettings::Get().MaxOnBoard);
			break;
		}
		const int32 Seat = Line[0];
		Line.RemoveAt(0);
		const float Offset = (Index - (Count - 1) * 0.5f) * Grid->GetCellSize() * 0.8f;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ABDMinister* Minister = World->SpawnActor<ABDMinister>(ABDMinister::StaticClass(), FTransform(Outside + Along * Offset), Params);
		if (Minister != nullptr)
		{
			const TArray<TSoftObjectPtr<UBDMinisterData>>& Datas = UBDMinisterSettings::Get().Ministers;
			const UBDMinisterData* Data = Datas.IsValidIndex(Seat - 1) ? Datas[Seat - 1].LoadSynchronous() : nullptr;
			Minister->InitializeMinister(Outside + Along * Offset, DoorCell, Building, Seat, Data);
			++Spawned;
		}
		else
		{
			Line.Insert(Seat, 0);
		}
	}
	return Spawned;
}

bool UBDMinisterSubsystem::NotifyCandidateKilled()
{
	const UBDMinisterSettings& Settings = UBDMinisterSettings::Get();
	if (!Settings.bReleaseOnCandidateKilled)
	{
		return false;
	}
	if (SpawnMinisters(1) == 0)
	{
		UE_LOG(LogBDMinister, Log, TEXT("Candidate down, but nobody is left in the STF."));
		return false;
	}
	++ReleasedByKills;
	UE_LOG(LogBDMinister, Log, TEXT("Candidate down: a minister comes out of the STF (%d out, %d inside, %d let out by kills)."),
		GetMinisters().Num(), GetLine().Num(), ReleasedByKills);
	return true;
}

const TArray<int32>& UBDMinisterSubsystem::GetLine()
{
	if (!bLineReady)
	{
		bLineReady = true;
		for (int32 Seat = 1; Seat <= UBDMinisterSettings::Get().MaxOnBoard; ++Seat)
		{
			Line.Add(Seat);
		}
	}
	return Line;
}

void UBDMinisterSubsystem::ReturnSeat(const int32 Seat)
{
	GetLine();
	if (Seat > 0 && !Line.Contains(Seat))
	{
		Line.Add(Seat);
		UE_LOG(LogBDMinister, Log, TEXT("Seat %d back in the STF, at the end of the line (%d inside)."), Seat, Line.Num());
	}
}

void UBDMinisterSubsystem::ResetForNewMatch()
{
	const TArray<ABDMinister*> Out = GetMinisters();
	for (ABDMinister* Minister : Out)
	{
		Minister->Destroy();
	}
	// Rebuilt rather than trusted: the seats the destroyed ones handed back went to the end.
	Line.Reset();
	bLineReady = false;
	GetLine();
	ReleasedByKills = 0;
	if (Out.Num() > 0)
	{
		UE_LOG(LogBDMinister, Log, TEXT("Board undone: %d minister(s) gone, the STF full again (%d seats)."), Out.Num(), Line.Num());
	}
}

TArray<ABDMinister*> UBDMinisterSubsystem::GetMinisters() const
{
	TArray<ABDMinister*> Out;
	for (TActorIterator<ABDMinister> It(GetWorld()); It; ++It)
	{
		if (!It->IsActorBeingDestroyed())
		{
			Out.Add(*It);
		}
	}
	return Out;
}

int32 UBDMinisterSubsystem::CountStanding() const
{
	int32 Count = 0;
	for (const ABDMinister* Minister : GetMinisters())
	{
		const EBDMinisterState State = Minister->GetState();
		Count += State != EBDMinisterState::Flying && State != EBDMinisterState::Down && State != EBDMinisterState::Fleeing ? 1 : 0;
	}
	return Count;
}

ABDMinister* UBDMinisterSubsystem::FindKickable(const FVector& From, const float Radius) const
{
	ABDMinister* Nearest = nullptr;
	float NearestSquared = FMath::Square(Radius);
	for (ABDMinister* Minister : GetMinisters())
	{
		const float DistanceSquared = FVector::DistSquared2D(From, Minister->GetActorLocation());
		if (Minister->CanBeKicked() && DistanceSquared <= NearestSquared)
		{
			Nearest = Minister;
			NearestSquared = DistanceSquared;
		}
	}
	return Nearest;
}

//~ Console -----------------------------------------------------------------------

namespace BDMinisterCommands
{
	static void ExecSpawn(const TArray<FString>& Args, UWorld* World)
	{
		UBDMinisterSubsystem* Ministers = UBDMinisterSubsystem::Get(World);
		if (Ministers == nullptr)
		{
			UE_LOG(LogBDMinister, Error, TEXT("BD.Minister.Spawn needs a game world."));
			return;
		}
		const int32 Count = FMath::Clamp(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1, 1, 11);
		const int32 Before = Ministers->GetMinisters().Num();
		const int32 Spawned = Ministers->SpawnMinisters(Count);

		// "agent": the new ones straight in front of the Agent, to see the chase and the kick at once.
		if (Args.Num() > 1 && Args[1].Equals(TEXT("agent"), ESearchCase::IgnoreCase))
		{
			TActorIterator<ABDAgent> Agent(World);
			const UBDGridSubsystem* Grid = World->GetSubsystem<UBDGridSubsystem>();
			const TArray<ABDMinister*> All = Ministers->GetMinisters();
			for (int32 Index = Before; Agent && Grid != nullptr && Index < All.Num(); ++Index)
			{
				const FVector Ahead = Agent->GetActorLocation() + Agent->GetActorForwardVector() * Grid->GetCellSize() * (2.0f + Index - Before);
				All[Index]->DebugPlaceOnBoard(Ahead);
			}
		}
		UE_LOG(LogBDMinister, Log, TEXT("BD.Minister.Spawn: %d of %d out of the STF."), Spawned, Count);
	}

	static void ExecStatus(const TArray<FString>& Args, UWorld* World)
	{
		UBDMinisterSubsystem* Ministers = UBDMinisterSubsystem::Get(World);
		if (Ministers == nullptr)
		{
			UE_LOG(LogBDMinister, Error, TEXT("BD.Minister.Status needs a game world."));
			return;
		}
		FBDCellCoord Door;
		FVector Outside;
		const AActor* Building = nullptr;
		const bool bDoor = Ministers->FindDoor(Door, Outside, Building);
		const TArray<ABDMinister*> All = Ministers->GetMinisters();
		UE_LOG(LogBDMinister, Log, TEXT("BD.Minister.Status: %d minister(s) out; STF %s, door %s, outside it at %s."), All.Num(),
			Building != nullptr ? *Building->GetName() : TEXT("not in the world"), bDoor ? *Door.ToString() : TEXT("none"), *Outside.ToCompactString());
		for (const ABDMinister* Minister : All)
		{
			UE_LOG(LogBDMinister, Log, TEXT("  %s"), *Minister->Describe());
		}
	}

	static void ExecClear(const TArray<FString>& Args, UWorld* World)
	{
		UBDMinisterSubsystem* Ministers = UBDMinisterSubsystem::Get(World);
		int32 Removed = 0;
		for (ABDMinister* Minister : Ministers != nullptr ? Ministers->GetMinisters() : TArray<ABDMinister*>())
		{
			Minister->Destroy();
			++Removed;
		}
		UE_LOG(LogBDMinister, Log, TEXT("BD.Minister.Clear: %d minister(s) sent home."), Removed);
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSpawn(
		TEXT("BD.Minister.Spawn"),
		TEXT("BD.Minister.Spawn [n=1] [agent]: lets n ministers out of the STF; 'agent' puts them straight in front of the Agent."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSpawn));

	static FAutoConsoleCommandWithWorldAndArgs CmdStatus(
		TEXT("BD.Minister.Status"),
		TEXT("BD.Minister.Status: the STF, its door, and every minister out with what he is doing."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));

	static FAutoConsoleCommandWithWorldAndArgs CmdClear(
		TEXT("BD.Minister.Clear"),
		TEXT("BD.Minister.Clear: every minister out is gone at once."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecClear));
}

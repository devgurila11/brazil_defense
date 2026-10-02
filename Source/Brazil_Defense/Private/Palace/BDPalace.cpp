// Brazil Defense. The Palácio do Governo standing on the board.

#include "Palace/BDPalace.h"

#include "BDLog.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Palace/BDAgent.h"
#include "Palace/BDPalaceData.h"

namespace BDPalacePrivate
{
	/** The placeholder block while the data has no mesh of its own. */
	static const TCHAR* const PlaceholderMeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
}

ABDPalace::ABDPalace()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// The cursor's traces see the building; nothing that walks does.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetCanEverAffectNavigation(false);
}

void ABDPalace::InitializePalace(const UBDPalaceData* InData, const FVector2D& FootprintSize)
{
	Data = InData;
	PalaceLevel = InData != nullptr ? FMath::Clamp(InData->Level, 0, UBDPalaceData::MaxLevel) : 0;

	// One Agent per palace, out of the door as it goes up.
	UWorld* World = GetWorld();
	if (InData != nullptr && World != nullptr && World->IsGameWorld() && !Agent.IsValid())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.Owner = this;
		if (ABDAgent* Spawned = World->SpawnActor<ABDAgent>(ABDAgent::StaticClass(), GetActorTransform(), SpawnParams))
		{
			Agent = Spawned;
			Spawned->InitializeAgent(this);
		}
	}

	UStaticMesh* LoadedMesh = InData != nullptr ? InData->Mesh.LoadSynchronous() : nullptr;
	if (LoadedMesh == nullptr)
	{
		LoadedMesh = LoadObject<UStaticMesh>(nullptr, BDPalacePrivate::PlaceholderMeshPath);
	}
	Mesh->SetStaticMesh(LoadedMesh);
	if (LoadedMesh == nullptr)
	{
		UE_LOG(LogBDTower, Warning, TEXT("Palace '%s' has no mesh and the placeholder did not load."), *GetName());
		return;
	}

	const bool bFit = InData == nullptr || InData->bFitToFootprint;
	const FVector Extra = InData != nullptr ? InData->MeshScale : FVector::OneVector;
	const FBoxSphereBounds Bounds = LoadedMesh->GetBounds();
	const FVector Extent = Bounds.BoxExtent.ComponentMax(FVector(1.0f));

	FVector Scale = Extra;
	if (bFit)
	{
		// Uniform, so a real model keeps its proportions: the widest side spans the footprint.
		const float Fill = InData != nullptr ? InData->FootprintFill : 0.9f;
		const float Uniform = Fill * FMath::Min(FootprintSize.X / (2.0f * Extent.X), FootprintSize.Y / (2.0f * Extent.Y));
		Scale *= Uniform;
	}
	Mesh->SetRelativeScale3D(Scale);

	// Centered on the footprint and standing on the floor, wherever the mesh has its pivot.
	// Taken as authored when not fitted: the pivot is the ground point.
	const FVector Offset = bFit
		? FVector(-Bounds.Origin.X * Scale.X, -Bounds.Origin.Y * Scale.Y, -(Bounds.Origin.Z - Extent.Z) * Scale.Z)
		: FVector::ZeroVector;
	Mesh->SetRelativeLocation(Offset);

	UE_LOG(LogBDTower, Log, TEXT("Palace '%s' up from '%s': mesh %s scaled %s over %.0fx%.0f cm, level %d of %d."),
		*GetName(), *GetNameSafe(InData), *GetNameSafe(LoadedMesh), *Scale.ToCompactString(),
		FootprintSize.X, FootprintSize.Y, PalaceLevel, UBDPalaceData::MaxLevel);
}

void ABDPalace::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Sold, cleared or the match over: his Agent goes with him.
	if (ABDAgent* Gone = Agent.Get())
	{
		Gone->Destroy();
	}
	Agent.Reset();

	Super::EndPlay(EndPlayReason);
}

void ABDPalace::SetPalaceLevel(const int32 NewLevel)
{
	PalaceLevel = FMath::Clamp(NewLevel, 0, UBDPalaceData::MaxLevel);
}

FVector ABDPalace::GetStarsAnchor() const
{
	const FBoxSphereBounds Bounds = Mesh->Bounds;
	const float Top = Mesh->GetStaticMesh() != nullptr ? Bounds.Origin.Z + Bounds.BoxExtent.Z : GetActorLocation().Z;
	const float Lift = Data != nullptr ? Data->StarsLift : 80.0f;
	return FVector(GetActorLocation().X, GetActorLocation().Y, Top + Lift);
}

namespace BDPalaceDebug
{
	static void ExecStatus(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogBDTower, Error, TEXT("This command needs a world."));
			return;
		}

		int32 Count = 0;
		for (TActorIterator<ABDPalace> It(World); It; ++It)
		{
			UE_LOG(LogBDTower, Log, TEXT("  %s (%s) at %s: level %d of %d, stars at %s."), *It->GetName(), *GetNameSafe(It->GetData()),
				*It->GetActorLocation().ToCompactString(), It->GetPalaceLevel(), UBDPalaceData::MaxLevel, *It->GetStarsAnchor().ToCompactString());
			++Count;
		}
		UE_LOG(LogBDTower, Log, TEXT("BD.Palace.Status: %d palace(s) on the board."), Count);
	}

	static void ExecSetLevel(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || Args.Num() < 1)
		{
			UE_LOG(LogBDTower, Error, TEXT("Usage: BD.Palace.SetLevel <0-5>. Sets every palace on the board; no effect on play yet."));
			return;
		}

		const int32 Level = FCString::Atoi(*Args[0]);
		int32 Count = 0;
		for (TActorIterator<ABDPalace> It(World); It; ++It)
		{
			It->SetPalaceLevel(Level);
			++Count;
		}
		UE_LOG(LogBDTower, Log, TEXT("BD.Palace.SetLevel: %d palace(s) now show %d star(s)."), Count, FMath::Clamp(Level, 0, UBDPalaceData::MaxLevel));
	}

	static FAutoConsoleCommandWithWorldAndArgs StatusCommand(
		TEXT("BD.Palace.Status"),
		TEXT("Lists every palace on the board with its level."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));

	static FAutoConsoleCommandWithWorldAndArgs SetLevelCommand(
		TEXT("BD.Palace.SetLevel"),
		TEXT("BD.Palace.SetLevel <0-5>: fills that many stars over every palace. Visual only, until the Agent exists."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSetLevel));
}

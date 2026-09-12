// Brazil Defense. A full defense built on Play, so a test never starts from an empty board.

#include "Debug/BDDebugAutoSetup.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "BDLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Match/BDMatchManager.h"
#include "Math/RandomStream.h"
#include "Misc/CommandLine.h"
#include "Objective/BDObjectiveSettings.h"
#include "Objective/BDObjectiveSubsystem.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementComponent.h"
#include "Platform/BDPlatformComponent.h"
#include "TimerManager.h"
#include "Tower/BDTowerBase.h"
#include "Tower/BDTowerData.h"
#include "UObject/UObjectIterator.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDAutoSetupPrivate
{
	/** On by default while the game is being balanced: every Play starts with a defense to look at. */
	static int32 GAutoSetup = 1;
	static int32 GAutoSetupSeed = 0;
	static int32 GAutoSetupLevel = 1;

	static FAutoConsoleVariableRef CVarAutoSetup(
		TEXT("BD.Debug.AutoSetup"),
		GAutoSetup,
		TEXT("1 builds a full random defense when a game world starts. 0 to start from an empty board."),
		ECVF_Cheat);

	static FAutoConsoleVariableRef CVarAutoSetupSeed(
		TEXT("BD.Debug.AutoSetup.Seed"),
		GAutoSetupSeed,
		TEXT("Seed of the automatic defense. 0 draws a new one every Play; the seed used is logged."),
		ECVF_Cheat);

	static FAutoConsoleVariableRef CVarAutoSetupLevel(
		TEXT("BD.Debug.AutoSetup.Level"),
		GAutoSetupLevel,
		TEXT("Level every defender of the automatic defense starts at, for nothing: a full level 3 defense against a level 1 one."),
		ECVF_Cheat);

	/** Switches a balancing session wants on, flipped when the setup runs. */
	static const TCHAR* const DebugSwitchesOn[] = {
		TEXT("BD.Match.FreezeTimer"), TEXT("BD.Day.Freeze"), TEXT("BD.Grid.Debug"),
		TEXT("BD.Tower.ShowRange"), TEXT("BD.Tower.ShowTarget") };

	/** How many fences the setup lays at most, budget allowing: enough to bend the routes, not to seal them. */
	static constexpr int32 MaxFences = 10;
	static constexpr int32 FenceAttemptsPerFence = 4;

	/** Rings of cells around a target tried for a piece, nearest first. */
	static constexpr int32 SearchRadius = 2;

	/** Fraction of a stretch a target may drift from its middle, so two seeds never line up the same. */
	static constexpr float StretchJitter = 0.3f;

	static void Shuffle(TArray<FBDCellCoord>& Cells, FRandomStream& Stream)
	{
		for (int32 Index = Cells.Num() - 1; Index > 0; --Index)
		{
			Cells.Swap(Index, Stream.RandRange(0, Index));
		}
	}

	template <typename T>
	static T* Pick(const TArray<TObjectPtr<T>>& From, FRandomStream& Stream)
	{
		return From.Num() > 0 ? From[Stream.RandRange(0, From.Num() - 1)].Get() : nullptr;
	}
}

bool UBDDebugAutoSetup::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UBDDebugAutoSetup::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// The command line wins over the console variable, so a headless run can pin both.
	int32 Enabled = BDAutoSetupPrivate::GAutoSetup;
	FParse::Value(FCommandLine::Get(), TEXT("BDAutoSetup="), Enabled);
	int32 Seed = BDAutoSetupPrivate::GAutoSetupSeed;
	FParse::Value(FCommandLine::Get(), TEXT("BDAutoSetupSeed="), Seed);
	int32 Level = BDAutoSetupPrivate::GAutoSetupLevel;
	FParse::Value(FCommandLine::Get(), TEXT("BDAutoSetupLevel="), Level);

	if (Enabled == 0)
	{
		return;
	}

	// One tick later: the match manager and the player controller are spawned during
	// BeginPlay, and the setup needs both standing.
	InWorld.GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Seed, Level]()
	{
		Run(Seed, Level);
	}));
}

UBDPlacementComponent* UBDDebugAutoSetup::FindPlacement() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APlayerController* Controller = It->Get())
		{
			if (UBDPlacementComponent* Placement = Controller->FindComponentByClass<UBDPlacementComponent>())
			{
				return Placement;
			}
		}
	}

	return nullptr;
}

ABDMatchManager* UBDDebugAutoSetup::FindMatch() const
{
	return ABDMatchManager::Get(GetWorld());
}

UBDWaveSubsystem* UBDDebugAutoSetup::FindWaves() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr;
}

void UBDDebugAutoSetup::GatherPlaceables()
{
	ObjectivePieces.Reset();
	PlatformPieces.Reset();
	CharacterPieces.Reset();
	TowerPieces.Reset();
	DividerPieces.Reset();

	const IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByClass(UBDPlaceableData::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses*/ true);

	for (const FAssetData& Asset : Assets)
	{
		UBDPlaceableData* Piece = Cast<UBDPlaceableData>(Asset.GetAsset());
		if (Piece == nullptr)
		{
			continue;
		}

		switch (Piece->GetPieceKind())
		{
		case EBDPieceKind::Objective: ObjectivePieces.Add(Piece); break;
		case EBDPieceKind::Platform: PlatformPieces.Add(Piece); break;
		case EBDPieceKind::Character: CharacterPieces.Add(Piece); break;
		case EBDPieceKind::Tower: TowerPieces.Add(Piece); break;
		case EBDPieceKind::Divider: DividerPieces.Add(Piece); break;
		default: break;
		}
	}

	// Stable order whatever the registry returns, so a seed always means the same board.
	auto ByName = [](const UBDPlaceableData& A, const UBDPlaceableData& B) { return A.GetName() < B.GetName(); };
	ObjectivePieces.Sort(ByName);
	PlatformPieces.Sort(ByName);
	CharacterPieces.Sort(ByName);
	TowerPieces.Sort(ByName);
	DividerPieces.Sort(ByName);
}

void UBDDebugAutoSetup::ApplyDebugSwitches()
{
	for (const TCHAR* Name : BDAutoSetupPrivate::DebugSwitchesOn)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Variable->Set(1, ECVF_SetByCode);
		}
	}
}

void UBDDebugAutoSetup::Run(const int32 Seed, const int32 DefenderLevel)
{
	UBDPlacementComponent* Placement = FindPlacement();
	ABDMatchManager* Match = FindMatch();
	if (Placement == nullptr || Match == nullptr)
	{
		UE_LOG(LogBDDebug, Error, TEXT("Auto setup needs a player controller with a placement component and a match manager."));
		return;
	}

	// A board that already holds its urn is a board already set up: building on top of it
	// would only fail on the budgets. ClearAll is the way to start over.
	const UBDObjectiveSubsystem* Objectives = UBDObjectiveSubsystem::Get(GetWorld());
	if (Objectives != nullptr && Objectives->IsPlaced() && Match->GetBudgetRemaining(EBDPieceKind::Objective) <= 0)
	{
		UE_LOG(LogBDDebug, Warning, TEXT("Auto setup skipped: the board is already set up. BD.Debug.ClearAll first."));
		return;
	}

	LastSeed = Seed != 0 ? Seed : FMath::Max(1, FMath::Rand());
	FRandomStream Stream(LastSeed);

	ApplyDebugSwitches();
	GatherPlaceables();

	// The obstacles are part of the scenario: one number has to bring the whole board
	// back, so the match's own random obstacle seed is replaced by this one.
	Match->DebugRegenerateObstacles(LastSeed);

	UE_LOG(LogBDDebug, Log, TEXT("Auto setup, seed %d. Repeat it with BD.Debug.AutoSetup.Seed %d (or -BDAutoSetupSeed=%d)."),
		LastSeed, LastSeed, LastSeed);

	// Hundreds of spots are tried; one line per answer would drown the log.
	Placement->SetRefusalLogging(false);

	const bool bUrn = PlaceObjective(*Placement, Stream);
	const int32 Platforms = bUrn ? PlacePlatforms(*Placement, Stream) : 0;
	const int32 Characters = bUrn ? FillSlots(*Placement, Stream) : 0;
	const int32 Towers = bUrn ? PlaceTowers(*Placement, Stream) : 0;
	const int32 Fences = bUrn ? PlaceFences(*Placement, Stream) : 0;

	Placement->CancelSelection();
	Placement->SetRefusalLogging(true);

	// Every defender at the requested level, for nothing: the question is where a whole
	// defense of that level breaks, not whether it could have been paid for.
	const int32 Level = FMath::Clamp(DefenderLevel, 1, UBDTowerData::MaxLevels);
	for (TActorIterator<ABDTowerBase> It(GetWorld()); It; ++It)
	{
		It->DebugSetLevel(Level);
	}

	UE_LOG(LogBDDebug, Log, TEXT("Auto setup done: urn %s, %d platform(s), %d character(s), %d tower(s), %d fence(s), all defenders at level %d. Budgets left: %d platforms, %d characters, %d towers, %d dividers."),
		bUrn ? TEXT("placed") : TEXT("NOT placed"), Platforms, Characters, Towers, Fences, Level,
		Match->GetPlatformsRemaining(), Match->GetCharactersRemaining(), Match->GetTowersRemaining(), Match->GetDividersRemaining());
}

void UBDDebugAutoSetup::ClearAll()
{
	if (UBDWaveSubsystem* Waves = FindWaves())
	{
		Waves->DespawnAll();
	}

	if (UBDPlacementComponent* Placement = FindPlacement())
	{
		Placement->DebugRemoveAll();
	}

	if (UBDObjectiveSubsystem* Objectives = UBDObjectiveSubsystem::Get(GetWorld()))
	{
		Objectives->ClearObjective();
	}

	if (ABDMatchManager* Match = FindMatch())
	{
		Match->DebugResetBudgets();
	}

	UE_LOG(LogBDDebug, Log, TEXT("Board cleared. BD.Debug.AutoSetup.Run builds a new defense; the wave counter was left alone."));
}

//~ The pieces ---------------------------------------------------------------------

bool UBDDebugAutoSetup::PlaceObjective(UBDPlacementComponent& Placement, FRandomStream& Stream)
{
	UBDPlaceableData* Piece = BDAutoSetupPrivate::Pick(ObjectivePieces, Stream);
	if (Piece == nullptr)
	{
		UE_LOG(LogBDDebug, Error, TEXT("Auto setup found no objective placeable (a UBDPlaceableData with OccupiesAs = Goal)."));
		return false;
	}

	TArray<FBDCellCoord> Zone;
	UBDObjectiveSettings::Get().GetZoneCells(Zone);
	BDAutoSetupPrivate::Shuffle(Zone, Stream);

	Placement.SelectPlaceable(Piece);
	for (const FBDCellCoord& Cell : Zone)
	{
		Placement.SetHoveredCellDirect(Cell);
		if (Placement.IsCurrentPlacementValid() && Placement.TryPlaceAtHovered())
		{
			return true;
		}
	}

	UE_LOG(LogBDDebug, Error, TEXT("Auto setup could not place the urn anywhere in its zone."));
	return false;
}

void UBDDebugAutoSetup::BuildStretchTargets(const int32 Count, FRandomStream& Stream, TArray<FBDCellCoord>& OutTargets) const
{
	OutTargets.Reset();

	UBDWaveSubsystem* Waves = FindWaves();
	if (Waves == nullptr || Count <= 0)
	{
		return;
	}

	// Routes with at least a cell between spawn and urn to stand next to.
	TArray<const TArray<FBDCellCoord>*> Routes;
	for (const FBDSpawnPoint& Point : Waves->GetSpawnPoints())
	{
		if (Point.Route.Num() >= 3)
		{
			Routes.Add(&Point.Route);
		}
	}
	if (Routes.Num() == 0)
	{
		return;
	}

	// Round robin over the routes, then each route is cut into as many stretches as it
	// got pieces and each piece takes the middle of its stretch, give or take.
	TArray<int32> PerRoute;
	PerRoute.Init(0, Routes.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		++PerRoute[Index % Routes.Num()];
	}

	for (int32 RouteIndex = 0; RouteIndex < Routes.Num(); ++RouteIndex)
	{
		const TArray<FBDCellCoord>& Route = *Routes[RouteIndex];
		const int32 Stretches = PerRoute[RouteIndex];
		for (int32 Stretch = 0; Stretch < Stretches; ++Stretch)
		{
			const float Fraction = (Stretch + 0.5f + Stream.FRandRange(-BDAutoSetupPrivate::StretchJitter, BDAutoSetupPrivate::StretchJitter)) / Stretches;
			const int32 CellIndex = FMath::Clamp(FMath::RoundToInt(Fraction * (Route.Num() - 1)), 1, Route.Num() - 2);
			OutTargets.Add(Route[CellIndex]);
		}
	}

	// Interleaved by route already; a shuffle keeps two seeds from placing in the same order.
	BDAutoSetupPrivate::Shuffle(OutTargets, Stream);
}

bool UBDDebugAutoSetup::TryPlaceCellPieceNear(UBDPlacementComponent& Placement, UBDPlaceableData* Piece,
	const FBDCellCoord& Target, FRandomStream& Stream) const
{
	if (Piece == nullptr)
	{
		return false;
	}

	Placement.SelectPlaceable(Piece);

	// Next to the route first, on it last: a piece on the route reroutes the creeps,
	// which is fine, but a piece beside it is what "along the route" means.
	TArray<FBDCellCoord> Candidates;
	for (int32 Radius = 1; Radius <= BDAutoSetupPrivate::SearchRadius; ++Radius)
	{
		TArray<FBDCellCoord> Ring;
		for (int32 DY = -Radius; DY <= Radius; ++DY)
		{
			for (int32 DX = -Radius; DX <= Radius; ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) == Radius)
				{
					Ring.Emplace(Target.X + DX, Target.Y + DY);
				}
			}
		}
		BDAutoSetupPrivate::Shuffle(Ring, Stream);
		Candidates.Append(Ring);
	}
	Candidates.Add(Target);

	TArray<int32> Rotations = { 0, 1, 2, 3 };
	for (int32 Index = Rotations.Num() - 1; Index > 0; --Index)
	{
		Rotations.Swap(Index, Stream.RandRange(0, Index));
	}

	for (const FBDCellCoord& Cell : Candidates)
	{
		for (const int32 Rotation : Rotations)
		{
			Placement.SetRotationSteps(Rotation);
			Placement.SetHoveredCellDirect(Cell);
			if (Placement.IsCurrentPlacementValid() && Placement.TryPlaceAtHovered())
			{
				UE_LOG(LogBDDebug, Verbose, TEXT("  %s at %s (rotation %d) for target %s."),
					*Piece->GetName(), *Cell.ToString(), Rotation, *Target.ToString());
				return true;
			}
		}
	}

	return false;
}

int32 UBDDebugAutoSetup::PlacePlatforms(UBDPlacementComponent& Placement, FRandomStream& Stream)
{
	const ABDMatchManager* Match = FindMatch();
	if (Match == nullptr || PlatformPieces.Num() == 0)
	{
		return 0;
	}

	TArray<FBDCellCoord> Targets;
	BuildStretchTargets(Match->GetBudgetRemaining(EBDPieceKind::Platform), Stream, Targets);

	int32 Placed = 0;
	for (const FBDCellCoord& Target : Targets)
	{
		// A random platform first; when it does not fit, every other one in turn, so a
		// tight spot still gets the smallest platform rather than nothing.
		const int32 First = Stream.RandRange(0, PlatformPieces.Num() - 1);
		for (int32 Offset = 0; Offset < PlatformPieces.Num(); ++Offset)
		{
			if (TryPlaceCellPieceNear(Placement, PlatformPieces[(First + Offset) % PlatformPieces.Num()], Target, Stream))
			{
				++Placed;
				break;
			}
		}
	}

	return Placed;
}

int32 UBDDebugAutoSetup::FillSlots(UBDPlacementComponent& Placement, FRandomStream& Stream)
{
	const UWorld* World = GetWorld();
	const ABDMatchManager* Match = FindMatch();
	if (World == nullptr || Match == nullptr || CharacterPieces.Num() == 0)
	{
		return 0;
	}

	// Every platform on the board, placed just now or authored in the level.
	TArray<UBDPlatformComponent*> Platforms;
	for (TObjectIterator<UBDPlatformComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && It->bInsideBattleArea && IsValid(It->GetOwner()))
		{
			Platforms.Add(*It);
		}
	}
	Platforms.Sort([](const UBDPlatformComponent& A, const UBDPlatformComponent& B)
	{
		return A.GetOwner()->GetName() < B.GetOwner()->GetName();
	});

	int32 Placed = 0;
	for (UBDPlatformComponent* Platform : Platforms)
	{
		for (int32 Slot = 0; Slot < Platform->Slots.Num(); ++Slot)
		{
			if (Match->GetBudgetRemaining(EBDPieceKind::Character) <= 0)
			{
				return Placed;
			}
			if (!Platform->IsSlotFree(Slot))
			{
				continue;
			}

			Placement.SelectPlaceable(BDAutoSetupPrivate::Pick(CharacterPieces, Stream));
			Placement.SetHoveredSlotDirect(Platform, Slot);
			if (Placement.IsCurrentPlacementValid() && Placement.TryPlaceAtHovered())
			{
				++Placed;
			}
		}
	}

	return Placed;
}

int32 UBDDebugAutoSetup::PlaceTowers(UBDPlacementComponent& Placement, FRandomStream& Stream)
{
	const ABDMatchManager* Match = FindMatch();
	if (Match == nullptr || TowerPieces.Num() == 0)
	{
		return 0;
	}

	TArray<FBDCellCoord> Targets;
	BuildStretchTargets(Match->GetBudgetRemaining(EBDPieceKind::Tower), Stream, Targets);

	int32 Placed = 0;
	for (const FBDCellCoord& Target : Targets)
	{
		if (TryPlaceCellPieceNear(Placement, BDAutoSetupPrivate::Pick(TowerPieces, Stream), Target, Stream))
		{
			++Placed;
		}
	}

	return Placed;
}

int32 UBDDebugAutoSetup::PlaceFences(UBDPlacementComponent& Placement, FRandomStream& Stream)
{
	UBDWaveSubsystem* Waves = FindWaves();
	const ABDMatchManager* Match = FindMatch();
	UBDPlaceableData* Piece = BDAutoSetupPrivate::Pick(DividerPieces, Stream);
	if (Waves == nullptr || Match == nullptr || Piece == nullptr)
	{
		return 0;
	}

	const int32 Wanted = FMath::Min(BDAutoSetupPrivate::MaxFences, Match->GetBudgetRemaining(EBDPieceKind::Divider));
	Placement.SelectPlaceable(Piece);

	// Each fence goes across a route, so the creeps have to bend around it: the routes
	// are asked again after every fence, since every fence moves them.
	int32 Placed = 0;
	for (int32 Attempt = 0; Attempt < Wanted * BDAutoSetupPrivate::FenceAttemptsPerFence && Placed < Wanted; ++Attempt)
	{
		TArray<const TArray<FBDCellCoord>*> Routes;
		for (const FBDSpawnPoint& Point : Waves->GetSpawnPoints())
		{
			if (Point.Route.Num() >= 6)
			{
				Routes.Add(&Point.Route);
			}
		}
		if (Routes.Num() == 0)
		{
			break;
		}

		const TArray<FBDCellCoord>& Route = *Routes[Stream.RandRange(0, Routes.Num() - 1)];
		const int32 Index = Stream.RandRange(2, Route.Num() - 4);

		bool bAdjacent = false;
		const FBDEdgeCoord Edge = FBDEdgeCoord::Between(Route[Index], Route[Index + 1], bAdjacent);
		if (!bAdjacent)
		{
			continue;
		}

		// Unrotated, the fence runs along X and blocks +Y edges; one turn blocks +X edges.
		Placement.SetRotationSteps(Edge.Direction == FBDEdgeCoord::DirectionX ? 1 : 0);
		Placement.SetHoveredEdgeDirect(Edge);
		if (Placement.IsCurrentPlacementValid() && Placement.TryPlaceAtHovered())
		{
			++Placed;
			UE_LOG(LogBDDebug, Verbose, TEXT("  fence across %s."), *Edge.ToString());
		}
	}

	return Placed;
}

//~ Console -----------------------------------------------------------------------

namespace BDAutoSetupCommands
{
	static UBDDebugAutoSetup* Find(const UWorld* World)
	{
		UBDDebugAutoSetup* Setup = World != nullptr ? World->GetSubsystem<UBDDebugAutoSetup>() : nullptr;
		if (Setup == nullptr)
		{
			UE_LOG(LogBDDebug, Error, TEXT("No auto setup in this world: it only exists in game worlds."));
		}
		return Setup;
	}

	static void ExecRun(const TArray<FString>& Args, UWorld* World)
	{
		if (UBDDebugAutoSetup* Setup = Find(World))
		{
			Setup->Run(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : BDAutoSetupPrivate::GAutoSetupSeed,
				Args.Num() > 1 ? FCString::Atoi(*Args[1]) : BDAutoSetupPrivate::GAutoSetupLevel);
		}
	}

	static void ExecClearAll(const TArray<FString>& Args, UWorld* World)
	{
		if (UBDDebugAutoSetup* Setup = Find(World))
		{
			Setup->ClearAll();
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdRun(
		TEXT("BD.Debug.AutoSetup.Run"),
		TEXT("BD.Debug.AutoSetup.Run [seed] [level]: builds the automatic defense now, every defender at that level."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecRun));

	static FAutoConsoleCommandWithWorldAndArgs CmdClearAll(
		TEXT("BD.Debug.ClearAll"),
		TEXT("BD.Debug.ClearAll: removes every piece and creep and hands the budgets back."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecClearAll));
}

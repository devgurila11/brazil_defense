// Brazil Defense. A full defense built on Play, so a test never starts from an empty board.

#include "Debug/BDDebugAutoSetup.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "BDLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Match/BDMatchManager.h"
#include "Player/BDGameMode.h"
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
	/** Off by default: the real flow starts from an empty board. A balancing session switches it on. */
	static int32 GAutoSetup = 0;
	static int32 GAutoSetupSeed = 0;
	static int32 GAutoSetupLevel = 1;

	static FAutoConsoleVariableRef CVarAutoSetup(
		TEXT("BD.Debug.AutoSetup"),
		GAutoSetup,
		TEXT("1 builds a full random defense when a game world starts; also switched on by -BDAutoSetup=1 or -BDAutoSetupSeed=N on the command line. 0 (default) starts from an empty board."),
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
		TEXT("BD.Tower.ShowRange"), TEXT("BD.Tower.ShowTarget"), TEXT("BD.HUD.Debug") };

	//~ Strategy, exposed so one setup can play like a good player and another like a
	// careless one, and the gap between them is what the difficulty range really is.

	/**
	 * Share of the divider hand the maze uses each time it is built: the opening one, and
	 * every pass as the hand grows. Not all of it by default: on seed 101 the whole opening
	 * hand bent the candidate's route away from the defense and lost wave 6, where three
	 * quarters held to wave 30 - a player keeps some back to answer the board they see.
	 */
	static float GFenceShare = 0.75f;

	/** Cells between two fences of the serpentine. Small is a tighter maze and a longer walk. */
	static int32 GMazeStride = 3;

	/** Sides of the urn to fence, of its four. 3 leaves one door; 0 leaves the urn open. */
	static int32 GUrnRing = 3;

	/**
	 * How the capital left after the maze is split between the two kinds of defender.
	 * Nothing counts defenders any more, so these two numbers are what decides the shape
	 * of the automatic defense, and without them it has none: the characters would fill
	 * every slot the platforms opened and leave the ground bare, because they are asked
	 * first. The tower share is of what the characters did not take, so 1 spends the rest.
	 */
	static float GCharacterShare = 0.5f;
	static float GTowerShare = 1.0f;

	/**
	 * Share of the money left after the maze spent on platforms, and only while every slot
	 * standing is manned: a platform costs like a defender now, and a board of empty trucks
	 * is money that shoots at nothing.
	 */
	static float GPlatformShare = 0.25f;

	/** Whether the passes between waves lay the dividers the waves and the bosses grant. */
	static int32 GGrowMaze = 1;

	/** 0 spreads the defense along the whole corridor, 1 piles it at the urn; between is a mix. */
	static float GFocusUrn = 0.35f;

	/** Route cells within this many cells of a spot count towards its score. Roughly a tower's reach. */
	static constexpr int32 CorridorRadius = 3;

	static FAutoConsoleVariableRef CVarFenceShare(
		TEXT("BD.Debug.AutoSetup.FenceShare"),
		GFenceShare,
		TEXT("Share of the divider hand the automatic defense lays each time it builds the maze. 0.75 (default) keeps a quarter back; 0 builds no maze."),
		ECVF_Cheat);

	static FAutoConsoleVariableRef CVarMazeStride(
		TEXT("BD.Debug.AutoSetup.MazeStride"),
		GMazeStride,
		TEXT("Cells between two fences of the serpentine: lower is a denser maze and a longer route."),
		ECVF_Cheat);

	static FAutoConsoleVariableRef CVarUrnRing(
		TEXT("BD.Debug.AutoSetup.UrnRing"),
		GUrnRing,
		TEXT("How many of the urn's four sides to fence. 3 leaves a single door; 0 leaves it open."),
		ECVF_Cheat);

	static FAutoConsoleVariableRef CVarCharacterShare(
		TEXT("BD.Debug.AutoSetup.CharacterShare"),
		GCharacterShare,
		TEXT("Share of the capital left after the maze that the automatic defense spends manning platform slots. The rest is left for the ground towers."),
		ECVF_Cheat);

	static FAutoConsoleVariableRef CVarTowerShare(
		TEXT("BD.Debug.AutoSetup.TowerShare"),
		GTowerShare,
		TEXT("Share of the capital left after the maze and the characters that the automatic defense spends on ground towers. 1 (default) spends all of it."),
		ECVF_Cheat);

	static FAutoConsoleVariableRef CVarGrowMaze(
		TEXT("BD.Debug.AutoSetup.GrowMaze"),
		GGrowMaze,
		TEXT("1 (default) lays the granted dividers between waves; 0 keeps the opening maze as it was built."),
		ECVF_Cheat);

	static FAutoConsoleVariableRef CVarPlatformShare(
		TEXT("BD.Debug.AutoSetup.PlatformShare"),
		GPlatformShare,
		TEXT("Share of the money in hand the automatic defense spends on platforms, and only while no slot stands empty."),
		ECVF_Cheat);

	static FAutoConsoleVariableRef CVarFocusUrn(
		TEXT("BD.Debug.AutoSetup.FocusUrn"),
		GFocusUrn,
		TEXT("0 spreads the defense along the corridor, 1 piles it around the urn."),
		ECVF_Cheat);

	/**
	 * Dividers handed to the match before the maze is built, on top of what the difficulty
	 * gives. Purely a measuring knob: the fence budget turned out to be what caps the maze,
	 * so this is how the question "how many would it take" gets a number instead of a guess.
	 */
	static int32 GExtraDividers = 0;

	static FAutoConsoleVariableRef CVarExtraDividers(
		TEXT("BD.Debug.AutoSetup.ExtraDividers"),
		GExtraDividers,
		TEXT("Extra dividers granted before the maze is built, to measure what a bigger DividerBudget would buy. 0 uses the difficulty's own."),
		ECVF_Cheat);

	static constexpr int32 FenceAttemptsPerFence = 4;

	/** Rings of cells around a target tried for a piece, nearest first. */
	static constexpr int32 SearchRadius = 2;

	/** Fraction of a stretch a target may drift from its middle, so two seeds never line up the same. */
	static constexpr float StretchJitter = 0.3f;

	/**
	 * Regions the board is cut into per axis, for the pieces to be dealt over. Two makes
	 * quadrants: enough to stop a corner from taking everything, coarse enough that a
	 * board whose routes all run down one side can still be filled.
	 */
	static constexpr int32 RegionsPerAxis = 2;

	/** Which region of the board a cell falls in. */
	static int32 RegionOf(const UBDGridSubsystem& Grid, const FBDCellCoord& Cell)
	{
		const int32 X = FMath::Clamp(Cell.X * RegionsPerAxis / FMath::Max(1, Grid.GetSizeX()), 0, RegionsPerAxis - 1);
		const int32 Y = FMath::Clamp(Cell.Y * RegionsPerAxis / FMath::Max(1, Grid.GetSizeY()), 0, RegionsPerAxis - 1);
		return Y * RegionsPerAxis + X;
	}

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

	// Never on its own: only the console variable or the command line switch it on, and
	// naming a seed on the command line is asking for it. The real flow starts empty.
	int32 Enabled = BDAutoSetupPrivate::GAutoSetup;
	int32 Seed = BDAutoSetupPrivate::GAutoSetupSeed;
	if (FParse::Value(FCommandLine::Get(), TEXT("BDAutoSetupSeed="), Seed))
	{
		Enabled = 1;
	}
	FParse::Value(FCommandLine::Get(), TEXT("BDAutoSetup="), Enabled);
	int32 Level = BDAutoSetupPrivate::GAutoSetupLevel;
	FParse::Value(FCommandLine::Get(), TEXT("BDAutoSetupLevel="), Level);

	if (Enabled == 0)
	{
		return;
	}

	// Only a world with a match: the menu level has no board to set up.
	if (ABDMatchManager::Get(&InWorld) == nullptr && InWorld.GetAuthGameMode<ABDGameMode>() == nullptr)
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

	// The urn, then the MAZE, then the defenders. The order is the strategy: fences are
	// the primary defense because they decide how long the horde walks, and a defender
	// placed before the maze exists is a defender standing next to a route that is about
	// to move somewhere else.
	const bool bUrn = PlaceObjective(*Placement, Stream);

	float RouteBefore = 0.0f;
	int32 LongestBefore = 0;
	int32 RoutedBefore = 0;
	MeasureRoutes(RouteBefore, LongestBefore, RoutedBefore);

	// The measuring knob: a bigger fence budget than the difficulty gives, so the question
	// of how many fences a real maze needs can be answered with a number.
	if (BDAutoSetupPrivate::GExtraDividers > 0)
	{
		Match->AdjustDividerBudget(BDAutoSetupPrivate::GExtraDividers, TEXT("BD.Debug.AutoSetup.ExtraDividers"));
	}

	const int32 Fences = bUrn ? BuildMaze(*Placement, Stream) : 0;

	float RouteAfter = 0.0f;
	int32 LongestAfter = 0;
	int32 RoutedAfter = 0;
	MeasureRoutes(RouteAfter, LongestAfter, RoutedAfter);

	UE_LOG(LogBDDebug, Log, TEXT("Maze: %d fence(s), route %.1f -> %.1f cells on average (%+.0f%%), longest %d -> %d, %d of %d mouth(s) still routed."),
		Fences, RouteBefore, RouteAfter,
		RouteBefore > 0.0f ? (RouteAfter / RouteBefore - 1.0f) * 100.0f : 0.0f,
		LongestBefore, LongestAfter, RoutedAfter, RoutedBefore);

	const int32 Platforms = bUrn ? PlacePlatforms(*Placement, Stream) : 0;
	const int32 Characters = bUrn ? FillSlots(*Placement, Stream) : 0;
	const int32 Towers = bUrn ? PlaceTowers(*Placement, Stream) : 0;

	Placement->CancelSelection();
	Placement->SetRefusalLogging(true);

	// Every defender at the requested level, for nothing: the question is where a whole
	// defense of that level breaks, not whether it could have been paid for.
	const int32 Level = FMath::Clamp(DefenderLevel, 1, UBDTowerData::MaxLevels);
	for (TActorIterator<ABDTowerBase> It(GetWorld()); It; ++It)
	{
		It->DebugSetLevel(Level);
	}

	UE_LOG(LogBDDebug, Log, TEXT("Auto setup done: urn %s, %d platform(s), %d character(s), %d tower(s), %d fence(s), all defenders at level %d. Left: %d platforms, %d dividers, %d free slot(s), %d public money."),
		bUrn ? TEXT("placed") : TEXT("NOT placed"), Platforms, Characters, Towers, Fences, Level,
		Match->GetPlatformsRemaining(), Match->GetDividersRemaining(), Match->GetFreeCharacterSlots(), Match->GetPublicMoney());

	LogDistribution();
}

int32 UBDDebugAutoSetup::BuildGrantedBudget()
{
	UBDPlacementComponent* Placement = FindPlacement();
	ABDMatchManager* Match = FindMatch();
	if (Placement == nullptr || Match == nullptr)
	{
		return 0;
	}

	// Asked before anything else, because what the capital buys is read off these palettes
	// and this may be the first call of a session.
	GatherPlaceables();

	// What there is left to build with: platforms still in hand, slots standing empty, and
	// whatever the public money earned since the last pass buys. The last one is the new one - the
	// defense now grows on the income of the waves, not on a ceiling somebody raised.
	const int32 Waiting = (BDAutoSetupPrivate::GGrowMaze != 0 ? Match->GetDividersRemaining() : 0)
		+ (Match->GetFreeCharacterSlots() > 0 ? 0 : FMath::Min(Match->GetPlatformsRemaining(), AffordableCount(PlatformPieces, BDAutoSetupPrivate::GPlatformShare)))
		+ FMath::Min(Match->GetFreeCharacterSlots(), AffordableCount(CharacterPieces, BDAutoSetupPrivate::GCharacterShare))
		+ AffordableCount(TowerPieces, BDAutoSetupPrivate::GTowerShare);
	if (Waiting <= 0)
	{
		return 0;
	}

	// A stream of its own per pass, seeded off the setup seed, so a run stays repeatable
	// and two passes do not try the same spots.
	FRandomStream Stream(LastSeed * 7919 + ++GrantedPasses);

	Placement->SetRefusalLogging(false);

	// The maze first, out of its own hand: the dividers the waves and the bosses granted.
	// Then platforms: they are what makes slots for the characters to stand on.
	const int32 Fences = BDAutoSetupPrivate::GGrowMaze != 0 ? BuildMaze(*Placement, Stream) : 0;
	const int32 Platforms = PlacePlatforms(*Placement, Stream);
	const int32 Characters = FillSlots(*Placement, Stream);
	const int32 Towers = PlaceTowers(*Placement, Stream);

	Placement->CancelSelection();
	Placement->SetRefusalLogging(true);

	const int32 Placed = Fences + Platforms + Characters + Towers;
	UE_CLOG(Placed > 0, LogBDDebug, Log, TEXT("Income built (pass %d): %d fence(s), %d platform(s), %d character(s), %d tower(s). Left: %d platforms, %d free slot(s), %d public money."),
		GrantedPasses, Fences, Platforms, Characters, Towers,
		Match->GetPlatformsRemaining(), Match->GetFreeCharacterSlots(), Match->GetPublicMoney());
	return Placed;
}

void UBDDebugAutoSetup::LogDistribution() const
{
	const UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = UBDGridSubsystem::Get(World);
	if (World == nullptr || Grid == nullptr)
	{
		return;
	}

	// Every defender standing, wherever it stands: on a cell of the grid or in the slot
	// of a platform. Both shoot, so both count as cover.
	TArray<int32> PerRegion;
	PerRegion.Init(0, BDAutoSetupPrivate::RegionsPerAxis * BDAutoSetupPrivate::RegionsPerAxis);

	for (TActorIterator<ABDTowerBase> It(World); It; ++It)
	{
		FBDCellCoord Cell;
		if (Grid->WorldToCell(It->GetActorLocation(), Cell))
		{
			++PerRegion[BDAutoSetupPrivate::RegionOf(*Grid, Cell)];
		}
	}

	FString Regions;
	for (const int32 Count : PerRegion)
	{
		Regions += Regions.IsEmpty() ? FString::FromInt(Count) : FString::Printf(TEXT(" %d"), Count);
	}

	FString Slots;
	int32 SlotsFilled = 0;
	int32 SlotsTotal = 0;
	for (TObjectIterator<UBDPlatformComponent> It; It; ++It)
	{
		if (It->GetWorld() != World || !It->bInsideBattleArea || !IsValid(It->GetOwner()))
		{
			continue;
		}

		int32 Used = 0;
		for (int32 Slot = 0; Slot < It->Slots.Num(); ++Slot)
		{
			Used += It->IsSlotFree(Slot) ? 0 : 1;
		}

		SlotsFilled += Used;
		SlotsTotal += It->Slots.Num();
		Slots += Slots.IsEmpty()
			? FString::Printf(TEXT("%d/%d"), Used, It->Slots.Num())
			: FString::Printf(TEXT(" %d/%d"), Used, It->Slots.Num());
	}

	int32 Routes = 0;
	int32 Covered = 0;
	if (UBDWaveSubsystem* Waves = FindWaves())
	{
		for (const FBDSpawnPoint& Point : Waves->GetSpawnPoints())
		{
			if (Point.Route.Num() == 0)
			{
				continue;
			}
			++Routes;

			for (TActorIterator<ABDTowerBase> It(World); It; ++It)
			{
				const float RangeSquared = FMath::Square(It->GetEffectiveRange());
				const FVector Guard = It->GetActorLocation();
				bool bInRange = false;
				for (const FBDCellCoord& Cell : Point.Route)
				{
					if (FVector::DistSquared2D(Guard, Grid->CellToWorld(Cell)) <= RangeSquared)
					{
						bInRange = true;
						break;
					}
				}
				if (bInRange)
				{
					++Covered;
					break;
				}
			}
		}
	}

	UE_LOG(LogBDDebug, Log, TEXT("Auto setup spread: defenders per region [%s]; platform slots %d/%d [%s]; %d of %d route(s) with a defender in range."),
		*Regions, SlotsFilled, SlotsTotal, *Slots, Covered, Routes);
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
			// Kept so the maze can be built around it without asking the world again.
			ObjectiveCell = Cell;
			return true;
		}
	}

	UE_LOG(LogBDDebug, Error, TEXT("Auto setup could not place the urn anywhere in its zone."));
	return false;
}

void UBDDebugAutoSetup::MeasureRoutes(float& OutAverage, int32& OutLongest, int32& OutRouted) const
{
	OutAverage = 0.0f;
	OutLongest = 0;
	OutRouted = 0;

	UBDWaveSubsystem* Waves = FindWaves();
	if (Waves == nullptr)
	{
		return;
	}

	int32 Total = 0;
	for (const FBDSpawnPoint& Point : Waves->GetSpawnPoints())
	{
		if (Point.Route.Num() > 0)
		{
			Total += Point.Route.Num();
			OutLongest = FMath::Max(OutLongest, Point.Route.Num());
			++OutRouted;
		}
	}
	OutAverage = OutRouted > 0 ? static_cast<float>(Total) / OutRouted : 0.0f;
}

int32 UBDDebugAutoSetup::RingTheUrn(UBDPlacementComponent& Placement, FRandomStream& Stream)
{
	const int32 Sides = FMath::Clamp(BDAutoSetupPrivate::GUrnRing, 0, 3);
	if (Sides <= 0)
	{
		return 0;
	}

	// The four sides of the cell the urn stands on. At most three go up: the fourth is
	// the door, and the placement would refuse it anyway once it sealed the last way in.
	const FBDCellCoord& Urn = ObjectiveCell;
	TArray<FBDEdgeCoord> Ring;
	Ring.Add(FBDEdgeCoord(Urn, FBDEdgeCoord::DirectionX));
	Ring.Add(FBDEdgeCoord(Urn, FBDEdgeCoord::DirectionY));
	Ring.Add(FBDEdgeCoord(FBDCellCoord(Urn.X - 1, Urn.Y), FBDEdgeCoord::DirectionX));
	Ring.Add(FBDEdgeCoord(FBDCellCoord(Urn.X, Urn.Y - 1), FBDEdgeCoord::DirectionY));

	for (int32 Index = Ring.Num() - 1; Index > 0; --Index)
	{
		Ring.Swap(Index, Stream.RandRange(0, Index));
	}

	int32 Placed = 0;
	for (const FBDEdgeCoord& Edge : Ring)
	{
		if (Placed >= Sides)
		{
			break;
		}

		Placement.SetRotationSteps(Edge.Direction == FBDEdgeCoord::DirectionX ? 1 : 0);
		Placement.SetHoveredEdgeDirect(Edge);
		if (Placement.IsCurrentPlacementValid() && Placement.TryPlaceAtHovered())
		{
			++Placed;
		}
	}

	UE_CLOG(Placed > 0, LogBDDebug, Verbose, TEXT("  urn ringed on %d of its %d side(s)."), Placed, Ring.Num());
	return Placed;
}

int32 UBDDebugAutoSetup::BuildMaze(UBDPlacementComponent& Placement, FRandomStream& Stream)
{
	UBDWaveSubsystem* Waves = FindWaves();
	const ABDMatchManager* Match = FindMatch();
	UBDPlaceableData* Piece = BDAutoSetupPrivate::Pick(DividerPieces, Stream);
	if (Waves == nullptr || Match == nullptr || Piece == nullptr)
	{
		return 0;
	}

	// The dividers are a hand of their own, not money: the maze takes its share of the
	// hand and leaves the public money whole for the defense.
	const int32 Wanted = FMath::FloorToInt(Match->GetBudgetRemaining(EBDPieceKind::Divider) * FMath::Clamp(BDAutoSetupPrivate::GFenceShare, 0.0f, 1.0f));
	if (Wanted <= 0)
	{
		return 0;
	}

	Placement.SelectPlaceable(Piece);

	// The door first: everything that follows bends around it.
	// (ExtraDividers, when set, was already added to the hand by Run.)
	int32 Placed = RingTheUrn(Placement, Stream);

	// Then the serpentine. The longest route is always the one worth bending, and it is
	// asked again after every fence because a fence that lands moves it.
	const int32 Stride = FMath::Max(1, BDAutoSetupPrivate::GMazeStride);
	int32 Barren = 0;
	while (Placed < Wanted && Barren < Wanted * BDAutoSetupPrivate::FenceAttemptsPerFence)
	{
		const TArray<FBDCellCoord>* Longest = nullptr;
		for (const FBDSpawnPoint& Point : Waves->GetSpawnPoints())
		{
			if (Point.Route.Num() >= 6 && (Longest == nullptr || Point.Route.Num() > Longest->Num()))
			{
				Longest = &Point.Route;
			}
		}
		if (Longest == nullptr)
		{
			break;
		}

		// Walked from the mouth towards the urn, a fence every Stride cells, so the
		// detours are spread along the whole walk instead of piling at one bend.
		bool bAny = false;
		const TArray<FBDCellCoord> Route = *Longest;
		for (int32 Index = 2; Index + 2 < Route.Num() && Placed < Wanted; Index += Stride)
		{
			bool bAdjacent = false;
			const FBDEdgeCoord Edge = FBDEdgeCoord::Between(Route[Index], Route[Index + 1], bAdjacent);
			if (!bAdjacent)
			{
				continue;
			}

			Placement.SetRotationSteps(Edge.Direction == FBDEdgeCoord::DirectionX ? 1 : 0);
			Placement.SetHoveredEdgeDirect(Edge);
			if (Placement.IsCurrentPlacementValid() && Placement.TryPlaceAtHovered())
			{
				++Placed;
				bAny = true;
				// The route this loop is walking is stale the moment a fence lands.
				break;
			}
		}

		Barren = bAny ? 0 : Barren + 1;
	}

	return Placed;
}

void UBDDebugAutoSetup::BuildCorridorTargets(const int32 Count, FRandomStream& Stream, TArray<FBDCellCoord>& OutTargets) const
{
	OutTargets.Reset();

	UBDWaveSubsystem* Waves = FindWaves();
	const UBDGridSubsystem* Grid = UBDGridSubsystem::Get(GetWorld());
	if (Waves == nullptr || Grid == nullptr || Count <= 0)
	{
		return;
	}

	// Every cell of every route, with how far along its own walk it is: the tail of the
	// route is what the urn focus pulls towards.
	TMap<FBDCellCoord, float> RouteCells;
	for (const FBDSpawnPoint& Point : Waves->GetSpawnPoints())
	{
		const int32 Length = Point.Route.Num();
		for (int32 Index = 0; Index < Length; ++Index)
		{
			const float Along = Length > 1 ? static_cast<float>(Index) / (Length - 1) : 1.0f;
			float& Best = RouteCells.FindOrAdd(Point.Route[Index], 0.0f);
			Best = FMath::Max(Best, Along);
		}
	}
	if (RouteCells.Num() == 0)
	{
		return;
	}

	// A spot is worth the route it covers: every route cell within a tower's reach counts
	// once. A bend of the serpentine has the route folded past it several times, so it
	// scores several times, which is exactly the spot a player would pick.
	const int32 Radius = BDAutoSetupPrivate::CorridorRadius;
	const float Focus = FMath::Clamp(BDAutoSetupPrivate::GFocusUrn, 0.0f, 1.0f);
	TMap<FBDCellCoord, float> Scores;
	for (const TPair<FBDCellCoord, float>& Cell : RouteCells)
	{
		for (int32 dY = -Radius; dY <= Radius; ++dY)
		{
			for (int32 dX = -Radius; dX <= Radius; ++dX)
			{
				if (dX * dX + dY * dY > Radius * Radius)
				{
					continue;
				}

				const FBDCellCoord Spot(Cell.Key.X + dX, Cell.Key.Y + dY);
				if (!Grid->IsValidCoord(Spot) || Grid->GetCellState(Spot) != EBDCellState::Free)
				{
					continue;
				}

				// One point for covering the cell, plus the urn focus on how late in the
				// walk that cell is: at Focus 0 every stretch is worth the same.
				Scores.FindOrAdd(Spot, 0.0f) += 1.0f + Focus * 4.0f * Cell.Value;
			}
		}
	}
	if (Scores.Num() == 0)
	{
		return;
	}

	// A little noise on the way in, so two seeds with the same board do not lay the same
	// defense down to the cell while the ranking still decides the shape.
	TArray<TPair<FBDCellCoord, float>> Ranked;
	Ranked.Reserve(Scores.Num());
	for (const TPair<FBDCellCoord, float>& Entry : Scores)
	{
		Ranked.Emplace(Entry.Key, Entry.Value * Stream.FRandRange(0.9f, 1.1f));
	}
	Ranked.Sort([](const TPair<FBDCellCoord, float>& A, const TPair<FBDCellCoord, float>& B)
	{
		return A.Value > B.Value;
	});

	// Taken from the top, but never two touching: a defender on the very next cell covers
	// almost the same route and the corridor would be armed in one clump.
	for (const TPair<FBDCellCoord, float>& Entry : Ranked)
	{
		if (OutTargets.Num() >= Count)
		{
			break;
		}

		const bool bCrowded = OutTargets.ContainsByPredicate([&Entry](const FBDCellCoord& Taken)
		{
			return FMath::Abs(Taken.X - Entry.Key.X) <= 1 && FMath::Abs(Taken.Y - Entry.Key.Y) <= 1;
		});
		if (!bCrowded)
		{
			OutTargets.Add(Entry.Key);
		}
	}

	// Still short (a small board, a short corridor): the best spots again, crowding allowed.
	for (int32 Index = 0; OutTargets.Num() < Count && Index < Ranked.Num(); ++Index)
	{
		OutTargets.Add(Ranked[Index].Key);
	}
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

int32 UBDDebugAutoSetup::AffordableCount(const TArray<TObjectPtr<UBDPlaceableData>>& Pieces, const float Share) const
{
	const ABDMatchManager* Match = FindMatch();
	if (Match == nullptr || Pieces.Num() == 0)
	{
		return 0;
	}

	// The average of the kinds, because the spread passes draw one at random per spot:
	// asking the cheapest would promise more pieces than the capital pays for, and the
	// dearest would leave money unspent. Priced on the current wave, like a click would be.
	// Pieces not unlocked yet are left out: they cannot be bought, whatever they cost.
	int32 Total = 0;
	int32 Kinds = 0;
	for (const TObjectPtr<UBDPlaceableData>& Piece : Pieces)
	{
		if (Piece != nullptr && Match->IsUnlocked(Piece))
		{
			Total += FMath::Max(1, Match->GetBuildPrice(Piece));
			++Kinds;
		}
	}
	if (Kinds == 0)
	{
		return 0;
	}

	const int32 AverageCost = FMath::Max(1, Total / Kinds);
	return FMath::FloorToInt(Match->GetPublicMoney() * FMath::Clamp(Share, 0.0f, 1.0f)) / AverageCost;
}

int32 UBDDebugAutoSetup::PlacePlatforms(UBDPlacementComponent& Placement, FRandomStream& Stream)
{
	const ABDMatchManager* Match = FindMatch();
	if (Match == nullptr || PlatformPieces.Num() == 0)
	{
		return 0;
	}

	// Paid for like everything else: a share of the money, never past the hand, and none
	// while a slot already standing is empty - a player mans what they have first.
	const int32 Wanted = Match->GetFreeCharacterSlots() > 0 ? 0
		: FMath::Min(Match->GetBudgetRemaining(EBDPieceKind::Platform), AffordableCount(PlatformPieces, BDAutoSetupPrivate::GPlatformShare));
	if (Wanted <= 0)
	{
		return 0;
	}

	TArray<FBDCellCoord> Targets;
	BuildCorridorTargets(Wanted, Stream, Targets);

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

	int32 MostSlots = 0;
	for (const UBDPlatformComponent* Platform : Platforms)
	{
		MostSlots = FMath::Max(MostSlots, Platform->Slots.Num());
	}

	// What the characters may spend, read once: asked again per slot it would keep taking
	// a share of a smaller and smaller balance and never leave the towers anything.
	const int32 Wanted = AffordableCount(CharacterPieces, BDAutoSetupPrivate::GCharacterShare);

	// A slot at a time across every platform, not a platform at a time: twelve characters
	// over six platforms is two on each, and a platform standing empty beside a full one
	// is exactly the heap this setup exists to avoid.
	int32 Placed = 0;
	for (int32 Slot = 0; Slot < MostSlots; ++Slot)
	{
		for (UBDPlatformComponent* Platform : Platforms)
		{
			// A character is held back by the slot it stands on and by the money it costs.
			// The slot is answered by the loop; this is the other half.
			if (Placed >= Wanted || AffordableCount(CharacterPieces, 1.0f) <= 0)
			{
				return Placed;
			}
			if (!Platform->Slots.IsValidIndex(Slot) || !Platform->IsSlotFree(Slot))
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

	// No ceiling decides this any more: what the capital left in hand buys does. The
	// characters got their slots first, because a platform standing empty evolves nothing.
	TArray<FBDCellCoord> Targets;
	BuildCorridorTargets(AffordableCount(TowerPieces, BDAutoSetupPrivate::GTowerShare), Stream, Targets);

	int32 Placed = 0;
	for (const FBDCellCoord& Target : Targets)
	{
		if (TryPlaceCellPieceNear(Placement, BDAutoSetupPrivate::Pick(TowerPieces, Stream), Target, Stream))
		{
			++Placed;
		}
	}

	return Placed + CoverUncoveredRoutes(Placement, Stream);
}

int32 UBDDebugAutoSetup::CoverUncoveredRoutes(UBDPlacementComponent& Placement, FRandomStream& Stream)
{
	UBDWaveSubsystem* Waves = FindWaves();
	const ABDMatchManager* Match = FindMatch();
	const UBDGridSubsystem* Grid = UBDGridSubsystem::Get(GetWorld());
	if (Waves == nullptr || Match == nullptr || Grid == nullptr || TowerPieces.Num() == 0)
	{
		return 0;
	}

	int32 Placed = 0;
	for (const FBDSpawnPoint& Point : Waves->GetSpawnPoints())
	{
		if (AffordableCount(TowerPieces, 1.0f) <= 0)
		{
			break;
		}
		if (Point.Route.Num() < 3)
		{
			continue;
		}

		// Every defender already standing, characters on their platforms included, with
		// the range it actually covers. Read again per route: the last one placed counts.
		bool bCovered = false;
		for (TActorIterator<ABDTowerBase> It(GetWorld()); It && !bCovered; ++It)
		{
			const float RangeSquared = FMath::Square(It->GetEffectiveRange());
			const FVector Guard = It->GetActorLocation();
			for (const FBDCellCoord& Cell : Point.Route)
			{
				if (FVector::DistSquared2D(Guard, Grid->CellToWorld(Cell)) <= RangeSquared)
				{
					bCovered = true;
					break;
				}
			}
		}

		if (bCovered)
		{
			continue;
		}

		// The middle of the route: the stretch the creeps spend the longest on, and far
		// enough from both ends that a piece has somewhere to stand.
		const FBDCellCoord& Target = Point.Route[Point.Route.Num() / 2];
		if (TryPlaceCellPieceNear(Placement, BDAutoSetupPrivate::Pick(TowerPieces, Stream), Target, Stream))
		{
			++Placed;
			UE_LOG(LogBDDebug, Verbose, TEXT("  defender added on the uncovered route through %s."), *Target.ToString());
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

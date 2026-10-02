// Brazil Defense. The thermometer: every mechanic that already works, checked in one go.

#include "Debug/BDRegressionSubsystem.h"

#include "BDBuildInfo.h"
#include "BDLog.h"
#include "Bribe/BDBribeSubsystem.h"
#include "Candidate/BDCandidateSubsystem.h"
#include "Enemy/BDCandidate.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Enemy/BDEnemyData.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Enemy/BDAnimNotify_BodyFall.h"
#include "Enemy/BDAnimNotify_Footstep.h"
#include "Enemy/BDCreepCorpse.h"
#include "EngineUtils.h"
#include "UI/BDUISettings.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Match/BDDifficultyData.h"
#include "Match/BDGameBalanceSettings.h"
#include "Match/BDMatchManager.h"
#include "Objective/BDObjective.h"
#include "Palace/BDAgent.h"
#include "Palace/BDPalace.h"
#include "Palace/BDPalaceData.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Objective/BDObjectiveSettings.h"
#include "Objective/BDObjectiveSubsystem.h"
#include "Placement/BDPlaceableData.h"
#include "SkeletalMeshComponentBudgeted.h"
#include "Placement/BDPlacementComponent.h"
#include "Placement/BDPlacementSettings.h"
#include "Platform/BDPlatformComponent.h"
#include "Stats/Stats.h"
#include "Tower/BDTowerBase.h"
#include "UObject/UObjectIterator.h"
#include "Wave/BDBusSubsystem.h"
#include "Wave/BDWaveSettings.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDRegressionPrivate
{
	/** Steps that wait on the game give up after this many frames and say so. */
	static constexpr int32 MaxWaitTicks = 600;

	/** The first piece of the palette of a kind. The platform asked for is the one open from the start. */
	static UBDPlaceableData* FindPiece(const EBDPieceKind Kind)
	{
		for (const TSoftObjectPtr<UBDPlaceableData>& Entry : UBDPlacementSettings::Get().Palette)
		{
			UBDPlaceableData* Data = Entry.LoadSynchronous();
			if (Data != nullptr && Data->GetPieceKind() == Kind && ABDMatchManager::GetUnlockWave(Data) == 0)
			{
				return Data;
			}
		}
		return nullptr;
	}

	static int32 GetCVarInt(const TCHAR* Name)
	{
		const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name);
		return Variable != nullptr ? Variable->GetInt() : 0;
	}

	static void SetCVarInt(const TCHAR* Name, const int32 Value)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Variable->Set(Value, ECVF_SetByCode);
		}
	}

	static FString RefusalName(const EBDPlacementRefusal Refusal)
	{
		return StaticEnum<EBDPlacementRefusal>()->GetNameStringByValue(static_cast<int64>(Refusal));
	}
}

TStatId UBDRegressionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBDRegressionSubsystem, STATGROUP_Tickables);
}

void UBDRegressionSubsystem::Deinitialize()
{
	if (ABDMatchManager* Match = WatchedMatch.Get())
	{
		Match->OnVotesChanged.Remove(VotesHandle);
	}
	Super::Deinitialize();
}

ABDMatchManager* UBDRegressionSubsystem::GetMatch() const
{
	return ABDMatchManager::Get(GetWorld());
}

UBDPlacementComponent* UBDRegressionSubsystem::GetPlacement() const
{
	const UWorld* World = GetWorld();
	const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	return Controller != nullptr ? Controller->FindComponentByClass<UBDPlacementComponent>() : nullptr;
}

void UBDRegressionSubsystem::Start(const bool bQuitWhenDone)
{
	ABDMatchManager* Match = GetMatch();
	if (bRunning)
	{
		UE_LOG(LogBDDebug, Warning, TEXT("REGRESSION is already running."));
		return;
	}
	if (Match == nullptr || GetPlacement() == nullptr || Match->GetCurrentWave() != 0 || Match->GetObjectivesRemaining() != 1
		|| Match->GetPhase() != EBDMatchPhase::Building)
	{
		UE_LOG(LogBDDebug, Error, TEXT("REGRESSION needs a fresh match: building for wave 1 with no urn down. Run it right after the map opens (e.g. -ExecCmds=\"BD.Test.Regression 1\")."));
		return;
	}

	bRunning = true;
	bQuit = bQuitWhenDone;
	Step = 0;
	bCandidateWaveDealt = false;
	WaitTicks = 0;
	StepTicks = 0;
	Passed = 0;
	Failed = 0;
	Crew.Reset();

	// The countdown held, so the script decides when waves go; the report kept out of the
	// sheet, so a test match never reads as a played one.
	SavedFreezeTimer = BDRegressionPrivate::GetCVarInt(TEXT("BD.Match.FreezeTimer"));
	SavedPostMatch = BDRegressionPrivate::GetCVarInt(TEXT("BD.PostMatch.Enabled"));
	SavedWaveLog = BDRegressionPrivate::GetCVarInt(TEXT("BD.WaveLog.Enabled"));
	BDRegressionPrivate::SetCVarInt(TEXT("BD.Match.FreezeTimer"), 1);
	BDRegressionPrivate::SetCVarInt(TEXT("BD.PostMatch.Enabled"), 0);
	BDRegressionPrivate::SetCVarInt(TEXT("BD.WaveLog.Enabled"), 0);

	WatchedMatch = Match;
	LastBlue = Match->GetVotesBlue();
	LevelDownsSeen = Match->GetLevelDownCount();
	UnexplainedDrops = 0;
	VotesHandle = Match->OnVotesChanged.AddUObject(this, &UBDRegressionSubsystem::HandleVotesChanged);

	UE_LOG(LogBDDebug, Log, TEXT("REGRESSION start on %s, seed %d."), *BDBuildInfo::GetLabel(), Match->ObstacleSeed);
}

void UBDRegressionSubsystem::HandleVotesChanged(const int32 Blue, const int32 Red)
{
	// Blue only ever goes down through the levelling after the fallen are beaten again;
	// anything else taking it down is votes being spent, which nothing may do.
	const ABDMatchManager* Match = WatchedMatch.Get();
	if (Match != nullptr && Blue < LastBlue)
	{
		if (Match->GetLevelDownCount() > LevelDownsSeen)
		{
			LevelDownsSeen = Match->GetLevelDownCount();
		}
		else
		{
			++UnexplainedDrops;
			UE_LOG(LogBDDebug, Warning, TEXT("REGRESSION blue went down %d -> %d with no levelling behind it."), LastBlue, Blue);
		}
	}
	LastBlue = Blue;
}

void UBDRegressionSubsystem::Check(const TCHAR* Area, const TCHAR* What, const bool bPass, const FString& Measured)
{
	bPass ? ++Passed : ++Failed;
	const FString Line = FString::Printf(TEXT("%s  %-11s %s (%s)"), bPass ? TEXT("PASS") : TEXT("FAIL"), Area, What, *Measured);
	if (bPass)
	{
		UE_LOG(LogBDDebug, Log, TEXT("REGRESSION %s"), *Line);
	}
	else
	{
		UE_LOG(LogBDDebug, Error, TEXT("REGRESSION %s"), *Line);
	}
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 60.0f, bPass ? FColor::Green : FColor::Red, Line);
	}
}

void UBDRegressionSubsystem::Finish(const FString& Why)
{
	bRunning = false;
	if (ABDMatchManager* Match = WatchedMatch.Get())
	{
		Match->OnVotesChanged.Remove(VotesHandle);
	}
	if (UBDPlacementComponent* Placement = GetPlacement())
	{
		Placement->CancelSelection();
	}
	BDRegressionPrivate::SetCVarInt(TEXT("BD.Match.FreezeTimer"), SavedFreezeTimer);
	BDRegressionPrivate::SetCVarInt(TEXT("BD.PostMatch.Enabled"), SavedPostMatch);
	BDRegressionPrivate::SetCVarInt(TEXT("BD.WaveLog.Enabled"), SavedWaveLog);

	const FString Total = FString::Printf(TEXT("REGRESSION %s: %d PASS, %d FAIL%s"),
		Failed == 0 ? TEXT("ALL GREEN") : TEXT("BROKEN"), Passed, Failed, Why.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (%s)"), *Why));
	if (Failed == 0)
	{
		UE_LOG(LogBDDebug, Log, TEXT("%s"), *Total);
	}
	else
	{
		UE_LOG(LogBDDebug, Error, TEXT("%s"), *Total);
	}
	if (GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(-1, 60.0f, Failed == 0 ? FColor::Green : FColor::Red, Total);
	}

	if (bQuit)
	{
		FPlatformMisc::RequestExit(false);
	}
}

void UBDRegressionSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bRunning)
	{
		return;
	}
	if (WaitTicks > 0)
	{
		--WaitTicks;
		return;
	}

	if (!RunStep(Step))
	{
		Finish(FString::Printf(TEXT("stopped at step %d"), Step));
	}
}

bool UBDRegressionSubsystem::PlaceFence(const FBDEdgeCoord& Edge)
{
	UBDPlacementComponent* Placement = GetPlacement();
	Placement->SetRotationSteps(Edge.Direction == FBDEdgeCoord::DirectionX ? 1 : 0);
	Placement->SetHoveredEdgeDirect(Edge);
	return Placement->IsCurrentPlacementValid() && Placement->TryPlaceAtHovered();
}

bool UBDRegressionSubsystem::PlaceNear(UBDPlaceableData* Piece, const FBDCellCoord& Near, const int32 Radius, FBDCellCoord& OutCell)
{
	UBDPlacementComponent* Placement = GetPlacement();
	if (Piece == nullptr || !Placement->TakeIntoHand(Piece))
	{
		return false;
	}

	// Rings outwards from the point, so the piece lands as close as the board lets it.
	for (int32 Ring = 1; Ring <= Radius; ++Ring)
	{
		for (int32 DY = -Ring; DY <= Ring; ++DY)
		{
			for (int32 DX = -Ring; DX <= Ring; ++DX)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Ring)
				{
					continue;
				}
				const FBDCellCoord Cell(Near.X + DX, Near.Y + DY);
				Placement->SetHoveredCellDirect(Cell);
				if (Placement->IsCurrentPlacementValid() && Placement->TryPlaceAtHovered())
				{
					OutCell = Cell;
					return true;
				}
			}
		}
	}
	Placement->CancelSelection();
	return false;
}

ABDTowerBase* UBDRegressionSubsystem::FindNewTower(const TArray<ABDTowerBase*>& Before) const
{
	for (TActorIterator<ABDTowerBase> It(GetWorld()); It; ++It)
	{
		if (!Before.Contains(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

bool UBDRegressionSubsystem::RunStep(const int32 Index)
{
	using namespace BDRegressionPrivate;

	UWorld* World = GetWorld();
	ABDMatchManager* Match = GetMatch();
	UBDPlacementComponent* Placement = GetPlacement();
	UBDGridSubsystem* Grid = UBDGridSubsystem::Get(World);
	UBDWaveSubsystem* Waves = World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr;
	UBDCandidateSubsystem* Candidates = World != nullptr ? World->GetSubsystem<UBDCandidateSubsystem>() : nullptr;
	UBDBribeSubsystem* Bribes = UBDBribeSubsystem::Get(World);
	if (Match == nullptr || Placement == nullptr || Grid == nullptr || Waves == nullptr || Candidates == nullptr || Bribes == nullptr)
	{
		Check(TEXT("SETUP"), TEXT("the match and its systems are there"), false, TEXT("one of them is gone"));
		return false;
	}

	UBDPlaceableData* Divider = FindPiece(EBDPieceKind::Divider);
	UBDPlaceableData* Tower = FindPiece(EBDPieceKind::Tower);
	UBDPlaceableData* Character = FindPiece(EBDPieceKind::Character);
	UBDPlaceableData* Platform = FindPiece(EBDPieceKind::Platform);

	switch (Index)
	{
	case 0:
	{
		// Money enough for everything the script buys: the checks are about where it goes.
		Match->AddPublicMoney(200000, TEXT("BD.Test.Regression"));

		// The urn on a cell with all four sides open, away from the edge, so it can be
		// fenced in on three sides and the fourth tested.
		UBDPlaceableData* Urn = UBDObjectiveSettings::Get().ObjectivePlaceable.LoadSynchronous();
		const auto IsOpen = [Grid](const FBDCellCoord& Cell) { return Grid->IsValidCoord(Cell) && Grid->GetCellState(Cell) == EBDCellState::Free; };
		bool bPlaced = false;
		for (int32 X = Grid->GetSizeX() - 4; X >= 3 && !bPlaced; --X)
		{
			for (int32 Y = 3; Y < Grid->GetSizeY() - 3 && !bPlaced; ++Y)
			{
				const FBDCellCoord Cell(X, Y);
				if (!IsOpen(Cell) || !IsOpen(FBDCellCoord(X + 1, Y)) || !IsOpen(FBDCellCoord(X - 1, Y))
					|| !IsOpen(FBDCellCoord(X, Y + 1)) || !IsOpen(FBDCellCoord(X, Y - 1)) || !Placement->TakeIntoHand(Urn))
				{
					continue;
				}
				// A quarter turn in hand, the way the wheel click gives it: the urn has to keep it.
				Placement->SetRotationSteps(1);
				Placement->SetHoveredCellDirect(Cell);
				if (Placement->IsCurrentPlacementValid() && Placement->TryPlaceAtHovered())
				{
					UrnCell = Cell;
					bPlaced = true;
				}
			}
		}
		Placement->CancelSelection();
		Check(TEXT("SETUP"), TEXT("the urn goes down"), bPlaced, bPlaced ? UrnCell.ToString() : TEXT("no open cell took it"));
		if (bPlaced)
		{
			const UBDObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UBDObjectiveSubsystem>();
			const AActor* UrnActor = Objectives != nullptr ? Objectives->GetObjective() : nullptr;
			const float UrnYaw = UrnActor != nullptr ? FRotator::NormalizeAxis(UrnActor->GetActorRotation().Yaw) : -1.0f;
			Check(TEXT("COLOCACAO"), TEXT("the urn faces the turn the preview had"), FMath::IsNearlyEqual(UrnYaw, 90.0f, 1.0f),
				FString::Printf(TEXT("yaw %.1f, 90 given"), UrnYaw));
		}
		if (!bPlaced)
		{
			return false;
		}
		WaitTicks = 2;
		break;
	}

	case 1:
	{
		// Every mouth has its way to the urn.
		const TArray<FBDSpawnPoint>& Points = Waves->GetSpawnPoints();
		int32 Routed = 0;
		for (const FBDSpawnPoint& Point : Points)
		{
			Routed += Point.Route.Num() > 0 ? 1 : 0;
		}
		Check(TEXT("PATHFINDING"), TEXT("every mouth has a route to the urn"), Points.Num() > 0 && Routed == Points.Num(),
			FString::Printf(TEXT("%d of %d routed"), Routed, Points.Num()));

		// Three sides of the urn fenced; the fourth would seal it and must be refused. The
		// first fence also shows the divider hand is its own: the money does not move.
		const FBDEdgeCoord Sides[] = {
			FBDEdgeCoord(UrnCell, FBDEdgeCoord::DirectionX),
			FBDEdgeCoord(FBDCellCoord(UrnCell.X - 1, UrnCell.Y), FBDEdgeCoord::DirectionX),
			FBDEdgeCoord(UrnCell, FBDEdgeCoord::DirectionY),
			FBDEdgeCoord(FBDCellCoord(UrnCell.X, UrnCell.Y - 1), FBDEdgeCoord::DirectionY) };
		const int32 MoneyBefore = Match->GetPublicMoney();
		const int32 HandBefore = Match->GetDividersRemaining();
		if (!Placement->TakeIntoHand(Divider))
		{
			Check(TEXT("SEPARADOR"), TEXT("a divider can be taken"), false, RefusalName(Placement->GetHandRefusal(Divider)));
			return false;
		}
		int32 Fenced = 0;
		for (int32 Side = 0; Side < 3; ++Side)
		{
			Fenced += PlaceFence(Sides[Side]) ? 1 : 0;
			if (Side == 0)
			{
				Check(TEXT("SEPARADOR"), TEXT("a divider costs no public money and comes out of its own hand"),
					Match->GetPublicMoney() == MoneyBefore && Match->GetDividersRemaining() == HandBefore - 1,
					FString::Printf(TEXT("money %d -> %d, hand %d -> %d"), MoneyBefore, Match->GetPublicMoney(), HandBefore, Match->GetDividersRemaining()));
			}
		}
		Placement->SetRotationSteps(Sides[3].Direction == FBDEdgeCoord::DirectionX ? 1 : 0);
		Placement->SetHoveredEdgeDirect(Sides[3]);
		const EBDPlacementRefusal Sealing = Placement->GetCurrentRefusal();
		Check(TEXT("PATHFINDING"), TEXT("fencing the urn in completely is refused"), Fenced == 3 && Sealing == EBDPlacementRefusal::WouldBlockPath,
			FString::Printf(TEXT("%d of 3 sides fenced, the fourth: %s"), Fenced, *RefusalName(Sealing)));

		// The fences come off again: the rest of the script wants an open urn.
		Placement->CancelSelection();
		for (int32 Side = 0; Side < 3; ++Side)
		{
			Placement->SetHoveredEdgeDirect(Sides[Side]);
			Placement->TryRemoveAtHovered();
		}
		WaitTicks = 1;
		break;
	}

	case 2:
	{
		// No platform on the board yet: a character has nowhere to stand and is not offered.
		const EBDPlacementRefusal NoSlot = Placement->GetHandRefusal(Character);
		Check(TEXT("COLOCACAO"), TEXT("a character is refused with no free slot"), NoSlot == EBDPlacementRefusal::NoFreeSlot, RefusalName(NoSlot));

		// A ground tower bought, then a level: public money goes, the count does not move.
		TArray<ABDTowerBase*> Before;
		for (TActorIterator<ABDTowerBase> It(World); It; ++It) { Before.Add(*It); }
		const int32 Price = Match->GetBuildPrice(Tower);
		const int32 MoneyBefore = Match->GetPublicMoney();
		const int32 BlueBefore = Match->GetVotesBlue();
		const bool bBuilt = PlaceNear(Tower, UrnCell, 6, TowerCell);
		Placement->CancelSelection();
		GroundTower = FindNewTower(Before);
		Check(TEXT("ECONOMIA"), TEXT("building takes the price in public money and no votes"),
			bBuilt && Match->GetPublicMoney() == MoneyBefore - Price && Match->GetVotesBlue() == BlueBefore,
			FString::Printf(TEXT("price %d, money %d -> %d, blue %d -> %d"), Price, MoneyBefore, Match->GetPublicMoney(), BlueBefore, Match->GetVotesBlue()));
		if (!GroundTower.IsValid())
		{
			return false;
		}

		const int32 Cost = GroundTower->GetUpgradeCost();
		const int32 MoneyAtLevel = Match->GetPublicMoney();
		const float RangeAtLevel = GroundTower->GetEffectiveRangeCells();
		const float RangePromised = GroundTower->GetRangeCellsAtNextLevel();
		const bool bEvolved = GroundTower->Upgrade();
		Check(TEXT("ECONOMIA"), TEXT("evolving takes the level's cost in public money and no votes"),
			bEvolved && Match->GetPublicMoney() == MoneyAtLevel - Cost && Match->GetVotesBlue() == BlueBefore,
			FString::Printf(TEXT("cost %d, money %d -> %d, blue %d -> %d, level %d"), Cost, MoneyAtLevel, Match->GetPublicMoney(), BlueBefore, Match->GetVotesBlue(), GroundTower->GetTowerLevel()));

		// The reach a level promises is the reach it gives, and on the formula it grows.
		const float RangeNow = GroundTower->GetEffectiveRangeCells();
		const float Growth = UBDGameBalanceSettings::Get().RangeGrowthPerLevel;
		Check(TEXT("EVOLUCAO"), TEXT("evolving grows the range by the level formula"),
			bEvolved && FMath::IsNearlyEqual(RangeNow, RangePromised, 0.01f) && (Growth <= 0.0f || RangeNow > RangeAtLevel),
			FString::Printf(TEXT("range %.2f -> %.2f cells, promised %.2f, growth %.2f per level"), RangeAtLevel, RangeNow, RangePromised, Growth));
		break;
	}

	case 3:
	{
		// A platform, manned one character at a time: the block rule holds the levels back
		// until every slot is filled, and then lets only the lowest level buy.
		FBDCellCoord StandCell;
		TArray<UBDPlatformComponent*> PlatformsBefore;
		for (TObjectIterator<UBDPlatformComponent> It; It; ++It) { if (It->GetWorld() == World) { PlatformsBefore.Add(*It); } }
		const bool bStand = PlaceNear(Platform, UrnCell, 8, StandCell);
		Placement->CancelSelection();
		for (TObjectIterator<UBDPlatformComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && !PlatformsBefore.Contains(*It)) { Stand = *It; }
		}
		if (!bStand || !Stand.IsValid() || Stand->Slots.Num() < 2)
		{
			Check(TEXT("PLATAFORMA"), TEXT("a platform goes down"), false, bStand ? TEXT("it has fewer than two slots") : TEXT("no cell near the urn took it"));
			return false;
		}

		const auto Mount = [this, Placement, Character, World](const int32 Slot) -> ABDTowerBase*
		{
			TArray<ABDTowerBase*> Before;
			for (TActorIterator<ABDTowerBase> It(World); It; ++It) { Before.Add(*It); }
			if (!Placement->TakeIntoHand(Character))
			{
				return nullptr;
			}
			Placement->SetHoveredSlotDirect(Stand.Get(), Slot);
			const bool bMounted = Placement->IsCurrentPlacementValid() && Placement->TryPlaceAtHovered();
			Placement->CancelSelection();
			return bMounted ? FindNewTower(Before) : nullptr;
		};

		ABDTowerBase* First = Mount(0);
		FString Reason;
		const bool bHeldBack = First != nullptr && !First->CanUpgrade(Reason);
		Check(TEXT("PLATAFORMA"), TEXT("one character on a platform with empty slots cannot evolve"), bHeldBack,
			First == nullptr ? TEXT("the character did not mount") : (Reason.IsEmpty() ? TEXT("it could") : Reason));
		if (First == nullptr)
		{
			return false;
		}
		Crew.Add(First);
		for (int32 Slot = 1; Slot < Stand->Slots.Num(); ++Slot)
		{
			if (ABDTowerBase* Mounted = Mount(Slot)) { Crew.Add(Mounted); }
		}
		Check(TEXT("PLATAFORMA"), TEXT("the slots the board counts match the platforms standing"),
			Crew.Num() == Stand->Slots.Num() && Match->GetFreeCharacterSlots() == 0 && Stand->IsFullyManned(),
			FString::Printf(TEXT("%d of %d slots manned, %d free on the board"), Crew.Num(), Stand->Slots.Num(), Match->GetFreeCharacterSlots()));

		// Full: the first may buy, and then may not again until the others catch up.
		SlotZAtLevelOne = Stand->GetSlotWorldTransform(0).GetLocation().Z;
		const bool bFirstBuys = Crew[0]->Upgrade();
		FString OutOfStep;
		const bool bThenWaits = !Crew[0]->CanUpgrade(OutOfStep);
		Check(TEXT("PLATAFORMA"), TEXT("a full platform evolves in step: the one ahead waits for the rest"), bFirstBuys && bThenWaits,
			FString::Printf(TEXT("first bought %s, then %s"), bFirstBuys ? TEXT("yes") : TEXT("no"), bThenWaits ? *OutOfStep : TEXT("could buy again")));
		for (int32 Member = 1; Member < Crew.Num(); ++Member)
		{
			if (Crew[Member].IsValid()) { Crew[Member]->Upgrade(); }
		}
		Check(TEXT("PLATAFORMA"), TEXT("the block reaches level 2 once every shooter has"), Stand->GetBlockLevel() == 2,
			FString::Printf(TEXT("block level %d"), Stand->GetBlockLevel()));

		// The height is built on the platform's own tick.
		WaitTicks = 3;
		break;
	}

	case 4:
	{
		// A storey higher for the level, and the shooters up there with the deck.
		const float Lift = Stand->GetSlotWorldTransform(0).GetLocation().Z - SlotZAtLevelOne;
		Check(TEXT("PLATAFORMA"), TEXT("the height follows the level: one storey for level 2"),
			Stand->GetVisualLevel() == 2 && FMath::IsNearlyEqual(Lift, Stand->FloorHeight, 1.0f),
			FString::Printf(TEXT("visual level %d, slot raised %.0f cm, a storey is %.0f"), Stand->GetVisualLevel(), Lift, Stand->FloorHeight));
		float WorstGap = 0.0f;
		for (int32 Slot = 0; Slot < Crew.Num(); ++Slot)
		{
			if (Crew[Slot].IsValid())
			{
				WorstGap = FMath::Max(WorstGap, FMath::Abs(Crew[Slot]->GetActorLocation().Z - Stand->GetSlotWorldTransform(Slot).GetLocation().Z));
			}
		}
		Check(TEXT("PLATAFORMA"), TEXT("the shooters ride up with their slots"), WorstGap <= 5.0f, FString::Printf(TEXT("worst gap %.1f cm"), WorstGap));

		// The palace: 2x2, bought with public money at its share of candidates, holding its
		// cells like a platform, five empty stars over it. Sold again at the end, so the
		// rest of the script plays on the board it always had.
		UBDPlaceableData* PalacePiece = FindPiece(EBDPieceKind::Palace);
		if (PalacePiece == nullptr)
		{
			Check(TEXT("PALACIO"), TEXT("the palace is in the palette from the start"), false, TEXT("no Palace piece open on wave 0"));
			break;
		}
		const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
		const int32 PalacePrice = Match->GetBuildPrice(PalacePiece);
		const int32 Funds = Match->GetCandidateFunds(Match->GetPriceWave());
		const int32 Expected = FMath::RoundToInt(Funds * Balance.PalaceCostInCandidates);
		int32 DearestOther = 0;
		for (const TSoftObjectPtr<UBDPlaceableData>& Entry : UBDPlacementSettings::Get().Palette)
		{
			const UBDPlaceableData* Other = Entry.LoadSynchronous();
			if (Other != nullptr && Other != PalacePiece)
			{
				DearestOther = FMath::Max(DearestOther, Match->GetBuildPrice(Other));
			}
		}
		Check(TEXT("PALACIO"), TEXT("the palace costs PalaceCostInCandidates candidates and more than any other piece"),
			FMath::Abs(PalacePrice - Expected) <= 1 && PalacePrice > DearestOther,
			FString::Printf(TEXT("price %d, %.1f x candidate %d = %d, dearest other %d"), PalacePrice, Balance.PalaceCostInCandidates, Funds, Expected, DearestOther));

		// What a player who spends nothing has by the third candidate, against the price then.
		const int32 Interval = FMath::Max(1, Balance.CandidateInterval);
		const int32 Start = Match->GetDifficultyData() != nullptr ? Match->GetDifficultyData()->StartingFunds : 0;
		const int32 ThirdWave = Interval * 3;
		const int32 ByThird = Start + Match->GetCandidateFunds(Interval) + Match->GetCandidateFunds(Interval * 2) + Match->GetCandidateFunds(ThirdWave);
		UE_LOG(LogBDDebug, Log, TEXT("REGRESSION palace calibration: price %d on wave %d, %d on wave %d; saved by the third candidate %d (start %d + bribes of waves %d, %d, %d)."),
			PalacePrice, Match->GetPriceWave(), Match->GetBuildPriceOnWave(PalacePiece, ThirdWave), ThirdWave, ByThird, Start, Interval, Interval * 2, ThirdWave);

		TArray<ABDPalace*> PalacesBefore;
		for (TActorIterator<ABDPalace> It(World); It; ++It) { PalacesBefore.Add(*It); }
		const int32 MoneyBefore = Match->GetPublicMoney();
		FBDCellCoord PalaceCell;
		const bool bPalace = PlaceNear(PalacePiece, UrnCell, 10, PalaceCell);
		Placement->CancelSelection();
		ABDPalace* Palace = nullptr;
		for (TActorIterator<ABDPalace> It(World); It; ++It) { if (!PalacesBefore.Contains(*It)) { Palace = *It; } }
		int32 Held = 0;
		for (int32 DY = 0; DY < 2; ++DY)
		{
			for (int32 DX = 0; DX < 2; ++DX)
			{
				Held += Grid->GetCellState(FBDCellCoord(PalaceCell.X + DX, PalaceCell.Y + DY)) == EBDCellState::Platform ? 1 : 0;
			}
		}
		Check(TEXT("PALACIO"), TEXT("the palace goes down on 2x2 cells held like a platform, for its price"),
			bPalace && Palace != nullptr && Held == 4 && Match->GetPublicMoney() == MoneyBefore - PalacePrice,
			FString::Printf(TEXT("%s at %s, %d of 4 cells held, money %d -> %d"), bPalace ? TEXT("placed") : TEXT("refused"),
				*PalaceCell.ToString(), Held, MoneyBefore, Match->GetPublicMoney()));
		if (Palace == nullptr)
		{
			break;
		}

		const UBDPalaceData* PalaceData = Palace->GetData();
		const UStaticMeshComponent* PalaceMesh = Palace->GetMeshComponent();
		const float RoofZ = PalaceMesh->Bounds.Origin.Z + PalaceMesh->Bounds.BoxExtent.Z;
		const float Span = FMath::Max(PalaceMesh->Bounds.BoxExtent.X, PalaceMesh->Bounds.BoxExtent.Y) * 2.0f;
		Check(TEXT("PALACIO"), TEXT("the palace wears a mesh fitted to its footprint, the stars over the roof at the data's level"),
			PalaceData != nullptr && PalaceMesh->GetStaticMesh() != nullptr && Palace->GetPalaceLevel() == PalaceData->Level
				&& Span <= Grid->GetCellSize() * 2.0f + 1.0f && Span >= Grid->GetCellSize() && Palace->GetStarsAnchor().Z > RoofZ,
			FString::Printf(TEXT("mesh %s %.0f cm wide over %.0f cm, level %d (data %d), stars %.0f cm over the roof"),
				*GetNameSafe(PalaceMesh->GetStaticMesh()), Span, Grid->GetCellSize() * 2.0f, Palace->GetPalaceLevel(),
				PalaceData != nullptr ? PalaceData->Level : -1, Palace->GetStarsAnchor().Z - RoofZ));

		// The stars fade with the camera's distance: whole close, gone in the overview the
		// match opens on, eased in between rather than cut.
		const UBDUISettings& UI = UBDUISettings::Get();
		const float Near = UI.GetPalaceStarOpacity(UI.PalaceStarFadeStart);
		const float Middle = UI.GetPalaceStarOpacity((UI.PalaceStarFadeStart + UI.PalaceStarFadeEnd) * 0.5f);
		const float Far = UI.GetPalaceStarOpacity(UI.PalaceStarFadeEnd);
		const APlayerController* Viewer = World->GetFirstPlayerController();
		const float Overview = Viewer != nullptr && Viewer->PlayerCameraManager != nullptr
			? FVector::Dist(Viewer->PlayerCameraManager->GetCameraLocation(), Palace->GetStarsAnchor()) : -1.0f;
		Check(TEXT("PALACIO"), TEXT("the stars fade out with the camera's distance, gone in the opening overview"),
			FMath::IsNearlyEqual(Near, 1.0f) && FMath::IsNearlyEqual(Far, 0.0f) && Middle > 0.1f && Middle < 0.9f
				&& Overview > 0.0f && UI.GetPalaceStarOpacity(Overview) <= KINDA_SMALL_NUMBER,
			FString::Printf(TEXT("opacity %.2f at %.0f cm, %.2f halfway, %.2f at %.0f cm; overview camera %.0f cm away, %.2f"),
				Near, UI.PalaceStarFadeStart, Middle, Far, UI.PalaceStarFadeEnd, Overview, Overview > 0.0f ? UI.GetPalaceStarOpacity(Overview) : -1.0f));

		// Its Agent: out of the door on a full bar with the pistol, on a cell beside the palace.
		ABDAgent* Agent = Palace->GetAgent();
		const TWeakObjectPtr<ABDAgent> AgentRef = Agent;
		FBDCellCoord AgentCell;
		const bool bAgentOnBoard = Agent != nullptr && Grid->WorldToCell(Agent->GetActorLocation(), AgentCell);
		const bool bBeside = bAgentOnBoard && AgentCell.X >= PalaceCell.X - 1 && AgentCell.X <= PalaceCell.X + 2
			&& AgentCell.Y >= PalaceCell.Y - 1 && AgentCell.Y <= PalaceCell.Y + 2
			&& Grid->GetCellState(AgentCell) != EBDCellState::Platform;
		Check(TEXT("AGENTE"), TEXT("the palace sends out one Agent, beside it, on a full bar"),
			Agent != nullptr && bBeside && FMath::IsNearlyEqual(Agent->GetPatrolFraction(), 1.0f, 0.01f) && !Agent->IsAsleep(),
			Agent != nullptr ? Agent->Describe() : FString(TEXT("no agent")));

		// One weapon a level, the pistol designed, the kill bonus 1 s down 0.15 s a star.
		FString Bonuses;
		bool bWeaponsRight = PalaceData != nullptr && PalaceData->Weapons.Num() == UBDPalaceData::MaxLevel + 1 && !PalaceData->Weapons[0].bPlaceholder;
		for (int32 WeaponIndex = 0; PalaceData != nullptr && WeaponIndex < PalaceData->Weapons.Num(); ++WeaponIndex)
		{
			bWeaponsRight &= FMath::IsNearlyEqual(PalaceData->Weapons[WeaponIndex].KillBonusSeconds, 1.0f - 0.15f * WeaponIndex, 0.001f);
			Bonuses += FString::Printf(TEXT("%s%.2f"), WeaponIndex > 0 ? TEXT(" ") : TEXT(""), PalaceData->Weapons[WeaponIndex].KillBonusSeconds);
		}
		Check(TEXT("AGENTE"), TEXT("six weapons, the pistol designed, the kill bonus from 1 s to 0.25 s"), bWeaponsRight,
			FString::Printf(TEXT("%d weapon(s), bonus %s, pistol %.0f damage at %.1f/s"), PalaceData != nullptr ? PalaceData->Weapons.Num() : 0, *Bonuses,
				PalaceData != nullptr && PalaceData->Weapons.Num() > 0 ? PalaceData->Weapons[0].Damage : 0.0f,
				PalaceData != nullptr && PalaceData->Weapons.Num() > 0 ? PalaceData->Weapons[0].FireRate : 0.0f));

		if (Agent != nullptr)
		{
			// A kill puts the pistol's second back; the bar runs out and he heads home.
			const float Before = Agent->GetPatrolRemaining();
			Agent->NotifyKill();
			const float Earned = Agent->GetPatrolRemaining() - Before;
			const bool bKicked = Agent->Kick();
			Agent->DebugSetPatrolRemaining(0.0f);
			const EBDAgentState AfterDrain = Agent->GetState();
			Check(TEXT("AGENTE"), TEXT("a kill adds 1 s, the kick plays, an empty bar sends him home"),
				FMath::IsNearlyEqual(Earned, 1.0f, 0.001f) && bKicked
					&& (AfterDrain == EBDAgentState::Returning || AfterDrain == EBDAgentState::Sleeping),
				FString::Printf(TEXT("+%.2fs, kick %s, then %s"), Earned, bKicked ? TEXT("on") : TEXT("refused"),
					*StaticEnum<EBDAgentState>()->GetNameStringByValue(static_cast<int64>(AfterDrain))));
		}

		Placement->SetHoveredCellDirect(PalaceCell);
		const bool bSold = Placement->TryRemoveAtHovered();
		Check(TEXT("AGENTE"), TEXT("selling the palace takes the Agent with it"), Agent != nullptr && !AgentRef.IsValid(),
			AgentRef.IsValid() ? TEXT("still on the board") : TEXT("gone"));
		int32 Freed = 0;
		for (int32 DY = 0; DY < 2; ++DY)
		{
			for (int32 DX = 0; DX < 2; ++DX)
			{
				Freed += Grid->GetCellState(FBDCellCoord(PalaceCell.X + DX, PalaceCell.Y + DY)) == EBDCellState::Free ? 1 : 0;
			}
		}
		Check(TEXT("PALACIO"), TEXT("the palace sells whole from any of its cells"), bSold && Freed == 4,
			FString::Printf(TEXT("sold %s, %d of 4 cells free"), bSold ? TEXT("yes") : TEXT("no"), Freed));
		break;
	}

	case 5:
	{
		// The wave of the first candidate: he is out before any creep, the creeps held back.
		// He steps out of a bus once the buses have parked, so the step waits for him.
		const UBDBusSubsystem* BusesAtDeal = GetWorld()->GetSubsystem<UBDBusSubsystem>();
		if (!bCandidateWaveDealt)
		{
			bCandidateWaveDealt = true;
			SpawnedBeforeCandidate = Waves->GetMatchTotals().CreepsSpawned;
			Match->DebugSetWave(FMath::Max(0, UBDGameBalanceSettings::Get().CandidateInterval - 1));
			Match->CallWaveEarly();
			BusWaitAtDeal = BusesAtDeal != nullptr ? BusesAtDeal->GetParkRemaining() : 0.0f;
			StepTicks = 0;
		}
		if (Candidates->IsAwaitingBus())
		{
			if (Candidates->HasCandidateOnBoard() || ++StepTicks > MaxWaitTicks * 10)
			{
				Check(TEXT("ONIBUS"), TEXT("the candidate waits for the buses to park"), false,
					Candidates->HasCandidateOnBoard() ? TEXT("he is out while still waiting") : TEXT("he never stepped out"));
				return false;
			}
			return true;
		}
		const float BusLeft = BusesAtDeal != nullptr ? BusesAtDeal->GetParkRemaining() : 0.0f;
		Check(TEXT("ONIBUS"), TEXT("the candidate waits for the buses to park"),
			Candidates->HasCandidateOnBoard() && BusLeft <= 0.0f,
			FString::Printf(TEXT("buses had %.1fs to go at the deal, %.1fs left when he stepped out, %d tick(s) waited"), BusWaitAtDeal, BusLeft, StepTicks));
		StepTicks = 0;
		const int32 SpawnedBefore = SpawnedBeforeCandidate;
		Check(TEXT("CANDIDATO"), TEXT("the candidate walks out first on his wave"),
			Candidates->HasCandidateOnBoard() && Waves->GetMatchTotals().CreepsSpawned == SpawnedBefore && Waves->GetSpawnHoldRemaining() > 0.0f,
			FString::Printf(TEXT("wave %d, candidate %s, creeps out %d, creeps held %.1fs"), Match->GetCurrentWave(),
				Candidates->HasCandidateOnBoard() ? TEXT("out") : TEXT("missing"), Waves->GetMatchTotals().CreepsSpawned - SpawnedBefore, Waves->GetSpawnHoldRemaining()));

		// The mouths have slid for this wave; every bus goes with its own, and none is left
		// behind. The map parks one over each mouth.
		if (const UBDBusSubsystem* Buses = GetWorld()->GetSubsystem<UBDBusSubsystem>())
		{
			int32 Bound = 0, Astray = 0;
			const TArray<FBDSpawnPoint>& Points = Waves->GetSpawnPoints();
			Buses->CountBuses(Points, Bound, Astray);
			Check(TEXT("ONIBUS"), TEXT("every mouth has its bus, headed to where the mouth slid"),
				Bound == Points.Num() && Points.Num() > 0 && Astray == 0,
				FString::Printf(TEXT("%d of %d mouths with a bus, %d astray"), Bound, Points.Num(), Astray));

			// No two buses closer than the gap, the same edge or across a corner. The map's
			// anchors all stand further apart than that, so nothing excuses a closer pair.
			const float MinGap = Buses->GetMinGapCells(Points);
			Check(TEXT("ONIBUS"), TEXT("no two buses stand closer than MinBusGap, same edge or across a corner"),
				MinGap >= UBDWaveSettings::Get().MinBusGap,
				FString::Printf(TEXT("closest pair %.2f cell(s) apart, %.1f needed"), MinGap, UBDWaveSettings::Get().MinBusGap));
		}

		// Under a wave nothing new goes down - no kind of piece, not through the bar and not
		// through the gesture itself - but what stands can still be evolved. Confirmed rule
		// (briefing 2026-09-23 23:00): building is the setup's decision, evolving the reaction.
		FString Offered;
		for (const TSoftObjectPtr<UBDPlaceableData>& Entry : UBDPlacementSettings::Get().Palette)
		{
			const UBDPlaceableData* Piece = Entry.LoadSynchronous();
			if (Piece != nullptr && Placement->GetHandRefusal(Piece) == EBDPlacementRefusal::None)
			{
				Offered += Offered.IsEmpty() ? Piece->GetName() : TEXT(", ") + Piece->GetName();
			}
		}
		const int32 BuiltBefore = Match->GetLedger().PiecesBuilt;
		const int32 MoneyBefore = Match->GetPublicMoney();
		Placement->SelectPlaceable(Tower);
		FBDCellCoord Probe;
		bool bSlipped = false;
		for (int32 DX = -3; DX <= 3 && !bSlipped; ++DX)
		{
			Probe = FBDCellCoord(UrnCell.X + DX, UrnCell.Y + 2);
			Placement->SetHoveredCellDirect(Probe);
			bSlipped = Placement->TryPlaceAtHovered();
		}
		const EBDPlacementRefusal Gesture = Placement->GetCurrentRefusal();
		Placement->CancelSelection();
		Check(TEXT("COLOCACAO"), TEXT("no piece of any kind can be built while a wave is out"),
			Offered.IsEmpty() && !bSlipped && Match->GetLedger().PiecesBuilt == BuiltBefore && Match->GetPublicMoney() == MoneyBefore,
			FString::Printf(TEXT("offered by the bar: %s; a tower through the gesture: %s (%s)"), Offered.IsEmpty() ? TEXT("none") : *Offered,
				bSlipped ? TEXT("PLACED") : TEXT("refused"), *RefusalName(Gesture)));
		const int32 LevelBefore = GroundTower.IsValid() ? GroundTower->GetTowerLevel() : 0;
		Placement->SetHoveredCellDirect(TowerCell);
		Placement->DebugClick();
		Placement->DebugClick();
		const int32 LevelAfter = GroundTower.IsValid() ? GroundTower->GetTowerLevel() : 0;
		Check(TEXT("COLOCACAO"), TEXT("a defender is evolved by clicking it during a wave"), LevelAfter == LevelBefore + 1,
			FString::Printf(TEXT("level %d -> %d"), LevelBefore, LevelAfter));

		// A creep of the wave with an animated body wears it: the skeletal mesh, its loop
		// and its material, standing a person tall times the scale on the data. Its feet
		// are checked on the next step.
		const UBDEnemyData* WaveEnemy = UBDWaveSettings::Get().ResolveWaveEnemy();
		if (WaveEnemy != nullptr && !WaveEnemy->SkeletalMesh.IsNull())
		{
			ABDEnemyBase* Creep = Waves->SpawnEnemy(WaveEnemy, 0);
			AnimatedCreep = Creep;
			const USkeletalMeshComponentBudgeted* Body = Creep != nullptr ? Creep->GetSkeletalBody() : nullptr;
			const bool bWorn = Body != nullptr && Body->GetSkeletalMeshAsset() != nullptr && Creep->GetBody() == Body
				&& Body->GetSingleNodeInstance() != nullptr && Body->GetSingleNodeInstance()->GetAnimationAsset() != nullptr
				&& (WaveEnemy->GetSkinCount() == 0 || WaveEnemy->IsSkin(Body->GetMaterial(0)));
			const float Height = Body != nullptr ? Body->Bounds.BoxExtent.Z * 2.0f : 0.0f;
			const float Scale = FMath::Max(0.01f, static_cast<float>(WaveEnemy->MeshScale.Z));
			Check(TEXT("INIMIGO"), TEXT("an animated creep wears its skeletal body, loop and material, 150-250 cm tall per unit of scale"),
				bWorn && Height >= 150.0f * Scale && Height <= 250.0f * Scale,
				FString::Printf(TEXT("%s, %s, %.0f cm tall at scale %.2f, %.0f-%.0f accepted"), Body != nullptr ? *GetNameSafe(Body->GetSkeletalMeshAsset()) : TEXT("no body"),
					Body != nullptr ? *GetNameSafe(Body->GetMaterial(0)) : TEXT("-"), Height, Scale, 150.0f * Scale, 250.0f * Scale));

			// The skins: every one in the pool loads, and a horde's worth of draws wears
			// them all. 200 draws miss one of four skins about once in 10^24.
			if (WaveEnemy->GetSkinCount() > 0)
			{
				TMap<const UMaterialInterface*, int32> Drawn;
				for (int32 Draw = 0; Draw < 200; ++Draw)
				{
					++Drawn.FindOrAdd(WaveEnemy->PickSkin());
				}
				FString Tally;
				for (const TPair<const UMaterialInterface*, int32>& Entry : Drawn)
				{
					Tally += FString::Printf(TEXT("%s%s %d"), Tally.IsEmpty() ? TEXT("") : TEXT(", "), *GetNameSafe(Entry.Key), Entry.Value);
				}
				Check(TEXT("INIMIGO"), TEXT("the horde wears every skin of the pool"),
					!Drawn.Contains(nullptr) && Drawn.Num() == WaveEnemy->GetSkinCount(),
					FString::Printf(TEXT("%d skin(s) in the pool, 200 draws: %s"), WaveEnemy->GetSkinCount(), *Tally));
			}

			// Its sound: the words and the call through the effects class, so the options
			// own them, and a step on its loop wherever a foot lands. Sound itself cannot be
			// heard in a -nosound run; the wiring can be checked.
			if (WaveEnemy->IsVocal() || !WaveEnemy->FootstepSound.IsNull())
			{
				const USoundClass* Effects = UBDUISettings::Get().EffectsSoundClass.LoadSynchronous();
				int32 Routed = 0;
				int32 Sounds = 0;
				for (const TSoftObjectPtr<USoundBase>& Each : { WaveEnemy->SpeechSound, WaveEnemy->CallSound, WaveEnemy->FootstepSound, WaveEnemy->DeathSound, WaveEnemy->BodyFallSound })
				{
					if (const USoundBase* Sound = Each.LoadSynchronous())
					{
						++Sounds;
						Routed += Sound->GetSoundClass() == Effects ? 1 : 0;
					}
				}
				int32 Steps = 0;
				if (const UAnimSequenceBase* Loop = WaveEnemy->MoveAnimation.LoadSynchronous())
				{
					for (const FAnimNotifyEvent& Event : Loop->Notifies)
					{
						Steps += Cast<UBDAnimNotify_Footstep>(Event.Notify) != nullptr ? 1 : 0;
					}
				}
				// Every fall lands with a thud, when the data has one.
				int32 FallsLanding = 0;
				for (const TSoftObjectPtr<UAnimSequenceBase>& Each : WaveEnemy->DeathAnimations)
				{
					const UAnimSequenceBase* Fall = Each.LoadSynchronous();
					FallsLanding += Fall != nullptr && Fall->Notifies.ContainsByPredicate([](const FAnimNotifyEvent& Event)
					{
						return Cast<UBDAnimNotify_BodyFall>(Event.Notify) != nullptr;
					}) ? 1 : 0;
				}
				Check(TEXT("SOM"), TEXT("a vocal creep's sounds go through the effects class, its loop steps where the feet land and every fall lands with a thud"),
					Sounds > 0 && Routed == Sounds && (WaveEnemy->FootstepSound.IsNull() || Steps > 0)
						&& (WaveEnemy->BodyFallSound.IsNull() || FallsLanding == WaveEnemy->DeathAnimations.Num()),
					FString::Printf(TEXT("%d of %d sounds on %s, %d step notifies on the loop, %d of %d falls land"), Routed, Sounds, *GetNameSafe(Effects), Steps,
						FallsLanding, WaveEnemy->DeathAnimations.Num()));
			}
		}
		break;
	}

	case 6:
	{
		// The feet follow the route: the loop rate is the creep's speed over the speed the
		// loop was made at, so it neither skates nor pedals. Then the creep goes.
		if (ABDEnemyBase* Creep = AnimatedCreep.Get())
		{
			const USkeletalMeshComponentBudgeted* Body = Creep->GetSkeletalBody();
			const UBDWaveSettings& WaveSettings = UBDWaveSettings::Get();
			const float Rate = Body != nullptr ? Body->GetPlayRate() : -1.0f;
			const float Expected = Creep->GetCurrentSpeed() <= 0.0f ? 0.0f
				: FMath::Clamp(Creep->GetCurrentSpeed() / WaveSettings.AnimReferenceSpeed, WaveSettings.AnimMinPlayRate, WaveSettings.AnimMaxPlayRate);
			Check(TEXT("INIMIGO"), TEXT("the walk loop plays at the creep's speed over the reference speed"),
				Creep->GetCurrentSpeed() > 0.0f && FMath::IsNearlyEqual(Rate, Expected, 0.01f),
				FString::Printf(TEXT("speed %.0f cm/s, rate %.2f, expected %.2f"), Creep->GetCurrentSpeed(), Rate, Expected));

			// Killed, not just removed: the kill board counts it under its kind, whatever
			// its skin, and nothing else.
			const UBDEnemyData* CreepData = Creep->GetData();
			const FName Type = CreepData != nullptr ? CreepData->GetKillType() : NAME_None;
			const auto KillsOf = [Waves](const FName Kind)
			{
				const FBDKillTally* Tally = Waves->GetMatchTotals().KillsByType.FindByPredicate([Kind](const FBDKillTally& Each) { return Each.Type == Kind; });
				return Tally != nullptr ? Tally->Kills : 0;
			};
			const int32 KindsBefore = Waves->GetMatchTotals().KillsByType.Num();
			const int32 KillsBefore = KillsOf(Type);
			int32 CorpsesBefore = 0;
			for (ABDCreepCorpse* Each : TActorRange<ABDCreepCorpse>(GetWorld()))
			{
				++CorpsesBefore;
			}
			Creep->Kill();

			// Its fall is show only: the creep is gone from the game on the spot (nothing
			// may aim at it or wait on it), and what is left is a body that is not a creep
			// and blocks nothing.
			if (CreepData != nullptr && CreepData->DeathAnimations.Num() > 0)
			{
				int32 CorpsesAfter = 0;
				bool bInert = true;
				for (ABDCreepCorpse* Each : TActorRange<ABDCreepCorpse>(GetWorld()))
				{
					++CorpsesAfter;
					for (const UActorComponent* Component : Each->GetComponents())
					{
						const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
						bInert &= Primitive == nullptr || Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision;
					}
				}
				const bool bGone = !IsValid(Creep) || Creep->IsActorBeingDestroyed();
				Check(TEXT("INIMIGO"), TEXT("a kill leaves a fall to watch, the creep itself out of the game at once"),
					bGone && CorpsesAfter == CorpsesBefore + 1 && bInert && !Waves->GetLivingEnemiesRef().Contains(Creep),
					FString::Printf(TEXT("creep %s, bodies %d -> %d, %s"), bGone ? TEXT("gone") : TEXT("still there"),
						CorpsesBefore, CorpsesAfter, bInert ? TEXT("no collision") : TEXT("a body collides")));
			}
			const int32 KindsAfter = Waves->GetMatchTotals().KillsByType.Num();
			Check(TEXT("PLACAR"), TEXT("a kill counts once under its kind on the kill board"),
				KillsOf(Type) == KillsBefore + 1 && KindsAfter - KindsBefore == (KillsBefore == 0 ? 1 : 0),
				FString::Printf(TEXT("%s %d -> %d, %d kind(s) on the board"), *Type.ToString(), KillsBefore, KillsOf(Type), KindsAfter));
		}
		AnimatedCreep.Reset();

		// The scheduled candidate killed pays his drop, all of it.
		ABDCandidate* Candidate = Candidates->GetCandidate();
		if (Candidate == nullptr)
		{
			Check(TEXT("CANDIDATO"), TEXT("killing a scheduled candidate pays the bribe"), false, TEXT("no candidate on the board"));
			return false;
		}
		const int32 Expected = Match->GetCandidateFunds(Match->GetCurrentWave());
		const int32 EarnedBefore = Match->GetLedger().BribeEarned;
		const int32 FallenBefore = Candidates->GetFallenCount();
		Candidate->ApplyDamage(Candidate->GetMaxHealth() * 10.0f, nullptr);
		Bribes->FlushNow(TEXT("BD.Test.Regression"));
		const int32 Earned = Match->GetLedger().BribeEarned - EarnedBefore;
		Check(TEXT("CANDIDATO"), TEXT("killing a scheduled candidate pays his bribe"), Earned == Expected && Expected > 0,
			FString::Printf(TEXT("%d paid, %d expected"), Earned, Expected));
		Check(TEXT("PLACAR"), TEXT("a candidate brought down counts once on the candidates' board"),
			Candidates->GetFallenCount() == FallenBefore + 1,
			FString::Printf(TEXT("%d -> %d brought down"), FallenBefore, Candidates->GetFallenCount()));

		// The fallen brought back: they parade in one at a time, on the subsystem's tick.
		Candidates->BeginReturn(TEXT("BD.Test.Regression"));
		StepTicks = 0;
		break;
	}

	case 7:
	{
		// Waits for the parade to send its first.
		TArray<ABDCandidate*> Living;
		Candidates->GetLivingCandidates(Living);
		ABDCandidate* Returning = nullptr;
		for (ABDCandidate* Candidate : Living)
		{
			if (Candidate != nullptr && Candidate->bReturning) { Returning = Candidate; }
		}
		if (Returning == nullptr)
		{
			if (++StepTicks > MaxWaitTicks)
			{
				Check(TEXT("CANDIDATO"), TEXT("a candidate of the parade pays no bribe"), false, TEXT("no candidate came back"));
				return false;
			}
			return true;
		}

		const int32 EarnedBefore = Match->GetLedger().BribeEarned;
		Returning->ApplyDamage(Returning->GetMaxHealth() * 10.0f, nullptr);
		Bribes->FlushNow(TEXT("BD.Test.Regression"));
		Check(TEXT("CANDIDATO"), TEXT("a candidate of the parade pays no bribe"), Match->GetLedger().BribeEarned == EarnedBefore,
			FString::Printf(TEXT("%d paid"), Match->GetLedger().BribeEarned - EarnedBefore));
		break;
	}

	case 8:
	{
		// The money closes: what came in less what went out is what is held.
		const FBDMatchLedger& Ledger = Match->GetLedger();
		const int64 Gap = static_cast<int64>(Ledger.StartingFunds) + Ledger.BribeEarned + Ledger.Refunded + Ledger.Granted
			- Ledger.SpentBuild - Ledger.SpentMove - Ledger.SpentEvolve - Match->GetPublicMoney() - Match->GetBribeHeld();
		Check(TEXT("ECONOMIA"), TEXT("the ledger closes: LedgerGap is 0"), Gap == 0,
			FString::Printf(TEXT("gap %lld over %d built, %d levels, %d earned, %d refunded"), Gap, Ledger.PiecesBuilt, Ledger.LevelsBought, Ledger.BribeEarned, Ledger.Refunded));
		Check(TEXT("PLACAR"), TEXT("blue votes never go down but through the levelling after a parade"), UnexplainedDrops == 0,
			FString::Printf(TEXT("%d unexplained drop(s), %d levelling(s)"), UnexplainedDrops, LevelDownsSeen));

		// The endless group: with the step brought down to 1, the one fallen so far makes it
		// two walking out together, each out of a mouth of his own while there are mouths.
		{
			UBDGameBalanceSettings* Balance = GetMutableDefault<UBDGameBalanceSettings>();
			const int32 SavedStep = Balance->CandidateGroupStep;
			Balance->CandidateGroupStep = 1;
			const int32 Expected = Balance->GetCandidateGroupSize(Candidates->GetFallenCount());
			int32 Mouths = 0;
			const int32 Out = Candidates->SpawnCandidateGroup(TEXT("BD.Test.Regression"), &Mouths);
			Balance->CandidateGroupStep = SavedStep;
			const int32 Routed = Waves->GetSpawnPoints().FilterByPredicate([](const FBDSpawnPoint& Point) { return Point.Route.Num() > 0; }).Num();
			const bool bFormula = Balance->GetCandidateGroupSize(19) == 1 && Balance->GetCandidateGroupSize(20) == 2 && Balance->GetCandidateGroupSize(40) == 3 && Balance->GetCandidateGroupSize(60) == 4;
			Check(TEXT("CANDIDATO"), TEXT("in the endless the candidates walk out in groups, one more per 20 killed, each out of his own mouth"),
				bFormula && Expected >= 2 && Out == Expected && Mouths == FMath::Min(Out, Routed),
				FString::Printf(TEXT("%d of %d out of %d mouth(s), %d routed, formula %s"), Out, Expected, Mouths, Routed, bFormula ? TEXT("ok") : TEXT("off")));
		}

		// Last, because it ends the match: a candidate at the urn is the defeat.
		ABDCandidate* Walker = Candidates->SpawnCandidate(TEXT("BD.Test.Regression"));
		if (Walker == nullptr)
		{
			Check(TEXT("CANDIDATO"), TEXT("a candidate at the urn is the defeat"), false, TEXT("no candidate could be sent"));
			return false;
		}
		Walker->DebugArrive();
		Check(TEXT("CANDIDATO"), TEXT("a candidate at the urn is the defeat"), Match->GetPhase() == EBDMatchPhase::Defeat,
			StaticEnum<EBDMatchPhase>()->GetNameStringByValue(static_cast<int64>(Match->GetPhase())));

		// The wave the match ended on has its row, and the money of that stretch closes.
		const FBDWaveLogMark& WaveMark = Match->GetLedger().WaveMark;
		Check(TEXT("RELATORIO"), TEXT("the wave log writes the wave the match ended on, and its money closes"),
			WaveMark.Wave == Match->GetCurrentWave() && WaveMark.LastFundsGap == 0,
			FString::Printf(TEXT("last row on wave %d of %d, gap %lld"), WaveMark.Wave, Match->GetCurrentWave(), WaveMark.LastFundsGap));

		Finish(FString());
		return true;
	}

	default:
		Finish(FString());
		return true;
	}

	++Step;
	return true;
}

namespace BDRegressionCommands
{
	static void ExecRegression(const TArray<FString>& Args, UWorld* World)
	{
		UBDRegressionSubsystem* Regression = World != nullptr ? World->GetSubsystem<UBDRegressionSubsystem>() : nullptr;
		if (Regression == nullptr)
		{
			UE_LOG(LogBDDebug, Error, TEXT("BD.Test.Regression needs a game world."));
			return;
		}
		Regression->Start(Args.Num() > 0 && FCString::Atoi(*Args[0]) != 0);
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdRegression(
		TEXT("BD.Test.Regression"),
		TEXT("BD.Test.Regression [quit 0|1]: on a fresh match, checks every mechanic that already works and prints PASS/FAIL per line. Ends the match in a defeat."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecRegression));
}

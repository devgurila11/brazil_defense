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
#include "Grid/BDGridAudit.h"
#include "Grid/BDGridSubsystem.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Match/BDDifficultyData.h"
#include "Match/BDGameBalanceSettings.h"
#include "Match/BDMatchManager.h"
#include "Objective/BDObjective.h"
#include "Obstacle/BDObstacleGenerator.h"
#include "Animation/AnimSequenceBase.h"
#include "Day/BDDayCycleComponent.h"
#include "Day/BDDaySettings.h"
#include "Day/BDStreetLamp.h"
#include "Day/BDStreetLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/Button.h"
#include "UI/BDHUDWidget.h"
#include "Components/DecalComponent.h"
#include "Engine/Texture2D.h"
#include "Enemy/BDBloodDecals.h"
#include "Tower/BDShooter.h"
#include "Tower/BDShooterData.h"
#include "Tower/BDShotSound.h"
#include "Tower/BDTowerData.h"
#include "UI/BDMatchHUD.h"
#include "UObject/UObjectIterator.h"
#include "Audio/BDAudioSettings.h"
#include "Audio/BDSoundscapeSubsystem.h"
#include "Audio/BDSpeechMarks.h"
#include "Components/AudioComponent.h"
#include "Palace/BDAgent.h"
#include "Palace/BDAnimNotify_Shot.h"
#include "Placement/BDInspection.h"
#include "Palace/BDPalace.h"
#include "Palace/BDPalaceData.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Objective/BDObjectiveSettings.h"
#include "Objective/BDObjectiveSubsystem.h"
#include "Placement/BDPlaceableData.h"
#include "SkeletalMeshComponentBudgeted.h"
#include "Grid/BDGridDebug.h"
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

bool UBDRegressionSubsystem::MoveNear(const FBDCellCoord& Near, const int32 Radius, FBDCellCoord& OutCell)
{
	UBDPlacementComponent* Placement = GetPlacement();
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
	return false;
}

void UBDRegressionSubsystem::CheckCellsComeBack(UBDPlaceableData* Tower, UBDPlaceableData* Platform, UBDPlaceableData* Divider)
{
	using namespace BDRegressionPrivate;

	UWorld* World = GetWorld();
	UBDPlacementComponent* Placement = GetPlacement();
	UBDGridSubsystem* Grid = UBDGridSubsystem::Get(World);

	// The board as the match dealt it, before the player touches it: every taken cell has
	// something standing on it. The generated obstacles were Blocked cells with no body at
	// all, so with the debug grid off the ghost went red over cells that looked free; they
	// are off now, and the board starts clean.
	const FBDGridAuditReport Opening = BDGridAudit::Run(*World);
	if (!Opening.IsClean()) { BDGridAudit::LogReport(Opening); }
	Check(TEXT("CELULAS"), TEXT("every taken cell of the board has something on it, and something to see"),
		Opening.IsClean() && Opening.CellsTaken > 0, Opening.Summary());
	const UBDObstacleGenerator* Generator = World->GetSubsystem<UBDObstacleGenerator>();
	const int32 Obstacles = Generator != nullptr ? Generator->GetGeneratedCells().Num() : -1;
	Check(TEXT("CELULAS"), TEXT("the board starts clean: no generated obstacle"), Obstacles == 0,
		FString::Printf(TEXT("%d generated obstacle cell(s)"), Obstacles));

	// Cells of a rectangle still taken, except those of another one the same size (where the piece went).
	const auto CountTaken = [Grid](const FBDCellCoord& Origin, const FIntPoint& Span, const FBDCellCoord& SkipOrigin, const bool bSkip)
	{
		int32 Taken = 0;
		for (int32 Y = 0; Y < Span.Y; ++Y)
		{
			for (int32 X = 0; X < Span.X; ++X)
			{
				const FBDCellCoord Cell(Origin.X + X, Origin.Y + Y);
				const bool bInSkip = bSkip && Cell.X >= SkipOrigin.X && Cell.X < SkipOrigin.X + Span.X
					&& Cell.Y >= SkipOrigin.Y && Cell.Y < SkipOrigin.Y + Span.Y;
				Taken += !bInSkip && Grid->GetCellState(Cell) != EBDCellState::Free ? 1 : 0;
			}
		}
		return Taken;
	};

	int32 Left = 0;
	int32 Dirty = 0;
	int32 Done = 0;
	FString Trail;
	const auto Audit = [&Dirty, World]()
	{
		const FBDGridAuditReport Report = BDGridAudit::Run(*World);
		if (!Report.IsClean())
		{
			++Dirty;
			BDGridAudit::LogReport(Report);
		}
	};

	UBDPlaceableData* Pieces[] = { Tower, Platform, FindPiece(EBDPieceKind::Palace) };
	for (UBDPlaceableData* Piece : Pieces)
	{
		FBDCellCoord Cell;
		if (Piece == nullptr || !PlaceNear(Piece, UrnCell, 12, Cell))
		{
			Trail += FString::Printf(TEXT(" %s not built;"), *GetNameSafe(Piece));
			Placement->CancelSelection();
			continue;
		}
		Placement->CancelSelection();
		const FBDPlacedPiece* Placed = Placement->GetPlacedByCell().Find(Cell);
		if (Placed == nullptr)
		{
			Trail += FString::Printf(TEXT(" %s built but not on its cell;"), *Piece->GetName());
			continue;
		}
		const FBDCellCoord From = Placed->Origin;
		const FIntPoint Span(FMath::Max(1, Placed->Footprint.X), FMath::Max(1, Placed->Footprint.Y));
		Audit();

		// Lifted from its far corner, so a wide piece is picked up by any of its cells.
		Placement->SetHoveredCellDirect(FBDCellCoord(From.X + Span.X - 1, From.Y + Span.Y - 1));
		FBDCellCoord To = From;
		bool bMoved = false;
		if (Placement->TryBeginMoveAtHovered())
		{
			bMoved = MoveNear(From, 12, To);
			if (!bMoved)
			{
				Placement->CancelMove();
			}
		}
		const FBDPlacedPiece* Moved = Placement->GetPlacedByCell().Find(To);
		To = Moved != nullptr ? Moved->Origin : To;
		Left += bMoved ? CountTaken(From, Span, To, true) : 0;
		Audit();

		AActor* Actor = Moved != nullptr && Moved->Actors.Num() > 0 ? Moved->Actors[0].Get() : nullptr;
		const bool bSold = Actor != nullptr && Placement->TrySellActor(Actor);
		Left += CountTaken(To, Span, To, false);
		Audit();

		Done += bMoved && bSold ? 1 : 0;
		Trail += FString::Printf(TEXT(" %s %dx%d %s -> %s %s;"), *Piece->GetName(), Span.X, Span.Y, *From.ToString(),
			bMoved ? *To.ToString() : TEXT("(not moved)"), bSold ? TEXT("sold") : TEXT("NOT SOLD"));
	}

	// The divider lives on edges: built, moved and sold, every edge it held goes free.
	{
		TArray<FBDEdgeCoord> BlockedBefore;
		Grid->GetBlockedEdges(BlockedBefore);
		bool bBuilt = false;
		FBDEdgeCoord Edge;
		if (Divider != nullptr && Placement->TakeIntoHand(Divider))
		{
			for (int32 DX = 3; DX <= 12 && !bBuilt; ++DX)
			{
				Edge = FBDEdgeCoord(FBDCellCoord(UrnCell.X - DX, UrnCell.Y), FBDEdgeCoord::DirectionY);
				bBuilt = PlaceFence(Edge);
			}
		}
		Placement->CancelSelection();
		bool bMoved = false;
		bool bSold = false;
		if (bBuilt)
		{
			Placement->SetHoveredEdgeDirect(Edge);
			if (Placement->TryBeginMoveAtHovered())
			{
				for (int32 DY = 2; DY <= 6 && !bMoved; ++DY)
				{
					Placement->SetHoveredEdgeDirect(FBDEdgeCoord(FBDCellCoord(Edge.Cell.X, Edge.Cell.Y + DY), Edge.Direction));
					bMoved = Placement->IsCurrentPlacementValid() && Placement->TryPlaceAtHovered();
				}
				if (!bMoved)
				{
					Placement->CancelMove();
				}
			}
			Audit();

			// Sold by whatever edge it ended up on: every edge blocked now that was not before.
			TArray<FBDEdgeCoord> Now;
			Grid->GetBlockedEdges(Now);
			for (const FBDEdgeCoord& Each : Now)
			{
				if (!BlockedBefore.Contains(Each))
				{
					Placement->SetHoveredEdgeDirect(Each);
					bSold = Placement->TryRemoveAtHovered() || bSold;
				}
			}
		}
		TArray<FBDEdgeCoord> After;
		Grid->GetBlockedEdges(After);
		const int32 EdgesLeft = After.Num() - BlockedBefore.Num();
		Left += FMath::Max(0, EdgesLeft);
		Done += bMoved && bSold && EdgesLeft == 0 ? 1 : 0;
		Trail += FString::Printf(TEXT(" %s %s %s, %d edge(s) left;"), *GetNameSafe(Divider),
			bBuilt ? *Edge.ToString() : TEXT("not built"), bMoved ? TEXT("moved and sold") : TEXT("NOT MOVED"), EdgesLeft);
		Audit();
	}

	Check(TEXT("CELULAS"), TEXT("building, moving and selling a tower, a platform, the palace and a divider gives every cell and edge back"),
		Done == 4 && Left == 0 && Dirty == 0,
		FString::Printf(TEXT("%d of 4 pieces through, %d cell(s)/edge(s) left taken, %d audit(s) dirty:%s"), Done, Left, Dirty, *Trail));
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
		// The night is a short stretch of the day's waves: the share asked, not the third
		// of the sky the clock gives it. The light curves are not touched.
		{
			const UBDDaySettings& Day = UBDDaySettings::Get();
			int32 NightSteps = 0;
			constexpr int32 Steps = 1000;
			for (int32 Sample = 0; Sample < Steps; ++Sample)
			{
				const float Hour = Day.HourForAlpha(Day.SkyAlphaForProgress(static_cast<float>(Sample) / Steps));
				NightSteps += UBDDayCycleComponent::PhaseForHour(Hour) == EBDDayPhase::Night ? 1 : 0;
			}
			const float NightFraction = static_cast<float>(NightSteps) / Steps;
			Check(TEXT("DIA"), TEXT("the waves spend NightShare of the day at night, the rest by daylight"),
				FMath::Abs(NightFraction - Day.NightShare) <= 0.01f,
				FString::Printf(TEXT("%.1f%% of the waves at night, %.1f%% asked; the clock gives the night %.1f%% of the sky"),
					NightFraction * 100.0f, Day.NightShare * 100.0f,
					(FMath::Frac((Day.SunriseHour - Day.NightHour) / 24.0f + 1.0f)) * 100.0f));
		}

		// A lamp post dropped on the map lights itself: dark by day, its editor brightness
		// at night, the copy needing nothing set. Spawned far off the board so it blocks no
		// cell, and the sky put back where it was.
		if (UBDDayCycleComponent* Cycle = Match->GetDayCycle())
		{
			const float SkyBefore = Cycle->GetCycleAlpha();

			// The lamps follow the dark, not the clock: off while the sun is up or just under
			// the horizon (day, sunrise, the afterglow), full the whole night through and
			// whenever the sun is well down. A clock curve once left the board dark and the
			// lamps off from six to half past eight, and a check by phase alone let it by.
			{
				const UBDDaySettings& DaySettings = UBDDaySettings::Get();
				constexpr int32 Samples = 480;
				int32 LitWrong = 0;
				int32 DarkWrong = 0;
				int32 NightSamples = 0;
				float WorstNight = 1.0f;
				float WorstNightHour = 0.0f;
				float WorstLit = 0.0f;
				float WorstLitHour = 0.0f;
				for (int32 Sample = 0; Sample <= Samples; ++Sample)
				{
					Cycle->SetAlphaImmediate(static_cast<float>(Sample) / Samples);
					const float Multiplier = Cycle->GetStreetLightMultiplier();
					const float Elevation = Cycle->GetSunElevation();
					const EBDDayPhase Phase = UBDDayCycleComponent::PhaseForHour(Cycle->GetHour());
					const bool bMustBeFull = Phase == EBDDayPhase::Night || Elevation <= DaySettings.StreetLightFullElevation;
					const bool bMustBeOff = Phase == EBDDayPhase::Day || Elevation >= DaySettings.StreetLightOffElevation;
					if (Phase == EBDDayPhase::Night) { ++NightSamples; }
					if (bMustBeFull && Multiplier < 0.999f)
					{
						++DarkWrong;
						if (Multiplier < WorstNight) { WorstNight = Multiplier; WorstNightHour = Cycle->GetHour(); }
					}
					if (bMustBeOff && Multiplier > 0.001f)
					{
						++LitWrong;
						if (Multiplier > WorstLit) { WorstLit = Multiplier; WorstLitHour = Cycle->GetHour(); }
					}
				}
				Cycle->SetAlphaImmediate(SkyBefore);
				Check(TEXT("DIA"), TEXT("the street lights are full the whole night and whenever the sun is well down, off while it is up"),
					DaySettings.bStreetLightsFollowSun && NightSamples > 0 && DarkWrong == 0 && LitWrong == 0,
					FString::Printf(TEXT("%d dark samples short of full (worst %.2f at %.2f h), %d bright samples lit (worst %.2f at %.2f h), %d night samples; off from %.0f deg, full from %.0f deg"),
						DarkWrong, WorstNight, WorstNightHour, LitWrong, WorstLit, WorstLitHour, NightSamples,
						DaySettings.StreetLightOffElevation, DaySettings.StreetLightFullElevation));
			}
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ABDStreetLamp* Lamp = World->SpawnActor<ABDStreetLamp>(FVector(0.0, 0.0, -100000.0), FRotator::ZeroRotator, Params);
			if (Lamp == nullptr)
			{
				Check(TEXT("DIA"), TEXT("a street lamp lights itself at night and goes dark by day, spots and top light alike"), false, TEXT("the lamp did not spawn"));
			}
			else
			{
				// From the class default: the lamp was scaled to the current sky the moment it spawned.
				const float Authored = GetDefault<ABDStreetLamp>()->GetSpotA()->Intensity;
				const float TopAuthored = GetDefault<ABDStreetLamp>()->GetTopLight()->Intensity;
				Cycle->SetAlphaImmediate(0.7f);
				const float NightWant = Authored * Cycle->GetStreetLightMultiplier();
				const float Night = Lamp->GetSpotA()->Intensity;
				const float NightMultiplier = Cycle->GetStreetLightMultiplier();
				const float TopNight = Lamp->GetTopLight()->Intensity;
				const bool bNightShown = Lamp->GetSpotA()->IsVisible() && Lamp->GetSpotB()->IsVisible() && Lamp->GetTopLight()->IsVisible();
				Cycle->SetAlphaImmediate(0.3f);
				const float DayCd = Lamp->GetSpotA()->Intensity;
				const float TopDay = Lamp->GetTopLight()->Intensity;
				const bool bDayHidden = !Lamp->GetSpotA()->IsVisible() && !Lamp->GetSpotB()->IsVisible() && !Lamp->GetTopLight()->IsVisible();
				const bool bNoShadows = !Lamp->GetSpotA()->CastShadows && !Lamp->GetSpotB()->CastShadows && !Lamp->GetTopLight()->CastShadows;
				const bool bTopFollows = FMath::IsNearlyEqual(TopNight, TopAuthored * NightMultiplier, 0.5f) && TopDay == 0.0f;
				// The lamp heads glow through the material: without the parameter they stay lit by day.
				const int32 Glowing = Lamp->GetStreetLight()->GetGlowingMeshCount();
				Cycle->SetAlphaImmediate(SkyBefore);
				Lamp->Destroy();

				Check(TEXT("DIA"), TEXT("a street lamp lights itself at night and goes dark by day, spots and top light alike"),
					NightWant > 0.0f && FMath::IsNearlyEqual(Night, NightWant, 0.01f) && bNightShown && DayCd == 0.0f && bDayHidden && bNoShadows && Glowing > 0 && bTopFollows,
					FString::Printf(TEXT("night %.2f cd of %.2f (%s), day %.2f cd (%s), shadows %s, %d glowing meshes, top light %.0f by night and %.0f by day"),
						Night, NightWant, bNightShown ? TEXT("shown") : TEXT("hidden"),
						DayCd, bDayHidden ? TEXT("hidden") : TEXT("shown"), bNoShadows ? TEXT("off") : TEXT("on"), Glowing, TopNight, TopDay));
			}
		}

		// The posts on the map itself: each one driven by the cycle, and no light left
		// outside a post, which would burn day and night whatever the sky says.
		if (const UBDDayCycleComponent* Cycle = Match->GetDayCycle())
		{
			int32 Posts = 0;
			int32 Driven = 0;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (const UBDStreetLightComponent* Street = It->FindComponentByClass<UBDStreetLightComponent>())
				{
					++Posts;
					Driven += Cycle->IsStreetLightRegistered(Street) ? 1 : 0;
				}
			}
			TArray<ULocalLightComponent*> Loose;
			UBDStreetLightComponent::FindLooseLights(World, Loose);
			FString LooseNames;
			for (const ULocalLightComponent* Light : Loose)
			{
				LooseNames += TEXT(" ") + Light->GetOwner()->GetActorNameOrLabel();
			}
			Check(TEXT("DIA"), TEXT("every light on the map is a street light the cycle drives"),
				Driven == Posts && Loose.Num() == 0,
				FString::Printf(TEXT("%d of %d posts driven, %d loose lights%s"), Driven, Posts, Loose.Num(), *LooseNames));
		}

		// The HUD's buttons never keep the keyboard: a focused button would swallow Escape.
		{
			int32 HudButtons = 0;
			int32 Focusable = 0;
			for (TObjectIterator<UButton> It; It; ++It)
			{
				if (It->GetWorld() == World && It->GetTypedOuter<UBDHUDWidget>() != nullptr)
				{
					++HudButtons;
					Focusable += It->GetIsFocusable() ? 1 : 0;
				}
			}
			Check(TEXT("INTERFACE"), TEXT("no button of the HUD takes the keyboard focus, so Escape reaches the game"),
				Focusable == 0, FString::Printf(TEXT("%d HUD button(s), %d focusable"), HudButtons, Focusable));
		}

		// An election opens with no votes, on every difficulty.
		Check(TEXT("VOTOS"), TEXT("the count opens at 0 to 0"), Match->GetVotesBlue() == 0 && Match->GetVotesRed() == 0,
			FString::Printf(TEXT("%d blue / %d red on %s"), Match->GetVotesBlue(), Match->GetVotesRed(), *UEnum::GetValueAsString(Match->Difficulty)));

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

		CheckCellsComeBack(Tower, Platform, Divider);
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

		// The shooters' button of the bar unfolds their list upwards; an entry takes its
		// shooter into the hand and folds the list again. The key hands over the same one.
		{
			UBDHUDWidget* Hud = nullptr;
			for (TObjectIterator<UBDHUDWidget> It; It; ++It)
			{
				if (It->GetWorld() == World) { Hud = *It; }
			}
			const int32 Listed = UBDPlacementSettings::Get().Shooters.Num();
			const bool bClicked = Hud != nullptr && Hud->DebugClickShooterGroup();
			const bool bOpened = bClicked && Hud->AreShootersOpen();
			const int32 Entries = Hud != nullptr ? Hud->GetShooterEntryCount() : 0;
			const bool bPicked = bOpened && Hud->DebugPickShooter(0);
			const UBDPlaceableData* Held = Placement->GetCurrentSelection();
			const bool bFolded = Hud != nullptr && !Hud->AreShootersOpen();
			const bool bKeySame = Placement->ResolvePaletteEntry(Character) == Held;
			Placement->CancelSelection();
			Check(TEXT("INTERFACE"), TEXT("the shooters' button unfolds their list; an entry takes its shooter and folds it, and the key gives the same"),
				Listed > 0 && bOpened && Entries == Listed && bPicked && Held != nullptr && UBDPlacementSettings::Get().IsListedShooter(Held) && bFolded && bKeySame,
				FString::Printf(TEXT("%s, %d of %d listed, picked %s, list %s, key %s"), Hud == nullptr ? TEXT("no HUD") : (bOpened ? TEXT("unfolded") : TEXT("STAYED SHUT")),
					Entries, Listed, *GetNameSafe(Held), bFolded ? TEXT("folded") : TEXT("STILL OPEN"), bKeySame ? TEXT("same") : TEXT("ANOTHER")));
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

		const FVector StarsEmpty = ABDMatchHUD::GetPlatformStarAnchor(*Stand->GetOwner());
		ABDTowerBase* First = Mount(0);
		// The stars mark the construction: a shooter boarding does not move them.
		{
			const FVector StarsManned = ABDMatchHUD::GetPlatformStarAnchor(*Stand->GetOwner());
			Check(TEXT("ESTRELAS"), TEXT("a platform's stars stay put when a shooter boards"),
				First != nullptr && StarsManned.Equals(StarsEmpty, 0.5f),
				FString::Printf(TEXT("row at %.1f cm empty, %.1f cm manned"), StarsEmpty.Z, StarsManned.Z));
		}
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

		// Manned but not evolved: every star empty, over the block and over a tower just built.
		const int32 StarsManned = ABDMatchHUD::GetPieceStarsFilled(Stand->GetBlockLevel());
		const int32 StarsNewTower = ABDMatchHUD::GetPieceStarsFilled(Crew[0]->GetTowerLevel());

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
		const int32 StarsEvolved = ABDMatchHUD::GetPieceStarsFilled(Stand->GetBlockLevel());
		const int32 StarsTop = ABDMatchHUD::GetPieceStarsFilled(UBDTowerData::MaxLevels);
		Check(TEXT("ESTRELAS"), TEXT("a star is an evolution bought: none just built or manned, one at level 2, all five at the top level 6"),
			StarsManned == 0 && StarsNewTower == 0 && StarsEvolved == 1 && UBDTowerData::MaxLevels == 6 && StarsTop == 5,
			FString::Printf(TEXT("manned block %d, new tower %d, block at level 2 %d, level %d %d of 5"),
				StarsManned, StarsNewTower, StarsEvolved, UBDTowerData::MaxLevels, StarsTop));

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

		// A shot kicks a character back and it settles again: firing reads apart from idling.
		if (Crew.Num() > 0 && Crew[0].IsValid())
		{
			ABDEnemyBase* Dummy = Waves->SpawnEnemy(UBDWaveSettings::Get().ResolveWaveEnemy(), 0);
			const bool bKicked = Dummy != nullptr && Crew[0]->DebugFireAt(Dummy) && Crew[0]->GetRecoilRemaining() > 0.0f;
			const float Peak = Crew[0]->GetRecoilDistance();
			if (Dummy != nullptr)
			{
				Dummy->Destroy();
			}
			Check(TEXT("PLATAFORMA"), TEXT("a character kicks back at every shot"), bKicked && Peak > 1.0f,
				FString::Printf(TEXT("kick %s, %.0f cm back at its peak"), bKicked ? TEXT("playing") : TEXT("none"), Peak));
		}

		// The platform's character is a shooter with a body's slots: the weapon of his level
		// in hand, a flash and a sound at every shot, an impact where it lands.
		if (Crew.Num() > 0 && Crew[0].IsValid())
		{
			ABDShooter* Shooter = Cast<ABDShooter>(Crew[0].Get());
			const UBDShooterData* ShooterData = Shooter != nullptr ? Shooter->GetShooterData() : nullptr;
			const FBDShooterWeapon* Held = ShooterData != nullptr ? ShooterData->GetWeapon(Shooter->GetTowerLevel()) : nullptr;
			Check(TEXT("ATIRADOR"), TEXT("the character piece builds a shooter, six weapons, the pistol designed and the star's weapon in hand"),
				Shooter != nullptr && ShooterData != nullptr && ShooterData->Weapons.Num() == UBDTowerData::MaxLevels
					&& !ShooterData->Weapons[0].bPlaceholder && Shooter->GetWeaponLevel() == Shooter->GetTowerLevel() && Shooter->GetTowerLevel() == 2,
				Shooter == nullptr ? FString::Printf(TEXT("%s is a %s"), *Crew[0]->GetName(), *Crew[0]->GetClass()->GetName())
					: FString::Printf(TEXT("level %d, weapon of level %d (%s), %d weapons, hand socket '%s'"), Shooter->GetTowerLevel(), Shooter->GetWeaponLevel(),
						Held != nullptr ? *Held->Name.ToString() : TEXT("none"), ShooterData != nullptr ? ShooterData->Weapons.Num() : 0,
						ShooterData != nullptr ? *ShooterData->HandSocket.ToString() : TEXT("")));

			// The pistol's cue is on level 1 and every weapon above holds it until it has its own,
			// for the platform shooter and for the Agent alike.
			{
				int32 ShooterSilent = 0;
				for (int32 Level = 1; ShooterData != nullptr && Level <= UBDTowerData::MaxLevels; ++Level)
				{
					ShooterSilent += ShooterData->ResolveFireSound(Level).IsNull() ? 1 : 0;
				}
				int32 AgentSilent = 0;
				const UBDPalaceData* PalaceData = LoadObject<UBDPalaceData>(nullptr, TEXT("/Game/BD/Data/DA_PalaceData.DA_PalaceData"));
				for (int32 Level = 0; PalaceData != nullptr && Level <= UBDPalaceData::MaxLevel; ++Level)
				{
					AgentSilent += PalaceData->ResolveFireSound(Level).IsNull() ? 1 : 0;
				}
				Check(TEXT("ATIRADOR"), TEXT("every level of the shooter's and the Agent's weapons fires with a sound"),
					ShooterData != nullptr && PalaceData != nullptr && ShooterSilent == 0 && AgentSilent == 0,
					FString::Printf(TEXT("shooter level 1 '%s', %d silent level(s); Agent level 0 '%s', %d silent level(s)"),
						ShooterData != nullptr ? *ShooterData->ResolveFireSound(1).GetAssetName() : TEXT("no data"), ShooterSilent,
						PalaceData != nullptr ? *PalaceData->ResolveFireSound(0).GetAssetName() : TEXT("no data"), AgentSilent));
			}

			// Who is talking: an exclamation in the colour of the side, one a speaker, gone the
			// moment its sound is not playing. Headless has no audio, so a sentence here never
			// plays: its mark must be dropped on the next look.
			if (UBDSpeechMarkSubsystem* Marks = UBDSpeechMarkSubsystem::Get(World))
			{
				ABDEnemyBase* Talker = Waves->SpawnEnemy(UBDWaveSettings::Get().ResolveWaveEnemy(), 0);
				const bool bSides = Talker != nullptr && Shooter != nullptr
					&& UBDSpeechMarkSubsystem::SideOf(*Talker) == EBDSpeakerSide::Opponent
					&& UBDSpeechMarkSubsystem::SideOf(*GetDefault<ABDCandidate>()) == EBDSpeakerSide::Opponent
					&& UBDSpeechMarkSubsystem::SideOf(*Shooter) == EBDSpeakerSide::Player
					&& UBDSpeechMarkSubsystem::SideOf(*GetDefault<ABDAgent>()) == EBDSpeakerSide::Player;
				const UBDUISettings& UI = UBDUISettings::Get();
				const FLinearColor Red = UI.GetSpeechMarkColor(EBDSpeakerSide::Opponent);
				const FLinearColor Blue = UI.GetSpeechMarkColor(EBDSpeakerSide::Player);
				const FLinearColor Black = UI.GetSpeechMarkColor(EBDSpeakerSide::Minister);
				const bool bColours = Red.R > Red.B && Red.R > Red.G && Blue.B > Blue.R && Blue.B > Blue.G && Black.GetLuminance() < 0.15f;

				int32 HeldAfterTwo = 0;
				int32 LiveAfter = -1;
				if (Talker != nullptr)
				{
					UAudioComponent* First = NewObject<UAudioComponent>(Talker);
					UAudioComponent* Second = NewObject<UAudioComponent>(Talker);
					Marks->NoteSpeech(*Talker, *First);
					Marks->NoteSpeech(*Talker, *Second);
					HeldAfterTwo = Marks->CountMarks(*Talker);
					Marks->GetLiveMarks();
					LiveAfter = Marks->CountMarks(*Talker);
					Talker->Destroy();
				}
				Check(TEXT("FALA"), TEXT("a speaker shows one exclamation of his side's colour, gone as soon as his sentence is not sounding"),
					bSides && bColours && HeldAfterTwo == 1 && LiveAfter == 0,
					FString::Printf(TEXT("sides %s, colours %s (red %s, blue %s, black %s), %d mark(s) after two sentences, %d once not sounding"),
						bSides ? TEXT("ok") : TEXT("WRONG"), bColours ? TEXT("ok") : TEXT("WRONG"), *Red.ToFColor(true).ToHex(), *Blue.ToFColor(true).ToHex(),
						*Black.ToFColor(true).ToHex(), HeldAfterTwo, LiveAfter));
			}

			// Fifty of them fill eight platforms: their bodies are in the animation budget with the horde's.
			const USkeletalMeshComponentBudgeted* BudgetedBody = Shooter != nullptr ? Cast<USkeletalMeshComponentBudgeted>(Shooter->GetBody()) : nullptr;
			Check(TEXT("ATIRADOR"), TEXT("a shooter's body is animated under the animation budget, like the horde's"),
				BudgetedBody != nullptr && BudgetedBody->GetAutoCalculateSignificance(),
				Shooter == nullptr ? TEXT("no shooter") : FString::Printf(TEXT("body is a %s"), *Shooter->GetBody()->GetClass()->GetName()));

			// The slot spheres and the grid stay in the editor: in the game they cost a frame's worth.
			{
				IConsoleVariable* GridDebug = IConsoleManager::Get().FindConsoleVariable(TEXT("BD.Grid.Debug"));
				const int32 Before = GridDebug != nullptr ? GridDebug->GetInt() : 0;
				const bool bOffByDefault = Before >= 2 || !BDGridDebug::ShouldDrawInWorld(*World);
				if (GridDebug != nullptr) { GridDebug->Set(2, ECVF_SetByCode); }
				const bool bForced = BDGridDebug::ShouldDrawInWorld(*World);
				if (GridDebug != nullptr) { GridDebug->Set(Before, ECVF_SetByCode); }
				Check(TEXT("DESEMPENHO"), TEXT("the grid and the platform slots are not drawn in the game unless BD.Grid.Debug 2 asks"),
					GridDebug != nullptr && bOffByDefault && bForced,
					FString::Printf(TEXT("BD.Grid.Debug %d, drawn by default %s, with 2 %s"), Before,
						BDGridDebug::ShouldDrawInWorld(*World) ? TEXT("yes") : TEXT("no"), bForced ? TEXT("yes") : TEXT("no")));
			}

			// A placeholder weapon fights with the level ladder; the pistol with its own numbers.
			const float PistolDamage = ShooterData != nullptr ? ShooterData->Weapons[0].Damage : 0.0f;
			const FBDTowerLevel* Base = ShooterData != nullptr ? ShooterData->GetLevel(1) : nullptr;
			const float LadderAtTwo = Base != nullptr ? Base->Damage * UBDGameBalanceSettings::Get().GetUpgradeDamageScale(2) : 0.0f;
			Check(TEXT("ATIRADOR"), TEXT("the pistol keeps the old shooter's numbers, a placeholder weapon follows the level ladder"),
				Shooter != nullptr && Base != nullptr && FMath::IsNearlyEqual(PistolDamage, Base->Damage) && FMath::IsNearlyEqual(Shooter->GetEffectiveDamage(), LadderAtTwo, 0.01f)
					&& FMath::IsNearlyEqual(Shooter->GetFireRate(), Base->FireRate),
				FString::Printf(TEXT("pistol %.1f dmg (level 1 %.1f), level 2 %.1f dmg (ladder %.1f), %.2f shots/s"),
					PistolDamage, Base != nullptr ? Base->Damage : 0.0f, Shooter != nullptr ? Shooter->GetEffectiveDamage() : 0.0f, LadderAtTwo,
					Shooter != nullptr ? Shooter->GetFireRate() : 0.0f));

			UBDShotSoundSubsystem* Shots = World->GetSubsystem<UBDShotSoundSubsystem>();
			ABDEnemyBase* Dummy = Waves->SpawnEnemy(UBDWaveSettings::Get().ResolveWaveEnemy(), 0);
			const int32 FlashesBefore = Shooter != nullptr ? Shooter->GetFlashesRequested() : 0;
			const int32 SoundsBefore = Shots != nullptr ? Shots->GetShotsRequested() : 0;
			const int32 ImpactsBefore = Shooter != nullptr ? Shooter->GetImpactsRequested() : 0;
			const bool bFired = Shooter != nullptr && Dummy != nullptr && Shooter->DebugFireAt(Dummy);
			if (Shooter != nullptr && Dummy != nullptr)
			{
				Shooter->ApplyHit(Dummy, Dummy->GetActorLocation(), 0.0f);
			}
			if (Dummy != nullptr)
			{
				Dummy->Destroy();
			}
			const int32 Flashes = Shooter != nullptr ? Shooter->GetFlashesRequested() - FlashesBefore : 0;
			const int32 Sounds = Shots != nullptr ? Shots->GetShotsRequested() - SoundsBefore : 0;
			const int32 Impacts = Shooter != nullptr ? Shooter->GetImpactsRequested() - ImpactsBefore : 0;
			Check(TEXT("ATIRADOR"), TEXT("a shot asks once for the muzzle flash and the shot sound, a hit for the impact"),
				bFired && Flashes == 1 && Sounds == 1 && Impacts == 1,
				FString::Printf(TEXT("shot %s, %d flash, %d sound, %d impact (slots %s, %s, %s)"), bFired ? TEXT("fired") : TEXT("NOT FIRED"), Flashes, Sounds, Impacts,
					ShooterData != nullptr && !ShooterData->ResolveMuzzleFlash(2).IsNull() ? TEXT("flash set") : TEXT("flash empty"),
					ShooterData != nullptr && !ShooterData->ResolveFireSound(2).IsNull() ? TEXT("sound set") : TEXT("sound empty"),
					ShooterData != nullptr && !ShooterData->ImpactEffect.IsNull() ? TEXT("impact set") : TEXT("impact empty")));
		}

		// The green stains: one per kill, gone on their own with a fade, never more than
		// MaxDecals whole on the ground. Painted with an engine texture here, so the pool is
		// tested before the art lands; without a texture nothing is left at all.
		if (UBDBloodDecalSubsystem* Blood = UBDBloodDecalSubsystem::Get(World))
		{
			const UBDBloodSettings& BloodSettings = UBDBloodSettings::Get();
			const bool bEmptyLeavesNone = !BloodSettings.BloodTexture.IsNull() || Blood->SpawnAt(Grid->CellToWorld(UrnCell)) == nullptr;
			Blood->SetTextureOverride(LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/DefaultTexture.DefaultTexture")));
			const int32 EvictedBefore = Blood->GetEvicted();
			const int32 Burst = BloodSettings.MaxDecals + 5;
			TArray<TWeakObjectPtr<UDecalComponent>> Left;
			for (int32 Each = 0; Each < Burst; ++Each)
			{
				Left.Add(Blood->SpawnAt(Grid->CellToWorld(UrnCell) + FVector(Each * 10.0f, 0.0f, 0.0f)));
			}
			Blood->SetTextureOverride(nullptr);
			const IConsoleVariable* FadeScaleVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Decal.FadeDurationScale"));
			const float FadeScale = FadeScaleVar != nullptr ? FadeScaleVar->GetFloat() : 1.0f;
			int32 Spawned = 0;
			int32 FadingOnTheirOwn = 0;
			for (const TWeakObjectPtr<UDecalComponent>& Decal : Left)
			{
				Spawned += Decal.IsValid() ? 1 : 0;
				FadingOnTheirOwn += Decal.IsValid() && FMath::IsNearlyEqual(Decal->GetFadeDuration(), BloodSettings.DecalFadeTime * FadeScale, 0.01f) ? 1 : 0;
			}
			const int32 Evicted = Blood->GetEvicted() - EvictedBefore;
			const bool bWorldSettings = World->GetWorldSettings() != nullptr && !World->GetWorldSettings()->IsActorBeingDestroyed();
			Check(TEXT("SANGUE"), TEXT("a stain per kill fades out on its own, the oldest gives way past MaxDecals, none without a texture"),
				bEmptyLeavesNone && Spawned == Burst && FadingOnTheirOwn == BloodSettings.MaxDecals && Evicted == Burst - BloodSettings.MaxDecals && bWorldSettings,
				FString::Printf(TEXT("%d of %d left, %d fading on their own over %.1f s after %.1f s, %d sent early, cap %d, %s"),
					Spawned, Burst, FadingOnTheirOwn, BloodSettings.DecalFadeTime, BloodSettings.HoldTime, Evicted, BloodSettings.MaxDecals,
					bEmptyLeavesNone ? TEXT("empty slot leaves none") : TEXT("EMPTY SLOT LEFT ONE")));
		}

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
		int32 DearestOther = 0;
		FString PriceList;
		bool bPricesFixed = true;
		for (const TSoftObjectPtr<UBDPlaceableData>& Entry : UBDPlacementSettings::Get().Palette)
		{
			const UBDPlaceableData* Other = Entry.LoadSynchronous();
			if (Other != nullptr && Other != PalacePiece)
			{
				const int32 Price = Match->GetBuildPrice(Other);
				DearestOther = FMath::Max(DearestOther, Price);
				// The golden rule: building is the base cost on the fixed scale, nothing of the wave in it.
				const int32 Fixed = Other->GetPieceKind() == EBDPieceKind::Divider ? 0 : Balance.GetPieceCost(Other->GetBuildCost());
				bPricesFixed &= Price == Fixed;
				PriceList += FString::Printf(TEXT(" %s %d"), *Other->GetName(), Price);
			}
		}
		Check(TEXT("PALACIO"), TEXT("the palace costs PalaceCost on any wave, more than any other piece"),
			PalacePrice == Balance.PalaceCost && PalacePrice > DearestOther,
			FString::Printf(TEXT("price %d, PalaceCost %d, dearest other %d"), PalacePrice, Balance.PalaceCost, DearestOther));
		Check(TEXT("ECONOMIA"), TEXT("every piece is priced off its base cost alone, never the wave"), bPricesFixed,
			FString::Printf(TEXT("wave %d:%s"), Match->GetCurrentWave(), *PriceList));

		// Evolution climbs by level only: 1x, 3x, 9x, 27x of the base with the growth at 3.
		const int32 LadderBase = 40;
		const int32 L2 = Balance.GetUpgradeCost(LadderBase, 2), L3 = Balance.GetUpgradeCost(LadderBase, 3);
		const int32 L4 = Balance.GetUpgradeCost(LadderBase, 4), L5 = Balance.GetUpgradeCost(LadderBase, 5);
		const float Growth = Balance.UpgradeCostGrowth;
		Check(TEXT("EVOLUCAO"), TEXT("each level costs UpgradeCostGrowth times the last, level 2 the base"),
			L2 == LadderBase && L3 == FMath::RoundToInt(LadderBase * Growth) && L4 == FMath::RoundToInt(LadderBase * Growth * Growth)
				&& L5 == FMath::RoundToInt(LadderBase * Growth * Growth * Growth),
			FString::Printf(TEXT("base %d, growth %.1f: %d, %d, %d, %d"), LadderBase, Growth, L2, L3, L4, L5));

		// What a player who spends nothing has by the third candidate, against the price then.
		const int32 Interval = FMath::Max(1, Balance.CandidateInterval);
		const int32 Start = Match->GetDifficultyData() != nullptr ? Match->GetDifficultyData()->StartingFunds : 0;
		const int32 ThirdWave = Interval * 3;
		const int32 ByThird = Start + Match->GetCandidateFunds(Interval) + Match->GetCandidateFunds(Interval * 2) + Match->GetCandidateFunds(ThirdWave);
		Check(TEXT("PALACIO"), TEXT("the opening funds do not buy the palace: it waits for a few candidates"), PalacePrice > Start,
			FString::Printf(TEXT("price %d, opening funds %d"), PalacePrice, Start));
		UE_LOG(LogBDDebug, Log, TEXT("REGRESSION palace calibration: price %d on every wave; saved by the third candidate %d (start %d + bribes of waves %d, %d, %d)."),
			PalacePrice, ByThird, Start, Interval, Interval * 2, ThirdWave);

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

		// One palace a match: a second is refused in the hand and on the board, and the
		// reason names the palace even with the money short (it is asked before the money).
		{
			const EBDPlacementRefusal InHand = Placement->GetHandRefusal(PalacePiece);
			const bool bTaken = Placement->TakeIntoHand(PalacePiece);
			Placement->SelectPlaceable(PalacePiece);
			Placement->SetHoveredCellDirect(FBDCellCoord(PalaceCell.X + 4, PalaceCell.Y));
			const EBDPlacementRefusal OnBoard = Placement->GetCurrentRefusal();
			Placement->CancelSelection();
			int32 Palaces = 0;
			for (TActorIterator<ABDPalace> It(World); It; ++It) { ++Palaces; }
			Check(TEXT("PALACIO"), TEXT("only one palace is accepted: a second is refused with its reason"),
				InHand == EBDPlacementRefusal::PalaceAlreadyBuilt && !bTaken && OnBoard == EBDPlacementRefusal::PalaceAlreadyBuilt && Palaces == 1,
				FString::Printf(TEXT("hand %s, board %s, %d palace(s)"), *RefusalName(InHand), *RefusalName(OnBoard), Palaces));
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

		// On the floor, not on the board plane under it: the street stands above the plane.
		{
			float FeetOver = 0.0f, RootOver = 0.0f;
			const bool bMeasured = Agent != nullptr && Agent->MeasureFeet(FeetOver, RootOver);
			Check(TEXT("AGENTE"), TEXT("the Agent stands on the traced floor, not sunk to the board plane"),
				bMeasured && FMath::Abs(RootOver) <= 2.0f,
				FString::Printf(TEXT("root %+.1f cm over the floor, lowest bone %+.1f cm%s"), RootOver, FeetOver, bMeasured ? TEXT("") : TEXT(" (not measured)")));
		}

		// Off the grid: walking, he leaves the cells' centre lines at an angle, not along them.
		if (Agent != nullptr)
		{
			// A fixed draw, so the check never rests on a walk that happened to run along an axis.
			FMath::RandInit(20261005);
			const float Cell = Grid->GetCellSize();
			const FVector WalkStart = Agent->GetActorLocation();
			float WorstOffAxis = 0.0f;
			int32 WalkingTicks = 0;
			for (int32 Tick = 0; Tick < 120; ++Tick)
			{
				Agent->Tick(1.0f / 30.0f);
				FBDCellCoord Under;
				if (Agent->GetState() != EBDAgentState::Walking || !Grid->WorldToCell(Agent->GetActorLocation(), Under))
				{
					continue;
				}
				++WalkingTicks;
				const FVector Center = Grid->CellToWorld(Under);
				const float OffX = FMath::Abs(Agent->GetActorLocation().X - Center.X);
				const float OffY = FMath::Abs(Agent->GetActorLocation().Y - Center.Y);
				WorstOffAxis = FMath::Max(WorstOffAxis, FMath::Min(OffX, OffY));
			}
			const float Moved = FVector::Dist2D(WalkStart, Agent->GetActorLocation());
			// The board is drawn anew every run, and a walk can run within a degree or two of an
			// axis and keep near a centre line. A grid-bound walk would also face the axis
			// exactly: off the lines, or off the axis, is free of the grid.
			const float Yaw = Agent->GetActorRotation().Yaw;
			const float OffAxisDegrees = FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, FMath::RoundToFloat(Yaw / 90.0f) * 90.0f));
			Check(TEXT("AGENTE"), TEXT("the Agent walks free of the grid: off both centre lines of a cell, at any angle"),
				WalkingTicks > 0 && (WorstOffAxis > Cell * 0.05f || OffAxisDegrees > 0.5f),
				FString::Printf(TEXT("%d walking tick(s), %.0f cm off the nearer centre line at most (cell %.0f), moved %.0f cm, yaw %.1f (%.1f deg off the axis)"),
					WalkingTicks, WorstOffAxis, Cell, Moved, Yaw, OffAxisDegrees));
		}

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

		// The rest: shorter the more he killed, never under the floor; the shot leaves on the gesture.
		if (PalaceData != nullptr)
		{
			const float RestNone = PalaceData->GetRestTime(0);
			const float RestTwenty = PalaceData->GetRestTime(20);
			const float RestMany = PalaceData->GetRestTime(1000);
			Check(TEXT("AGENTE"), TEXT("the rest shrinks with the kills of the patrol, down to its floor"),
				FMath::IsNearlyEqual(RestNone, PalaceData->RestTimeBase) && RestTwenty < RestNone
					&& FMath::IsNearlyEqual(RestTwenty, PalaceData->RestTimeBase - 20 * PalaceData->RestReductionPerKill)
					&& FMath::IsNearlyEqual(RestMany, PalaceData->RestTimeMin),
				FString::Printf(TEXT("0 kills %.1fs, 20 kills %.1fs, 1000 kills %.1fs (floor %.1fs)"), RestNone, RestTwenty, RestMany, PalaceData->RestTimeMin));
			const UAnimSequenceBase* Shoot = PalaceData->ShootAnimation.LoadSynchronous();
			Check(TEXT("AGENTE"), TEXT("the shooting animation carries the shot notify"), UBDAnimNotify_Shot::IsOn(Shoot),
				FString::Printf(TEXT("%s"), *GetNameSafe(Shoot)));
		}

		// The reach shows for the piece selected only: the palace shows its Agent's.
		if (UBDInspectionSubsystem* Inspection = UBDInspectionSubsystem::Get(World))
		{
			ABDTowerBase* AnyTower = nullptr;
			for (TActorIterator<ABDTowerBase> It(World); It && AnyTower == nullptr; ++It) { AnyTower = *It; }
			Placement->SelectDefender(AnyTower);
			const bool bTowerShown = AnyTower != nullptr && UBDInspectionSubsystem::ShouldDrawReach(1, AnyTower)
				&& (Agent == nullptr || !UBDInspectionSubsystem::ShouldDrawReach(1, Agent, Palace));
			Placement->InspectPiece(Palace);
			const bool bAgentShown = Agent != nullptr && UBDInspectionSubsystem::ShouldDrawReach(1, Agent, Palace)
				&& (AnyTower == nullptr || !UBDInspectionSubsystem::ShouldDrawReach(1, AnyTower));
			Placement->SelectDefender(nullptr);
			const bool bCleared = Inspection->GetInspected() == nullptr && (AnyTower == nullptr || !UBDInspectionSubsystem::ShouldDrawReach(1, AnyTower));
			Check(TEXT("ALCANCE"), TEXT("a reach shows only for the piece selected, and clears with the selection"),
				bTowerShown && bAgentShown && bCleared,
				FString::Printf(TEXT("tower %s, palace shows the agent %s, cleared %s"), bTowerShown ? TEXT("yes") : TEXT("no"),
					bAgentShown ? TEXT("yes") : TEXT("no"), bCleared ? TEXT("yes") : TEXT("no")));
		}

		// One vote per body: a militant killed is one blue, one at the urn one red.
		if (UBDGameBalanceSettings::Get().bVotesByBody)
		{
			const UBDEnemyData* Militant = UBDWaveSettings::Get().ResolveWaveEnemy();
			const int32 BlueBeforeBody = Match->GetVotesBlue();
			const int32 RedBeforeBody = Match->GetVotesRed();
			ABDEnemyBase* Killed = Militant != nullptr ? Waves->SpawnEnemy(Militant, 0) : nullptr;
			if (Killed != nullptr)
			{
				Killed->Kill();
			}
			ABDEnemyBase* Leaked = Militant != nullptr ? Waves->SpawnEnemy(Militant, 0) : nullptr;
			if (Leaked != nullptr)
			{
				Waves->NotifyEnemyArrived(Leaked);
				Leaked->Destroy();
			}
			Check(TEXT("VOTOS"), TEXT("one vote per body: +1 blue for a militant killed, +1 red for one at the urn"),
				Killed != nullptr && Leaked != nullptr && Match->GetVotesBlue() == BlueBeforeBody + 1 && Match->GetVotesRed() == RedBeforeBody + 1,
				FString::Printf(TEXT("blue %d -> %d, red %d -> %d, creep of %.0f hp"), BlueBeforeBody, Match->GetVotesBlue(), RedBeforeBody, Match->GetVotesRed(),
					Killed != nullptr ? Killed->GetMaxHealth() : 0.0f));
		}

		// With a piece in hand, the right button and Escape put it back, and sell nothing:
		// the hand never keeps the click from reaching what is on the board.
		{
			const int32 MoneyBeforeCancel = Match->GetPublicMoney();
			Placement->SelectPlaceable(Tower);
			Placement->SetHoveredCellDirect(PalaceCell);
			Placement->DebugRightClick();
			const bool bRightCancelled = Placement->GetCurrentSelection() == nullptr;
			const bool bPalaceStands = Palace != nullptr && IsValid(Palace);
			Placement->SelectPlaceable(Tower);
			Placement->DebugEscape();
			const bool bEscCancelled = Placement->GetCurrentSelection() == nullptr;
			Check(TEXT("COLOCACAO"), TEXT("the right button and Escape put the piece in hand back, selling nothing"),
				bRightCancelled && bEscCancelled && bPalaceStands && Match->GetPublicMoney() == MoneyBeforeCancel,
				FString::Printf(TEXT("right button %s, Escape %s, palace %s, money %d -> %d"), bRightCancelled ? TEXT("emptied the hand") : TEXT("KEPT IT"),
					bEscCancelled ? TEXT("emptied the hand") : TEXT("KEPT IT"), bPalaceStands ? TEXT("standing") : TEXT("SOLD"), MoneyBeforeCancel, Match->GetPublicMoney()));
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
		const EBDPlacementRefusal AfterSale = Placement->GetHandRefusal(PalacePiece);
		Check(TEXT("PALACIO"), TEXT("once sold, a palace may be built again"), AfterSale != EBDPlacementRefusal::PalaceAlreadyBuilt,
			RefusalName(AfterSale));
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
			EnginesBeforeDeal = BusesAtDeal != nullptr ? BusesAtDeal->GetEnginesAsked() : 0;
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

			// The wave's mouths start their engines as it is dealt, one each and only those.
			const int32 Engines = Buses->GetEnginesAsked() - EnginesBeforeDeal;
			Check(TEXT("ONIBUS"), TEXT("every mouth the wave opened starts its bus's engine, and no other"),
				Engines == Waves->GetActiveSpawnPoints().Num(),
				FString::Printf(TEXT("%d engine(s) for %d open mouth(s); %d played (slot set: %s)"), Engines, Waves->GetActiveSpawnPoints().Num(),
					Buses->GetEnginesPlayed(), UBDAudioSettings::Get().BusEngineSound.IsNull() ? TEXT("no") : TEXT("yes")));
		}

		// Under a wave the player's resources are still theirs (briefing 2026-10-02 17:15):
		// anything they can pay for goes down, through the bar and through the gesture.
		// Only the urn stays where the first wave found it.
		FString Offered;
		bool bUrnOffered = false;
		bool bTowerOffered = false;
		for (const TSoftObjectPtr<UBDPlaceableData>& Entry : UBDPlacementSettings::Get().Palette)
		{
			const UBDPlaceableData* Piece = Entry.LoadSynchronous();
			if (Piece != nullptr && Placement->GetHandRefusal(Piece) == EBDPlacementRefusal::None)
			{
				Offered += Offered.IsEmpty() ? Piece->GetName() : TEXT(", ") + Piece->GetName();
				bUrnOffered |= Piece->GetPieceKind() == EBDPieceKind::Objective;
				bTowerOffered |= Piece == Tower;
			}
		}
		const UBDPlaceableData* UrnPiece = UBDObjectiveSettings::Get().ObjectivePlaceable.LoadSynchronous();
		bUrnOffered |= UrnPiece != nullptr && Placement->GetHandRefusal(UrnPiece) == EBDPlacementRefusal::None;
		const int32 BuiltBefore = Match->GetLedger().PiecesBuilt;
		const int32 MoneyBefore = Match->GetPublicMoney();
		const int32 TowerPrice = Match->GetBuildPrice(Tower);
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
		Check(TEXT("COLOCACAO"), TEXT("a piece the player can pay for is built while a wave is out; the urn stays locked"),
			bTowerOffered && !bUrnOffered && bSlipped && Match->GetLedger().PiecesBuilt == BuiltBefore + 1 && Match->GetPublicMoney() == MoneyBefore - TowerPrice,
			FString::Printf(TEXT("offered by the bar: %s; urn %s; a tower through the gesture: %s at %s (%s), money %d -> %d"), Offered.IsEmpty() ? TEXT("none") : *Offered,
				bUrnOffered ? TEXT("OFFERED") : TEXT("locked"), bSlipped ? TEXT("placed") : TEXT("REFUSED"), *Probe.ToString(), *RefusalName(Gesture),
				MoneyBefore, Match->GetPublicMoney()));

		// What goes down under a wave must not fence a walking creep in: a platform on the
		// cell a creep is heading into is refused, as blocking the path.
		{
			const ABDEnemyBase* Walker = nullptr;
			for (const ABDEnemyBase* Enemy : Waves->GetLivingEnemiesRef())
			{
				if (Enemy != nullptr && !Enemy->IsOnFinalLeg() && !Enemy->HasArrived() && Grid->GetCellState(Enemy->GetHeadingCell()) == EBDCellState::Free)
				{
					Walker = Enemy;
					break;
				}
			}
			if (Walker != nullptr && Platform != nullptr)
			{
				const int32 BuiltBeforeTrap = Match->GetLedger().PiecesBuilt;
				Placement->SelectPlaceable(Platform);
				Placement->SetHoveredCellDirect(Walker->GetHeadingCell());
				const bool bTrapped = Placement->TryPlaceAtHovered();
				const EBDPlacementRefusal TrapRefusal = Placement->GetCurrentRefusal();
				Placement->CancelSelection();
				Check(TEXT("COLOCACAO"), TEXT("a piece that would fence a walking creep in is refused"),
					!bTrapped && Match->GetLedger().PiecesBuilt == BuiltBeforeTrap,
					FString::Printf(TEXT("%s heading into %s: %s (%s)"), *Walker->GetName(), *Walker->GetHeadingCell().ToString(),
						bTrapped ? TEXT("PLACED") : TEXT("refused"), *RefusalName(TrapRefusal)));
			}
		}
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
		// A wave on: a battle track plays, and the ambience goes down under it.
		if (const UBDSoundscapeSubsystem* Soundscape = UBDSoundscapeSubsystem::Get(World))
		{
			const bool bMusicSlots = UBDAudioSettings::Get().BattleMusic.Num() > 0;
			Check(TEXT("AUDIO"), TEXT("a battle track plays while a wave is on"),
				!bMusicSlots || Soundscape->GetMusic().IsPlaying(), Soundscape->Describe());
		}

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
			UBDBloodDecalSubsystem* Blood = UBDBloodDecalSubsystem::Get(GetWorld());
			const int32 StainsBefore = Blood != nullptr ? Blood->GetRequested() : 0;
			Creep->Kill();
			Check(TEXT("SANGUE"), TEXT("a kill asks for a stain where the creep fell"), Blood != nullptr && Blood->GetRequested() == StainsBefore + 1,
				FString::Printf(TEXT("%d stain(s) asked"), Blood != nullptr ? Blood->GetRequested() - StainsBefore : 0));

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

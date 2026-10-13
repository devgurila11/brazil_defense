// Brazil Defense. The thermometer: every mechanic that already works, checked in one go.

#include "Debug/BDRegressionSubsystem.h"

#include "BDBuildInfo.h"
#include "HAL/FileManager.h"
#include "Save/BDMatchSave.h"
#include "Save/BDProgressSave.h"
#include "Cast/BDCast.h"
#include "BDLog.h"
#include "UI/BDLocalization.h"
#include "Bribe/BDBribeSubsystem.h"
#include "Candidate/BDCandidateSubsystem.h"
#include "Candidate/BDCandidateData.h"
#include "Candidate/BDRallyComponent.h"
#include "Enemy/BDCandidate.h"
#include "Enemy/BDCreepSoundSettings.h"
#include "Enemy/BDCreepSoundSubsystem.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Enemy/BDEnemyData.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Enemy/BDAnimNotify_BodyFall.h"
#include "Enemy/BDAnimNotify_Footstep.h"
#include "Enemy/BDCreepCorpse.h"
#include "EngineUtils.h"
#include "UI/BDSettingsSave.h"
#include "UI/BDUISettings.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Animation/AnimSequenceBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Grid/BDFreeWalk.h"
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
#include "Palace/BDMinisterSubsystem.h"
#include "UI/BDSettingsSubsystem.h"
#include "UI/BDSettingsSave.h"
#include "GameFramework/GameUserSettings.h"
#include "Palace/BDMinisterSettings.h"
#include "Palace/BDMinister.h"
#include "Palace/BDMinisterData.h"
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
#include "Tower/BDShooter.h"
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

	// Saves go to the test slot, and the player's is stamped to prove it stayed untouched.
	bWasTestSlot = UBDMatchSave::IsTestSlot();
	UBDMatchSave::SetTestSlot(true);
	UBDMatchSave::Delete();
	UBDProgressSave::ResetProgress();
	PlayerFiles.Reset();
	PlayerFileExisted.Reset();
	PlayerFileTime.Reset();
	PlayerFileSize.Reset();
	for (const TCHAR* Slot : { UBDMatchSave::PlayerSlotName, UBDProgressSave::PlayerSlotName })
	{
		const FString File = FPaths::ProjectSavedDir() / TEXT("SaveGames") / FString(Slot) + TEXT(".sav");
		const bool bExists = IFileManager::Get().FileExists(*File);
		PlayerFiles.Add(File);
		PlayerFileExisted.Add(bExists);
		PlayerFileTime.Add(bExists ? IFileManager::Get().GetTimeStamp(*File) : FDateTime());
		PlayerFileSize.Add(bExists ? IFileManager::Get().FileSize(*File) : -1);
	}

	WatchedMatch = Match;
	LastBlue = Match->GetVotesBlue();
	LevelDownsSeen = Match->GetLevelDownCount();
	UnexplainedDrops = 0;
	VotesHandle = Match->OnVotesChanged.AddUObject(this, &UBDRegressionSubsystem::HandleVotesChanged);

	UE_LOG(LogBDDebug, Log, TEXT("REGRESSION start on %s, seed %d."), *BDBuildInfo::GetLabel(), Match->ObstacleSeed);
}

bool UBDRegressionSubsystem::IsPlayerSlotUntouched(FString& Measured) const
{
	bool bAllSame = true;
	Measured = FString::Printf(TEXT("writing '%s' and '%s'"), UBDMatchSave::GetSlotName(), UBDProgressSave::GetSlotName());
	for (int32 Index = 0; Index < PlayerFiles.Num(); ++Index)
	{
		const FString& File = PlayerFiles[Index];
		const bool bExists = IFileManager::Get().FileExists(*File);
		const bool bSame = bExists == PlayerFileExisted[Index]
			&& (!bExists || (IFileManager::Get().GetTimeStamp(*File) == PlayerFileTime[Index] && IFileManager::Get().FileSize(*File) == PlayerFileSize[Index]));
		bAllSame &= bSame;
		Measured += FString::Printf(TEXT("; the player's %s %s"), *FPaths::GetCleanFilename(File),
			!PlayerFileExisted[Index] && !bExists ? TEXT("absent before and after") : bSame ? TEXT("unchanged") : TEXT("CHANGED"));
	}
	return bAllSame;
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
	UBDMatchSave::Delete();
	UBDProgressSave::ResetProgress();
	UBDMatchSave::SetTestSlot(bWasTestSlot);

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

void UBDRegressionSubsystem::CheckCast()
{
	const BDCast::FAudit Audit = BDCast::RunAudit();
	const UBDCastSettings& Settings = UBDCastSettings::Get();
	constexpr int32 Kinds = static_cast<int32>(EBDCastKind::Count);

	// As many of each kind as the commandlet was last asked for: none lost, none stray.
	bool bCounts = true;
	int32 Expected = 0;
	FString Counts;
	for (int32 Kind = 0; Kind < Kinds; ++Kind)
	{
		const int32 Wanted = Settings.GetCount(static_cast<EBDCastKind>(Kind));
		bCounts &= Wanted > 0 && Audit.Found[Kind] == Wanted;
		Expected += Wanted;
		Counts += FString::Printf(TEXT("%s%s %d/%d"), Counts.IsEmpty() ? TEXT("") : TEXT(", "), BDCast::GetFolder(static_cast<EBDCastKind>(Kind)), Audit.Found[Kind], Wanted);
	}
	Check(TEXT("ELENCO"), TEXT("the cast holds as many data assets of each kind as BDScaffoldCast was asked for"), bCounts, Counts);

	// The audit finds every one where the convention puts it, with its slots listed.
	bool bInPlace = Audit.Missing.Num() == 0 && Audit.TotalFound() == Expected;
	for (const BDCast::FAuditEntry& Entry : Audit.Entries)
	{
		bInPlace &= Entry.PackageName == BDCast::GetPackageName(Entry.Kind, Entry.Number) && Entry.Slots.Num() > 0;
	}
	Check(TEXT("ELENCO"), TEXT("BD.Cast.Audit finds every one in its own folder and lists its slots"), bInPlace,
		FString::Printf(TEXT("%d DAs, %d empty slots, %d missing"), Audit.TotalFound(), Audit.TotalEmpty(), Audit.Missing.Num()));

	// The three characters of the game came over whole, and the game still plays them.
	const auto Unfilled = [&Audit](const EBDCastKind Kind, const TArray<FString>& Slots, FString& Out)
	{
		const BDCast::FAuditEntry* Entry = Audit.Find(Kind, 1);
		if (Entry == nullptr)
		{
			Out += FString::Printf(TEXT(" %s_01 gone;"), BDCast::GetStem(Kind));
			return;
		}
		for (const BDCast::FSlot& Slot : Entry->Slots)
		{
			if (!Slot.bFilled && (Slots.Num() == 0 || Slots.Contains(Slot.Name)))
			{
				Out += FString::Printf(TEXT(" %s_01 %s;"), BDCast::GetStem(Kind), *Slot.Name);
			}
		}
	};
	FString Lost;
	Unfilled(EBDCastKind::Horde, {}, Lost);
	Unfilled(EBDCastKind::Shooter, { TEXT("SkeletalMesh"), TEXT("Anim.Idle"), TEXT("Anim.Shooting"), TEXT("Weapon_1"), TEXT("Sound.Celebration") }, Lost);
	Unfilled(EBDCastKind::Candidate, { TEXT("MeshMaterial") }, Lost);
	const UBDWaveSettings& WaveSettings = UBDWaveSettings::Get();
	const UBDPlaceableData* CharacterPiece = LoadObject<UBDPlaceableData>(nullptr, TEXT("/Game/BD/Data/DA_Character.DA_Character"));
	const FString Played = FString::Printf(TEXT("wave %s, candidate %s, character piece %s"), *WaveSettings.WaveEnemy.GetAssetName(),
		*WaveSettings.CandidateEnemy.GetAssetName(), CharacterPiece != nullptr ? *CharacterPiece->TowerData.GetAssetName() : TEXT("-"));
	const bool bPlayed = WaveSettings.WaveEnemy.ToSoftObjectPath().GetLongPackageName() == BDCast::GetPackageName(EBDCastKind::Horde, 1)
		&& WaveSettings.CandidateEnemy.ToSoftObjectPath().GetLongPackageName() == BDCast::GetPackageName(EBDCastKind::Candidate, 1)
		&& CharacterPiece != nullptr && CharacterPiece->TowerData.ToSoftObjectPath().GetLongPackageName() == BDCast::GetPackageName(EBDCastKind::Shooter, 1);
	Check(TEXT("ELENCO"), TEXT("the donkey, Nicole and the candidate became the first of their kind with today's slots filled, and the game plays them"),
		Lost.IsEmpty() && bPlayed, FString::Printf(TEXT("%s; %s"), Lost.IsEmpty() ? TEXT("nothing lost") : *Lost.TrimEnd(), *Played));

	// The candidate dies with a cry and the minister flees with one, each a bank apart from the words.
	const auto Distinct = [](const UClass* Class, const TCHAR* Cry)
	{
		const FProperty* Words = FindFProperty<FProperty>(Class, TEXT("SpeechSound"));
		const FProperty* Other = FindFProperty<FProperty>(Class, Cry);
		return Words != nullptr && Other != nullptr && Words != Other && Words->GetOffset_ForInternal() != Other->GetOffset_ForInternal();
	};
	const bool bCandidate = Distinct(UBDEnemyData::StaticClass(), TEXT("DeathSound"));
	const bool bMinister = Distinct(UBDMinisterData::StaticClass(), TEXT("FleeSound"))
		&& FindFProperty<FProperty>(UBDMinisterData::StaticClass(), TEXT("DeathSound")) == nullptr;
	Check(TEXT("ELENCO"), TEXT("the candidate has a DeathSound and the minister a FleeSound (no death), each apart from SpeechSound"), bCandidate && bMinister,
		FString::Printf(TEXT("candidate %s, minister %s"), bCandidate ? TEXT("apart") : TEXT("SHARED OR MISSING"), bMinister ? TEXT("apart") : TEXT("SHARED, MISSING OR WITH A DEATH")));
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

void UBDRegressionSubsystem::TrackRally(const UBDRallyComponent& Rally)
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Rally.GetState() == TrackedState)
	{
		return;
	}
	const float Lasted = static_cast<float>(Now - TrackedSince);
	switch (TrackedState)
	{
	case EBDRallyState::Tell: MeasuredTell = Lasted; break;
	case EBDRallyState::Speaking: MeasuredSpeech = Lasted; break;
	case EBDRallyState::Spawning: MeasuredRise = Lasted; break;
	default: break;
	}
	TrackedState = Rally.GetState();
	TrackedSince = Now;
}

bool UBDRegressionSubsystem::IsCrowdSound(const TArray<TWeakObjectPtr<ABDEnemyBase>>& Crowd, FString& Measured) const
{
	const UBDGridSubsystem* Grid = UBDGridSubsystem::Get(GetWorld());
	TSet<FBDCellCoord> Cells;
	int32 Alive = 0, Overlaps = 0, OffGround = 0, Unrouted = 0;
	for (const TWeakObjectPtr<ABDEnemyBase>& Each : Crowd)
	{
		const ABDEnemyBase* Militant = Each.Get();
		if (Militant == nullptr || Grid == nullptr || Militant->GetPath().Num() == 0)
		{
			++Unrouted;
			continue;
		}
		++Alive;
		// Where it rose is where its route starts.
		const FBDCellCoord Cell = Militant->GetPath()[0];
		bool bAlreadyThere = false;
		Cells.Add(Cell, &bAlreadyThere);
		Overlaps += bAlreadyThere ? 1 : 0;
		const EBDCellState State = Grid->GetCellState(Cell);
		OffGround += State == EBDCellState::Free || State == EBDCellState::Tower || State == EBDCellState::Spawn ? 0 : 1;
		Unrouted += Grid->GetCellState(Militant->GetPath().Last()) == EBDCellState::Goal ? 0 : 1;
	}
	Measured = FString::Printf(TEXT("%d alive on %d cell(s), %d overlap(s), %d off free ground, %d without a way to the urn"),
		Alive, Cells.Num(), Overlaps, OffGround, Unrouted);
	return Alive == Crowd.Num() && Overlaps == 0 && OffGround == 0 && Unrouted == 0;
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
		CheckCast();

		// No urn down yet: the first countdown holds full whatever time goes by, and the wave
		// cannot be called onto a board with nowhere to walk to.
		{
			const float Full = Match->GetDifficultyData() != nullptr ? Match->GetDifficultyData()->FirstWaveDelay : -1.0f;
			BDRegressionPrivate::SetCVarInt(TEXT("BD.Match.FreezeTimer"), 0);
			for (int32 Second = 0; Second < 200; ++Second)
			{
				Match->Tick(1.0f);
			}
			const bool bHeld = Match->GetCurrentWave() == 0 && Match->GetPhase() == EBDMatchPhase::Building && Match->IsAwaitingUrn()
				&& FMath::IsNearlyEqual(Match->GetTimeUntilNextWave(), Full);
			Match->CallWaveEarly();
			const bool bRefused = Match->GetCurrentWave() == 0 && Match->GetPhase() == EBDMatchPhase::Building;
			BDRegressionPrivate::SetCVarInt(TEXT("BD.Match.FreezeTimer"), 1);
			Check(TEXT("URNA"), TEXT("with no urn down 200 s go by without a wave: the countdown holds full and calling the wave is refused"),
				bHeld && bRefused, FString::Printf(TEXT("wave %d, %.1fs on the clock of %.1f, awaiting urn %s, call %s"), Match->GetCurrentWave(),
					Match->GetTimeUntilNextWave(), Full, Match->IsAwaitingUrn() ? TEXT("yes") : TEXT("no"), bRefused ? TEXT("refused") : TEXT("SENT THE WAVE")));
		}

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

		// The urn down starts the first countdown from the difficulty's whole delay.
		{
			const float Full = Match->GetDifficultyData() != nullptr ? Match->GetDifficultyData()->FirstWaveDelay : -1.0f;
			const float AtPlace = Match->GetTimeUntilNextWave();
			BDRegressionPrivate::SetCVarInt(TEXT("BD.Match.FreezeTimer"), 0);
			Match->Tick(1.0f);
			const float AfterSecond = Match->GetTimeUntilNextWave();
			BDRegressionPrivate::SetCVarInt(TEXT("BD.Match.FreezeTimer"), 1);
			Check(TEXT("URNA"), TEXT("the urn down starts the countdown from FirstWaveDelay"),
				!Match->IsAwaitingUrn() && FMath::IsNearlyEqual(AtPlace, Full) && FMath::IsNearlyEqual(AfterSecond, Full - 1.0f, 0.01f),
				FString::Printf(TEXT("%.1fs when it went down, %.1fs a second later, delay %.1f"), AtPlace, AfterSecond, Full));
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

			// Each character speaks from his own bank, only now and then when he kills: his
			// interval, the board's gap, the chance, in that order. Nobody shares a voice.
			if (UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(World))
			{
				using ECelebration = UBDCreepSoundSubsystem::ECelebration;
				const UBDPalaceData* AgentData = LoadObject<UBDPalaceData>(nullptr, TEXT("/Game/BD/Data/DA_PalaceData.DA_PalaceData"));
				const UBDEnemyData* Militant = UBDWaveSettings::Get().ResolveWaveEnemy();
				const TSoftObjectPtr<USoundBase> Bank = ShooterData != nullptr ? ShooterData->CelebrationSound : TSoftObjectPtr<USoundBase>();
				const bool bOwnBanks = !Bank.IsNull() && Bank.GetAssetName() == TEXT("SCue_Nicole_Festejo") && AgentData != nullptr
					&& !AgentData->AgentCelebrationSound.IsNull() && AgentData->AgentCelebrationSound != Bank
					&& (Militant == nullptr || Militant->SpeechSound != Bank);

				ABDEnemyBase* Other = Waves->SpawnEnemy(UBDWaveSettings::Get().ResolveWaveEnemy(), 0);
				const UBDCreepSoundSettings& Voice = UBDCreepSoundSettings::Get();
				const double T = World->GetTimeSeconds() + 1000.0;
				const ECelebration Unlucky = Shooter != nullptr ? Sounds->GateCelebration(*Shooter, Bank, T, Voice.CelebrationChance + 0.01f) : ECelebration::NoBank;
				const ECelebration Lucky = Shooter != nullptr ? Sounds->GateCelebration(*Shooter, Bank, T, 0.0f) : ECelebration::NoBank;
				const ECelebration NoBank = Shooter != nullptr ? Sounds->GateCelebration(*Shooter, TSoftObjectPtr<USoundBase>(), T, 0.0f) : ECelebration::Said;
				ECelebration Again = ECelebration::Said, Busy = ECelebration::Said, Later = ECelebration::NoBank;
				if (Shooter != nullptr && Other != nullptr)
				{
					Sounds->NoteCelebration(*Shooter, T);
					Again = Sounds->GateCelebration(*Shooter, Bank, T + Voice.CelebrationMinInterval * 0.5, 0.0f);
					Busy = Sounds->GateCelebration(*Other, Bank, T + Voice.CelebrationGlobalGap * 0.5, 0.0f);
					Later = Sounds->GateCelebration(*Other, Bank, T + Voice.CelebrationGlobalGap + 0.1, 0.0f);
				}
				if (Other != nullptr)
				{
					Other->Destroy();
				}

				// The kill is what asks, not the shot.
				const int32 AskedBefore = Sounds->GetCelebrationsAsked();
				if (Shooter != nullptr)
				{
					Shooter->NotifyKill();
				}
				const int32 AskedByKill = Sounds->GetCelebrationsAsked() - AskedBefore;

				const bool bGates = Unlucky == ECelebration::Unlucky && Lucky == ECelebration::Said && NoBank == ECelebration::NoBank
					&& Again == ECelebration::TooSoon && Busy == ECelebration::BoardBusy && Later == ECelebration::Said;
				Check(TEXT("FALA"), TEXT("a kill sometimes makes a character say a line from his own bank: chance, his interval, the board's gap"),
					bOwnBanks && bGates && AskedByKill == 1 && Voice.CelebrationChance > 0.0f && Voice.CelebrationChance < 0.5f,
					FString::Printf(TEXT("banks %s (shooter '%s', Agent '%s'), gates %d/%d/%d/%d/%d/%d (Unlucky Said NoBank TooSoon BoardBusy Said wanted), %d ask per kill, chance %.2f, interval %.0f s, gap %.0f s"),
						bOwnBanks ? TEXT("own") : TEXT("WRONG"), *Bank.GetAssetName(), AgentData != nullptr ? *AgentData->AgentCelebrationSound.GetAssetName() : TEXT("no data"),
						static_cast<int32>(Unlucky), static_cast<int32>(Lucky), static_cast<int32>(NoBank), static_cast<int32>(Again), static_cast<int32>(Busy), static_cast<int32>(Later),
						AskedByKill, Voice.CelebrationChance, Voice.CelebrationMinInterval, Voice.CelebrationGlobalGap));
			}

			// A horde of any size asks about the same sentences a second; the words duck the effects,
			// not the music; the special figures stand twice as tall as before.
			if (UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(World))
			{
				const UBDCreepSoundSettings& Voice = UBDCreepSoundSettings::Get();
				const UBDEnemyData* Militant = UBDWaveSettings::Get().ResolveWaveEnemy();
				float RateSmall = 0.0f, RateLarge = 0.0f;
				if (Militant != nullptr)
				{
					const float Low = FMath::Max(0.5f, Militant->VocalIntervalMin);
					const float Mean = 0.5f * (Low + FMath::Max(Low, Militant->VocalIntervalMax));
					RateSmall = 20 * Sounds->GetSpeechShare(*Militant, 20) / Mean;
					RateLarge = 200 * Sounds->GetSpeechShare(*Militant, 200) / Mean;
				}
				const bool bSteady = Militant != nullptr && RateSmall <= Voice.SpeechTargetPerSecond * 1.05f
					&& FMath::IsNearlyEqual(RateLarge, Voice.SpeechTargetPerSecond, Voice.SpeechTargetPerSecond * 0.05f);
				Check(TEXT("FALA"), TEXT("the militants ask about the same sentences a second with 20 or 200 of them"), bSteady,
					FString::Printf(TEXT("%.2f/s with 20, %.2f/s with 200, target %.2f/s (unscaled the 200 would ask %.2f/s)"), RateSmall, RateLarge,
						Voice.SpeechTargetPerSecond, Militant != nullptr ? 200 * Militant->SpeechShare / (0.5f * (FMath::Max(0.5f, Militant->VocalIntervalMin) + Militant->VocalIntervalMax)) : 0.0f));

				// Four categories side by side under the master, each owned by its slider alone; the
				// words duck the effects and the environment while they sound, never the music nor
				// themselves, and the two come back faded.
				const UBDUISettings& UI = UBDUISettings::Get();
				const USoundClass* Master = UI.MasterSoundClass.LoadSynchronous();
				const USoundClass* VoiceClass = UI.VoiceSoundClass.LoadSynchronous();
				const USoundClass* Music = UI.MusicSoundClass.LoadSynchronous();
				const USoundClass* Environment = UI.EnvironmentSoundClass.LoadSynchronous();
				const USoundClass* Effects = UI.EffectsSoundClass.LoadSynchronous();
				FString Tree;
				bool bSiblings = Master != nullptr;
				TSet<const USoundClass*> Distinct;
				for (const USoundClass* Class : { VoiceClass, Music, Environment, Effects })
				{
					bSiblings &= Class != nullptr && Class->ParentClass == Master && Master->ChildClasses.Contains(Class) && Class->ChildClasses.IsEmpty();
					Distinct.Add(Class);
					Tree += FString::Printf(TEXT("%s%s under %s"), Tree.IsEmpty() ? TEXT("") : TEXT(", "), *GetNameSafe(Class),
						Class != nullptr ? *GetNameSafe(Class->ParentClass) : TEXT("-"));
				}
				bSiblings &= Distinct.Num() == 4;
				Check(TEXT("AUDIO"), TEXT("voices, music, environment and effects are four classes side by side under the master, none under another"),
					bSiblings, Tree);

				const UBDSettingsSave* Fresh = GetDefault<UBDSettingsSave>();
				const bool bDefaults = Fresh->GetVolume(EBDSoundCategory::Voice) == 80 && Fresh->GetVolume(EBDSoundCategory::Music) == 20
					&& Fresh->GetVolume(EBDSoundCategory::Environment) == 40 && Fresh->GetVolume(EBDSoundCategory::Effects) == 50
					&& !Fresh->IsMuted(EBDSoundCategory::Voice) && !Fresh->IsMuted(EBDSoundCategory::Music)
					&& !Fresh->IsMuted(EBDSoundCategory::Environment) && !Fresh->IsMuted(EBDSoundCategory::Effects);
				// A save of the old two-slider layout comes up at the defaults, its one mute on all four.
				UBDSettingsSave* Old = NewObject<UBDSettingsSave>();
				Old->AudioLayout = 0;
				Old->MusicVolume = 80;
				Old->EffectsVolume = 80;
				Old->ResetAudio();
				const bool bUpgraded = Old->AudioLayout == UBDSettingsSave::CurrentAudioLayout && Old->GetVolume(EBDSoundCategory::Music) == 20
					&& Old->GetVolume(EBDSoundCategory::Effects) == 50;
				Old->SetMuted(EBDSoundCategory::Environment, true);
				Old->SetVolume(EBDSoundCategory::Voice, 65);
				const bool bOwnFields = Old->IsMuted(EBDSoundCategory::Environment) && !Old->IsMuted(EBDSoundCategory::Effects)
					&& Old->GetVolume(EBDSoundCategory::Voice) == 65 && Old->GetVolume(EBDSoundCategory::Music) == 20;
				Check(TEXT("AUDIO"), TEXT("the options start at voices 80, music 20, environment 40, effects 50, and each slider and mute is its own"),
					bDefaults && bUpgraded && bOwnFields,
					FString::Printf(TEXT("defaults %d/%d/%d/%d, old save brought up %s, a mute and a slider touch only their own %s"),
						Fresh->GetVolume(EBDSoundCategory::Voice), Fresh->GetVolume(EBDSoundCategory::Music),
						Fresh->GetVolume(EBDSoundCategory::Environment), Fresh->GetVolume(EBDSoundCategory::Effects),
						bUpgraded ? TEXT("yes") : TEXT("no"), bOwnFields ? TEXT("yes") : TEXT("no")));

				// Quality on Auto lands at most on High (75% of the screen): the engine's own pick
				// for this machine is Epic, native, which a full screen cannot carry.
				const UBDSettingsSubsystem* GameSettings = World->GetGameInstance() != nullptr ? World->GetGameInstance()->GetSubsystem<UBDSettingsSubsystem>() : nullptr;
				const UGameUserSettings* UserSettings = GEngine != nullptr ? GEngine->GetGameUserSettings() : nullptr;
				if (GameSettings != nullptr && UserSettings != nullptr && GameSettings->Get().QualityLevel < 0)
				{
					const int32 Level = UserSettings->GetOverallScalabilityLevel();
					Check(TEXT("QUALIDADE"), TEXT("quality on Auto never goes past High"), Level >= 0 && Level <= 2,
						FString::Printf(TEXT("overall level %d, resolution quality %.0f%%"), Level, UserSettings->ScalabilityQuality.ResolutionQuality));
				}

				const USoundBase* Words = Militant != nullptr ? Militant->SpeechSound.LoadSynchronous() : nullptr;
				const USoundClass* WordsClass = Words != nullptr ? Words->GetSoundClass() : nullptr;
				const USoundMix* Duck = Voice.SpeechDuckMix.LoadSynchronous();
				const bool bPassive = VoiceClass != nullptr && VoiceClass->PassiveSoundMixModifiers.ContainsByPredicate(
					[Duck](const FPassiveSoundMixModifier& Modifier) { return Modifier.SoundMix == Duck; });
				TSet<const USoundClass*> Ducked;
				FString Adjusters;
				bool bClean = Duck != nullptr;
				for (const FSoundClassAdjuster& Adjuster : Duck != nullptr ? Duck->SoundClassEffects : TArray<FSoundClassAdjuster>())
				{
					Ducked.Add(Adjuster.SoundClassObject);
					bClean &= !Adjuster.bApplyToChildren && Adjuster.VolumeAdjuster >= 0.3f && Adjuster.VolumeAdjuster <= 0.7f;
					Adjusters += FString::Printf(TEXT("%s%s to %.2f%s"), Adjusters.IsEmpty() ? TEXT("") : TEXT(", "), *GetNameSafe(Adjuster.SoundClassObject),
						Adjuster.VolumeAdjuster, Adjuster.bApplyToChildren ? TEXT(" with children") : TEXT(""));
				}
				const bool bDucking = WordsClass != nullptr && WordsClass == VoiceClass && bPassive && bClean
					&& Ducked.Num() == 2 && Ducked.Contains(Effects) && Ducked.Contains(Environment) && Duck->FadeOutTime > 0.0f;
				Check(TEXT("FALA"), TEXT("a line ducks the effects and the environment, never the music nor itself, and they come back faded"), bDucking,
					FString::Printf(TEXT("words in %s, duck %s: %s, fade %.2f/%.2f s"), *GetNameSafe(WordsClass), *GetNameSafe(Duck),
						Adjusters.IsEmpty() ? TEXT("no adjuster") : *Adjusters,
						Duck != nullptr ? Duck->FadeInTime : 0.0f, Duck != nullptr ? Duck->FadeOutTime : 0.0f));

				// The words reach a normal playing zoom whole: a camera at middle height frames its
				// subject from some 14000 cm. Far off they still fade, and the calls keep their own curve.
				const auto SpeechDb = [&Voice](const float Distance)
				{
					return Distance <= Voice.SpeechInnerRadius ? 0.0f
						: FMath::Min(1.0f, (Distance - Voice.SpeechInnerRadius) / FMath::Max(1.0f, Voice.SpeechFalloffDistance)) * Voice.SpeechAttenuationAtMax;
				};
				const float AtMiddle = SpeechDb(15000.0f);
				const float AtOverview = SpeechDb(33000.0f);
				Check(TEXT("FALA"), TEXT("a line is heard whole at a middle zoom and still fades far off, on a curve wider than the calls'"),
					AtMiddle >= -6.0f && AtOverview <= -20.0f && Voice.GetSpeechReach() >= 33000.0f && Voice.SpeechInnerRadius > Voice.VoiceInnerRadius,
					FString::Printf(TEXT("words full to %.0f cm, %.1f dB at 15000, %.1f dB at 33000, reach %.0f; calls full to %.0f"),
						Voice.SpeechInnerRadius, AtMiddle, AtOverview, Voice.GetSpeechReach(), Voice.VoiceInnerRadius));

				const UBDPalaceData* Palace = LoadObject<UBDPalaceData>(nullptr, TEXT("/Game/BD/Data/DA_PalaceData.DA_PalaceData"));
				const UBDEnemyData* Candidate = LoadObject<UBDEnemyData>(nullptr, TEXT("/Game/Elenco/Candidatos/Candidato_01/DA_Candidato_01.DA_Candidato_01"));
				const bool bTall = Palace != nullptr && FMath::IsNearlyEqual(Palace->AgentMeshScale, 2.0f) && FMath::IsNearlyEqual(Palace->WalkAnimRate, 0.5f)
					&& Candidate != nullptr && Candidate->MeshScale.Equals(FVector(6.0f, 6.0f, 8.0f)) && Militant != nullptr && Militant->MeshScale.Z < Candidate->MeshScale.Z;
				Check(TEXT("TAMANHO"), TEXT("the Agent and the candidates stand twice their old size; the militants as they were"), bTall,
					FString::Printf(TEXT("Agent x%.2f (walk loop at %.2f), candidate %s, militant %s"), Palace != nullptr ? Palace->AgentMeshScale : 0.0f,
						Palace != nullptr ? Palace->WalkAnimRate : 0.0f, Candidate != nullptr ? *Candidate->MeshScale.ToCompactString() : TEXT("-"),
						Militant != nullptr ? *Militant->MeshScale.ToCompactString() : TEXT("-")));
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
			UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(World);
			const int32 CelebrationsBefore = Sounds != nullptr ? Sounds->GetCelebrationsAsked() : 0;
			const int32 KickLinesBefore = Sounds != nullptr ? Sounds->GetAlwaysAsked() : 0;
			const float Before = Agent->GetPatrolRemaining();
			Agent->NotifyKill();
			const float Earned = Agent->GetPatrolRemaining() - Before;
			const bool bKicked = Agent->Kick();
			const int32 CelebrationsAsked = Sounds != nullptr ? Sounds->GetCelebrationsAsked() - CelebrationsBefore : 0;
			const int32 KickLinesAsked = Sounds != nullptr ? Sounds->GetAlwaysAsked() - KickLinesBefore : 0;

			// One voice, two events: a celebration that may come on a kill, a line on every kick.
			const bool bVoice = PalaceData != nullptr && PalaceData->AgentCelebrationSound.GetAssetName() == TEXT("SCue_Mito_Festejo")
				&& PalaceData->AgentKickSound.GetAssetName() == TEXT("SCue_Mito_Chute");
			Check(TEXT("FALA"), TEXT("the Agent's kill asks for a celebration and every kick asks for its own line, from his two banks"),
				bVoice && bKicked && CelebrationsAsked == 1 && KickLinesAsked == 1,
				FString::Printf(TEXT("banks '%s' / '%s', %d celebration ask(s) on the kill, %d kick line ask(s) on the kick"),
					PalaceData != nullptr ? *PalaceData->AgentCelebrationSound.GetAssetName() : TEXT("no data"),
					PalaceData != nullptr ? *PalaceData->AgentKickSound.GetAssetName() : TEXT("no data"), CelebrationsAsked, KickLinesAsked));
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

		// The ministers (slice 3a): out of the STF door on the edge with no mouth, black and
		// twice a man, free on the board; the Agent runs at one, kicks, and he flies, cries,
		// falls and runs back.
		if (UBDMinisterSubsystem* Ministers = UBDMinisterSubsystem::Get(World))
		{
			const UBDMinisterSettings& MinisterSettings = UBDMinisterSettings::Get();
			FBDCellCoord Door;
			FVector Outside;
			const AActor* Stf = nullptr;
			const bool bDoor = Ministers->FindDoor(Door, Outside, Stf);
			const bool bOnEdge = bDoor && (Door.X == 0 || Door.X == Grid->GetSizeX() - 1 || Door.Y == 0 || Door.Y == Grid->GetSizeY() - 1);
			bool bMouthOnEdge = false;
			for (const FBDSpawnPoint& Mouth : Waves->GetSpawnPoints())
			{
				bMouthOnEdge |= (Door.X == 0 && Mouth.AnchorExit.X == 0) || (Door.X == Grid->GetSizeX() - 1 && Mouth.AnchorExit.X == Grid->GetSizeX() - 1)
					|| (Door.Y == 0 && Mouth.AnchorExit.Y == 0) || (Door.Y == Grid->GetSizeY() - 1 && Mouth.AnchorExit.Y == Grid->GetSizeY() - 1);
			}
			const int32 Before = Ministers->GetMinisters().Num();
			const int32 Spawned = Ministers->SpawnMinisters(1);
			TArray<ABDMinister*> Out = Ministers->GetMinisters();
			ABDMinister* Minister = Out.Num() > Before ? Out.Last() : nullptr;
			const UStaticMeshComponent* Block = Minister != nullptr ? Minister->FindComponentByClass<UStaticMeshComponent>() : nullptr;
			Check(TEXT("MINISTRO"), TEXT("the STF lets a black minister out of its door on the board edge no mouth opens on, twice a man"),
				bOnEdge && !bMouthOnEdge && Spawned == 1 && Minister != nullptr && Block != nullptr && Block->GetRelativeScale3D().Equals(FVector(4.0f, 4.0f, 8.0f))
					&& MinisterSettings.BlockColor.GetLuminance() < 0.05f,
				FString::Printf(TEXT("STF %s, door %s (on an edge %s, a mouth on it %s), %d out, block %s"), Stf != nullptr ? TEXT("found") : TEXT("missing"),
					*Door.ToString(), bOnEdge ? TEXT("yes") : TEXT("no"), bMouthOnEdge ? TEXT("yes") : TEXT("no"), Spawned,
					Block != nullptr ? *Block->GetRelativeScale3D().ToCompactString() : TEXT("-")));

			// He strolls: from the door, ten seconds of ticks move him over cells he may stand on.
			ABDAgent* Kicker = nullptr;
			for (TActorIterator<ABDAgent> It(World); It; ++It)
			{
				Kicker = *It;
			}
			if (Minister != nullptr && Kicker != nullptr)
			{
				FMath::RandInit(20261010);
				Minister->DebugPlaceOnBoard(Grid->CellToWorld(Door));
				const FVector StrollStart = Minister->GetActorLocation();
				FBDCellCoord Under;
				bool bStood = true;
				for (int32 Frame = 0; Frame < 300; ++Frame)
				{
					Minister->Tick(1.0f / 30.0f);
					bStood &= Grid->WorldToCell(Minister->GetActorLocation(), Under) && Grid->GetCellState(Under) != EBDCellState::Blocked;
				}
				const float Strolled = FVector::Dist2D(StrollStart, Minister->GetActorLocation());
				Check(TEXT("MINISTRO"), TEXT("a minister strolls the board free of the grid, never on a blocked cell"),
					Strolled > 50.0f && bStood, FString::Printf(TEXT("%.0f cm in 10 s, %s"), Strolled, *Minister->Describe()));

				// A divider stops the Mito but not a minister: he steps over it, in a straight
				// line and on a planned way alike. Blocked cells still stop both.
				{
					const FBDCellCoord Left(Grid->GetSizeX() / 2, Grid->GetSizeY() / 2);
					const FBDCellCoord Right(Left.X + 1, Left.Y);
					const FBDEdgeCoord Fence(Left, FBDEdgeCoord::DirectionX);
					const FBDFreeWalk MinisterRules = Minister->Walk();
					const FBDFreeWalk AgentRules(World, Grid);
					const bool bFenced = !Grid->IsEdgeBlockedAt(Fence) && Grid->CanBlockEdge(Fence) && Grid->SetEdgeBlocked(Fence, true);
					const FVector From = Grid->CellToWorld(Left);
					const FVector To = Grid->CellToWorld(Right);
					const bool bMinisterSteps = MinisterRules.CanStep(Left, Right) && MinisterRules.IsLineClear(From, To, 0.0f)
						&& MinisterRules.PlanWay(From, To).Num() == 1;
					const bool bAgentStopped = !AgentRules.CanStep(Left, Right) && !AgentRules.IsLineClear(From, To, 0.0f);
					if (bFenced)
					{
						Grid->SetEdgeBlocked(Fence, false);
					}
					Check(TEXT("MINISTRO"), TEXT("a minister steps over a divider the Mito has to go around"),
						bFenced && bMinisterSteps && bAgentStopped,
						FString::Printf(TEXT("fence %s between %s and %s, minister %s, Mito %s"), bFenced ? TEXT("up") : TEXT("refused"),
							*Left.ToString(), *Right.ToString(), bMinisterSteps ? TEXT("steps over") : TEXT("STOPPED"), bAgentStopped ? TEXT("goes around") : TEXT("WALKS THROUGH")));
				}

				// Left to it for four minutes, by stretches of up to eight cells, he leaves the STF's doorstep.
				float Farthest = Strolled;
				for (int32 Frame = 0; Frame < 30 * 240; ++Frame)
				{
					Minister->Tick(1.0f / 30.0f);
					bStood &= Grid->WorldToCell(Minister->GetActorLocation(), Under) && Grid->GetCellState(Under) != EBDCellState::Blocked;
					Farthest = FMath::Max(Farthest, FVector::Dist2D(StrollStart, Minister->GetActorLocation()));
				}
				const float FarthestCells = Farthest / Grid->GetCellSize();
				const float Wanted = 8.0f;
				Check(TEXT("MINISTRO"), TEXT("a minister strolls by stretches of up to eight cells, away from the STF's doorstep"),
					MinisterSettings.WalkCellsMax == 8 && FarthestCells >= Wanted && bStood,
					FString::Printf(TEXT("farthest %.1f cell(s) from the door in 4 min (wanted %.1f), %s"), FarthestCells, Wanted, *Minister->Describe()));

				// The Agent sees him in reach, runs at him and kicks; the foot sends him flying.
				UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(World);
				UBDSpeechMarkSubsystem* Marks = UBDSpeechMarkSubsystem::Get(World);
				const int32 KickLinesBefore = Sounds != nullptr ? Sounds->GetAlwaysAsked() : 0;
				const int32 BlackBefore = Marks != nullptr ? Marks->GetMarksStarted(EBDSpeakerSide::Minister) : 0;
				// Toward the middle of the board: facing out from an edge, ahead of him is off it.
				Kicker->DebugWake();
				const FVector TowardMiddle = (Grid->CellToWorld(FBDCellCoord(Grid->GetSizeX() / 2, Grid->GetSizeY() / 2)) - Kicker->GetActorLocation()).GetSafeNormal2D();
				Minister->DebugPlaceOnBoard(Kicker->GetActorLocation() + TowardMiddle * Grid->GetCellSize() * 2.0f);
				for (int32 Frame = 0; Frame < 240 && Kicker->GetMinistersKicked() == 0; ++Frame)
				{
					Kicker->Tick(1.0f / 30.0f);
				}
				const bool bKicked = Kicker->GetMinistersKicked() > 0 && Minister->GetState() == EBDMinisterState::Flying;
				for (int32 Frame = 0; Frame < 300 && Minister->GetState() != EBDMinisterState::Fleeing; ++Frame)
				{
					Minister->Tick(1.0f / 30.0f);
				}
				const bool bFled = Minister->GetState() == EBDMinisterState::Fleeing;
				const int32 KickLines = Sounds != nullptr ? Sounds->GetAlwaysAsked() - KickLinesBefore : 0;
				const int32 BlackMarks = Marks != nullptr ? Marks->GetMarksStarted(EBDSpeakerSide::Minister) - BlackBefore : 0;
				Check(TEXT("MINISTRO"), TEXT("the Agent runs at a minister in reach and kicks: he flies, cries out in black, falls and runs for the STF"),
					bKicked && Minister->GetLastFlightPeak() > MinisterSettings.FlightHeight * 0.9f && bFled && KickLines == 1 && BlackMarks == 1,
					FString::Printf(TEXT("kicked %s, flight peak %.0f cm, %s, kick line asked %d, black mark %d"), bKicked ? TEXT("yes") : TEXT("no"),
						Minister->GetLastFlightPeak(), *Minister->Describe(), KickLines, BlackMarks));
				Minister->Destroy();
			}

			// The refuge (slice 3b): a militant inside his radius stops him and he reads; the
			// militant is covered (no hit lands, no defender aims) and the dome goes up. With
			// the radius empty he reads on and then walks. The Agent's kick drops the cover.
			{
				// The line of eleven: they come out in order, each covering half a cell more than
				// the one before; one back in goes to the end, out again only after the others.
				for (ABDMinister* Old : Ministers->GetMinisters())
				{
					Old->Destroy();
				}
				const TArray<int32> LineBefore = Ministers->GetLine();
				const int32 First = LineBefore.Num() > 1 ? LineBefore[0] : 0;
				TArray<ABDMinister*> Pair;
				Ministers->SpawnMinisters(2);
				for (ABDMinister* OutNow : Ministers->GetMinisters())
				{
					Pair.Add(OutNow);
				}
				Pair.Sort([](const ABDMinister& A, const ABDMinister& B) { return A.GetSeat() < B.GetSeat(); });
				const bool bTwo = Pair.Num() == 2;
				const int32 SeatA = bTwo ? Pair[0]->GetSeat() : 0;
				const int32 SeatB = bTwo ? Pair[1]->GetSeat() : 0;
				const float CoverA = bTwo ? Pair[0]->GetRefugeRadiusCells() : 0.0f;
				const float CoverB = bTwo ? Pair[1]->GetRefugeRadiusCells() : 0.0f;
				const bool bWider = bTwo && SeatB == SeatA + 1 && FMath::IsNearlyEqual(CoverB - CoverA, MinisterSettings.RefugeRadiusStepCells);

				// The Mito's reach for ministers: his reach for the horde plus a quarter cell for
				// each one standing; a kicked one takes his quarter back at once.
				if (bTwo && Kicker != nullptr)
				{
					const float Cell = Grid->GetCellSize();
					const float Base = Kicker->GetDetectionRadius();
					Pair[0]->DebugPlaceOnBoard(Grid->CellToWorld(Door));
					Pair[1]->DebugPlaceOnBoard(Grid->CellToWorld(Door));
					const float WithTwo = (Kicker->GetMinisterReachRadius() - Base) / Cell;
					Pair[0]->ReceiveKick(*Kicker);
					const float WithOne = (Kicker->GetMinisterReachRadius() - Base) / Cell;
					const float ReachStep = MinisterSettings.AgentReachStepCells;
					Check(TEXT("MINISTRO"), TEXT("the Mito's reach for ministers is his reach for the horde plus a quarter cell per minister standing, back down for each one kicked"),
						FMath::IsNearlyEqual(WithTwo, 2.0f * ReachStep, 0.01f) && FMath::IsNearlyEqual(WithOne, ReachStep, 0.01f) && Pair[0]->GetState() == EBDMinisterState::Flying,
						FString::Printf(TEXT("horde reach %.2f cells, +%.2f with two standing, +%.2f with one kicked (%s)"), Base / Cell, WithTwo, WithOne, *Pair[0]->Describe()));
				}
				if (bTwo)
				{
					Pair[0]->Destroy();
					Pair[1]->Destroy();
				}
				const TArray<int32>& LineAfter = Ministers->GetLine();
				const bool bToTheEnd = LineAfter.Num() == LineBefore.Num() && LineAfter.Num() >= 2 && LineAfter[LineAfter.Num() - 2] == SeatA;
				Check(TEXT("REFUGIO"), TEXT("the STF's line: the next out covers half a cell wider, and one back in waits behind the rest"),
					bWider && SeatA == First && bToTheEnd,
					FString::Printf(TEXT("seats %d and %d, cover %.1f and %.1f cells, %d inside, seat %d back at %d of %d"),
						SeatA, SeatB, CoverA, CoverB, LineAfter.Num(), SeatA, LineAfter.IndexOfByKey(SeatA) + 1, LineAfter.Num()));
			}

			UBDWaveSubsystem* RefugeWaves = World->GetSubsystem<UBDWaveSubsystem>();
			const int32 RefugeBefore = Ministers->GetMinisters().Num();
			Ministers->SpawnMinisters(1);
			const TArray<ABDMinister*> RefugeOut = Ministers->GetMinisters();
			ABDMinister* Reader = RefugeOut.Num() > RefugeBefore ? RefugeOut.Last() : nullptr;
			ABDEnemyBase* Covered = RefugeWaves != nullptr ? RefugeWaves->SpawnEnemy(UBDWaveSettings::Get().ResolveWaveEnemy(), 0) : nullptr;
			if (Reader != nullptr && Covered != nullptr && Kicker != nullptr)
			{
				const float Cell = Grid->GetCellSize();
				const float Frame30 = 1.0f / 30.0f;
				Kicker->DebugWake();
				// Toward the middle of the board, whichever way the Agent faces after the kick above.
				const FVector BoardMiddle = Grid->CellToWorld(FBDCellCoord(Grid->GetSizeX() / 2, Grid->GetSizeY() / 2));
				const FVector Inward = (BoardMiddle - Kicker->GetActorLocation()).GetSafeNormal2D();
				const FVector Spot = Kicker->GetActorLocation() + Inward * Cell * 6.0f;
				Reader->DebugPlaceOnBoard(Spot);
				Covered->SetActorLocation(Reader->GetActorLocation() + FVector(Cell * Reader->GetRefugeRadiusCells() * 0.5f, 0.0f, 0.0f));
				Reader->Tick(Frame30);
				const float HealthBefore = Covered->GetCurrentHealth();
				Covered->ApplyDamage(HealthBefore * 0.5f, Kicker);
				const bool bCoverHeld = FMath::IsNearlyEqual(Covered->GetCurrentHealth(), HealthBefore);
				Check(TEXT("REFUGIO"), TEXT("a militant in a minister's radius stops him reading, under the dome: covered, no hit lands"),
					Reader->GetState() == EBDMinisterState::Reading && Reader->IsShieldUp() && Reader->GetShelteredCount() == 1
						&& Covered->IsSheltered() && bCoverHeld,
					FString::Printf(TEXT("%s, militant %s, %.0f -> %.0f hp"), *Reader->Describe(),
						Covered->IsSheltered() ? TEXT("covered") : TEXT("in plain sight"), HealthBefore, Covered->GetCurrentHealth()));

				// The radius empties: the dome goes, he reads on, and after the wait he walks.
				Covered->SetActorLocation(Reader->GetActorLocation() + FVector(Cell * (Reader->GetRefugeRadiusCells() + 3.0f), 0.0f, 0.0f));
				float Read = 0.0f;
				bool bShieldDown = true;
				while (Read < MinisterSettings.ResumeAfterSeconds - 0.5f)
				{
					Reader->Tick(Frame30);
					Read += Frame30;
					bShieldDown &= !Reader->IsShieldUp();
				}
				const bool bStillReading = Reader->GetState() == EBDMinisterState::Reading;
				for (int32 Frame = 0; Frame < 30; ++Frame)
				{
					Reader->Tick(Frame30);
				}
				Check(TEXT("REFUGIO"), TEXT("with nobody in his radius the dome is down, he reads on and walks again after the wait"),
					bShieldDown && bStillReading && Reader->GetState() != EBDMinisterState::Reading,
					FString::Printf(TEXT("dome %s, reading at %.1f s %s, then %s"), bShieldDown ? TEXT("down") : TEXT("up"), Read,
						bStillReading ? TEXT("yes") : TEXT("no"), *Reader->Describe()));

				// Covering again, in the Agent's reach: he comes for him, and the kick drops the cover at once.
				Reader->DebugPlaceOnBoard(Kicker->GetActorLocation() + Inward * Cell * 2.0f);
				Covered->SetActorLocation(Reader->GetActorLocation() + FVector(Cell * Reader->GetRefugeRadiusCells() * 0.5f, 0.0f, 0.0f));
				Reader->Tick(Frame30);
				const int32 KicksBefore = Kicker->GetMinistersKicked();
				for (int32 Frame = 0; Frame < 300 && Kicker->GetMinistersKicked() == KicksBefore; ++Frame)
				{
					Reader->Tick(Frame30);
					Kicker->Tick(Frame30);
				}
				const bool bKickedReading = Kicker->GetMinistersKicked() > KicksBefore && Reader->GetState() == EBDMinisterState::Flying;
				const float HealthExposed = Covered->GetCurrentHealth();
				Covered->ApplyDamage(1.0f, Kicker);
				Check(TEXT("REFUGIO"), TEXT("the Agent kicks the reading minister: the dome is down and the militant is a target again"),
					bKickedReading && !Reader->IsShieldUp() && !Covered->IsSheltered() && Covered->GetCurrentHealth() < HealthExposed,
					FString::Printf(TEXT("%s, militant %s, %.0f -> %.0f hp"), *Reader->Describe(),
						Covered->IsSheltered() ? TEXT("still covered") : TEXT("exposed"), HealthExposed, Covered->GetCurrentHealth()));
			}
			else
			{
				Check(TEXT("REFUGIO"), TEXT("a minister, a militant and the Agent for the refuge"), false,
					FString::Printf(TEXT("minister %s, militant %s, agent %s"), Reader != nullptr ? TEXT("yes") : TEXT("no"),
						Covered != nullptr ? TEXT("yes") : TEXT("no"), Kicker != nullptr ? TEXT("yes") : TEXT("no")));
			}
			if (Reader != nullptr)
			{
				Reader->Destroy();
			}
			if (Covered != nullptr)
			{
				Covered->Destroy();
			}

			// On an empty bar he walks home past the horde, yet a minister in his reach still
			// gets the kick, and from there he takes the way home again and lies down.
			const int32 LateBefore = Ministers->GetMinisters().Num();
			Ministers->SpawnMinisters(1);
			const TArray<ABDMinister*> LateOut = Ministers->GetMinisters();
			ABDMinister* Late = LateOut.Num() > LateBefore ? LateOut.Last() : nullptr;
			ABDEnemyBase* Passer = RefugeWaves != nullptr ? RefugeWaves->SpawnEnemy(UBDWaveSettings::Get().ResolveWaveEnemy(), 0) : nullptr;
			if (Late != nullptr && Passer != nullptr && Kicker != nullptr)
			{
				const float Cell = Grid->GetCellSize();
				const float Frame30 = 1.0f / 30.0f;
				Kicker->DebugWake();
				const FVector Inward = (Grid->CellToWorld(FBDCellCoord(Grid->GetSizeX() / 2, Grid->GetSizeY() / 2)) - Kicker->GetActorLocation()).GetSafeNormal2D();
				Kicker->DebugSetPatrolRemaining(0.0f);
				const bool bHomeBound = Kicker->GetState() == EBDAgentState::Returning;
				Late->DebugPlaceOnBoard(Kicker->GetActorLocation() + Inward * Cell * 2.0f);
				Passer->SetActorLocation(Kicker->GetActorLocation() - Inward * Cell * 1.5f);
				const float PasserHealth = Passer->GetCurrentHealth();
				const int32 KicksBefore = Kicker->GetMinistersKicked();
				for (int32 Frame = 0; Frame < 300 && Kicker->GetMinistersKicked() == KicksBefore; ++Frame)
				{
					Kicker->Tick(Frame30);
				}
				const bool bKickedHomeBound = Kicker->GetMinistersKicked() > KicksBefore;
				bool bBackOnWay = false;
				for (int32 Frame = 0; Frame < 30 * 60 && Kicker->GetState() != EBDAgentState::Sleeping; ++Frame)
				{
					Kicker->Tick(Frame30);
					bBackOnWay |= Kicker->GetState() == EBDAgentState::Returning;
				}
				const EBDAgentState Final = Kicker->GetState();
				Check(TEXT("AGENTE"), TEXT("on an empty bar he walks home past the horde, kicks a minister in reach, and takes the way home again"),
					bHomeBound && bKickedHomeBound && bBackOnWay && Final == EBDAgentState::Sleeping && FMath::IsNearlyEqual(Passer->GetCurrentHealth(), PasserHealth),
					FString::Printf(TEXT("home bound %s, minister kicked %s, back on the way %s, then %s, militant beside him %.0f -> %.0f hp"),
						bHomeBound ? TEXT("yes") : TEXT("no"), bKickedHomeBound ? TEXT("yes") : TEXT("no"), bBackOnWay ? TEXT("yes") : TEXT("no"),
						*StaticEnum<EBDAgentState>()->GetNameStringByValue(static_cast<int64>(Final)), PasserHealth, Passer->GetCurrentHealth()));
			}
			else
			{
				Check(TEXT("AGENTE"), TEXT("a minister, a militant and the Agent for the way home"), false,
					FString::Printf(TEXT("minister %s, militant %s, agent %s"), Late != nullptr ? TEXT("yes") : TEXT("no"),
						Passer != nullptr ? TEXT("yes") : TEXT("no"), Kicker != nullptr ? TEXT("yes") : TEXT("no")));
			}
			if (Late != nullptr)
			{
				Late->Destroy();
			}
			if (Passer != nullptr)
			{
				Passer->Destroy();
			}
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
		{
			const int32 SavesBefore = Match->GetSavesRemaining();
			const bool bSaved = Match->SaveMatch();
			Check(TEXT("SAVE"), TEXT("a save is refused while a wave is out, and nothing is spent"),
				Match->GetPhase() == EBDMatchPhase::WaveActive && !bSaved && Match->GetSavesRemaining() == SavesBefore && !UBDMatchSave::Exists(),
				FString::Printf(TEXT("phase %s, %s, %d save(s) left of %d"), *StaticEnum<EBDMatchPhase>()->GetNameStringByValue(static_cast<int64>(Match->GetPhase())),
					bSaved ? TEXT("WRITTEN") : *Match->GetSaveRefusal(), Match->GetSavesRemaining(), SavesBefore));
		}
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
		// cell a creep is heading into is refused, as blocking the path. The wave's creeps are
		// still held back here, so whether one happens to be walking depends on timing; when
		// none is, one is sent out of each mouth in turn (its route goes in before it ticks)
		// until one heads into a free cell, and taken off the board after.
		{
			const auto IsWalkingIntoFree = [Grid](const ABDEnemyBase* Enemy)
			{
				return Enemy != nullptr && !Enemy->IsOnFinalLeg() && !Enemy->HasArrived() && Grid->GetCellState(Enemy->GetHeadingCell()) == EBDCellState::Free;
			};
			const ABDEnemyBase* Walker = nullptr;
			for (const ABDEnemyBase* Enemy : Waves->GetLivingEnemiesRef())
			{
				if (IsWalkingIntoFree(Enemy))
				{
					Walker = Enemy;
					break;
				}
			}
			TArray<ABDEnemyBase*> Helpers;
			for (int32 Mouth = 0; Walker == nullptr && Mouth < Waves->GetSpawnPoints().Num(); ++Mouth)
			{
				ABDEnemyBase* Helper = Waves->SpawnEnemy(UBDWaveSettings::Get().ResolveWaveEnemy(), Mouth);
				if (Helper != nullptr)
				{
					Helpers.Add(Helper);
					Walker = IsWalkingIntoFree(Helper) ? Helper : nullptr;
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
					FString::Printf(TEXT("%s heading into %s%s: %s (%s)"), *Walker->GetName(), *Walker->GetHeadingCell().ToString(),
						Helpers.Contains(Walker) ? TEXT(", sent out for the check") : TEXT(""), bTrapped ? TEXT("PLACED") : TEXT("refused"), *RefusalName(TrapRefusal)));
			}
			else
			{
				Check(TEXT("COLOCACAO"), TEXT("a piece that would fence a walking creep in is refused"), false,
					FString::Printf(TEXT("nothing to check against: %s, %d creep(s) sent out and none heading into a free cell"),
						Platform != nullptr ? TEXT("platform found") : TEXT("NO platform piece"), Helpers.Num()));
			}
			for (ABDEnemyBase* Helper : Helpers)
			{
				Helper->Destroy();
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

			// Its sound: the words through the voice class, the call, steps and falls through the
			// effects, so each slider of the options owns its own, and a step on its loop wherever
			// a foot lands. Sound itself cannot be heard in a -nosound run; the wiring can be checked.
			if (WaveEnemy->IsVocal() || !WaveEnemy->FootstepSound.IsNull())
			{
				const USoundClass* Effects = UBDUISettings::Get().EffectsSoundClass.LoadSynchronous();
				const USoundClass* VoiceClass = UBDUISettings::Get().VoiceSoundClass.LoadSynchronous();
				int32 Routed = 0;
				int32 Sounds = 0;
				int32 Slot = 0;
				// The words first: the one slot of the voices.
				for (const TSoftObjectPtr<USoundBase>& Each : { WaveEnemy->SpeechSound, WaveEnemy->CallSound, WaveEnemy->FootstepSound, WaveEnemy->DeathSound, WaveEnemy->BodyFallSound })
				{
					const USoundClass* Wanted = Slot++ == 0 ? VoiceClass : Effects;
					if (const USoundBase* Sound = Each.LoadSynchronous())
					{
						++Sounds;
						const USoundClass* Class = Sound->GetSoundClass();
						Routed += Class != nullptr && Class == Wanted ? 1 : 0;
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
				Check(TEXT("SOM"), TEXT("a vocal creep's words go through the voice class and its other sounds through the effects, its loop steps where the feet land and every fall lands with a thud"),
					Sounds > 0 && Routed == Sounds && (WaveEnemy->FootstepSound.IsNull() || Steps > 0)
						&& (WaveEnemy->BodyFallSound.IsNull() || FallsLanding == WaveEnemy->DeathAnimations.Num()),
					FString::Printf(TEXT("%d of %d sounds on their class (%s, %s), %d step notifies on the loop, %d of %d falls land"), Routed, Sounds, *GetNameSafe(VoiceClass), *GetNameSafe(Effects), Steps,
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
		UBDMinisterSubsystem* Stf = UBDMinisterSubsystem::Get(World);
		const int32 MinistersBefore = Stf != nullptr ? Stf->GetMinisters().Num() : 0;
		Candidate->ApplyDamage(Candidate->GetMaxHealth() * 10.0f, nullptr);
		const int32 MinistersAfter = Stf != nullptr ? Stf->GetMinisters().Num() : 0;
		Check(TEXT("REFUGIO"), TEXT("a candidate killed lets one minister out of the STF"),
			MinistersAfter == MinistersBefore + 1 || (Stf != nullptr && Stf->GetLine().Num() == 0 && MinistersAfter == MinistersBefore),
			FString::Printf(TEXT("%d -> %d out"), MinistersBefore, MinistersAfter));
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
		break;
	}

	case 9:
	{
		// After the defeat: undo the board, which takes the ministers with it.
		UBDMinisterSubsystem* Ministers = GetWorld()->GetSubsystem<UBDMinisterSubsystem>();
		if (Ministers == nullptr)
		{
			Check(TEXT("MINISTRO"), TEXT("undoing the board takes every minister off and fills the STF again"), false, TEXT("no minister subsystem"));
			return false;
		}
		Ministers->SpawnMinisters(2);
		const int32 OutBefore = Ministers->GetMinisters().Num();
		Match->DebugForcePhase(EBDMatchPhase::Building);
		TArray<int32> FullLine;
		for (int32 Seat = 1; Seat <= UBDMinisterSettings::Get().MaxOnBoard; ++Seat) { FullLine.Add(Seat); }
		Check(TEXT("MINISTRO"), TEXT("undoing the board takes every minister off and fills the STF again, seats 1 to 11 in order"),
			OutBefore > 0 && Ministers->GetMinisters().Num() == 0 && Ministers->GetLine() == FullLine && Candidates->GetFallenCount() == 0,
			FString::Printf(TEXT("%d out before, %d after, %d in the STF, %d fallen left"), OutBefore, Ministers->GetMinisters().Num(),
				Ministers->GetLine().Num(), Candidates->GetFallenCount()));

		// A restore point with something in it: wave 12 held, one candidate fallen, the count
		// levelled once and blue ahead, the clock part run, the mouths wherever they are.
		Match->DebugSetWave(12);
		if (ABDCandidate* Felled = Candidates->SpawnCandidate(TEXT("BD.Test.Regression")))
		{
			Felled->ApplyDamage(Felled->GetMaxHealth() * 10.0f, nullptr);
		}
		Bribes->FlushNow(TEXT("BD.Test.Regression"));
		// Blue first, so the red added never passes it: with a fallen on record that would be a parade.
		Match->AddVotesBlue(FMath::Max(0, Match->GetVotesRed() - Match->GetVotesBlue()) + 20);
		Match->AddVotesRed(3);
		Match->EqualizeVotesDown(TEXT("BD.Test.Regression"));
		Match->AddVotesBlue(7);
		Match->DebugSetTimeUntilNextWave(37.5f);

		const auto MouthsOf = [Waves]()
		{
			FString Out;
			for (const FBDSpawnPoint& Point : Waves->GetSpawnPoints())
			{
				Out += Point.ExitCell.ToString() + TEXT(" ");
			}
			return Out;
		};
		const auto FallenOf = [Candidates]()
		{
			FString Out;
			for (const FBDCandidateRecord& Record : Candidates->CaptureState().Fallen)
			{
				Out += FString::Printf(TEXT("#%d@w%d/%.0f "), Record.Ordinal, Record.Wave, Record.MaxHealth);
			}
			return Out;
		};

		const int32 SavesBefore = Match->GetSavesRemaining();
		const bool bSaved = Match->SaveMatch();
		Check(TEXT("SAVE"), TEXT("a save between waves spends one and writes the test slot, never the player's"),
			bSaved && Match->GetSavesRemaining() == SavesBefore - 1 && UBDMatchSave::Exists() && UBDMatchSave::IsTestSlot(),
			FString::Printf(TEXT("%s, %d -> %d save(s), slot '%s'"), bSaved ? TEXT("written") : *Match->GetSaveRefusal(), SavesBefore,
				Match->GetSavesRemaining(), UBDMatchSave::GetSlotName()));
		SavedWave = Match->GetCurrentWave();
		SavedSaves = Match->GetSavesRemaining();
		SavedLevelDowns = Match->GetLevelDownCount();
		SavedBlue = Match->GetVotesBlue();
		SavedRed = Match->GetVotesRed();
		SavedClock = Match->GetTimeUntilNextWave();
		SavedMouths = MouthsOf();
		SavedFallen = FallenOf();

		// The match goes on - the mouths wander with the next wave - and is lost.
		Match->DebugForcePhase(EBDMatchPhase::WaveActive);
		const FString MouthsAfterWave = MouthsOf();
		if (ABDCandidate* Walker = Candidates->SpawnCandidate(TEXT("BD.Test.Regression")))
		{
			Walker->DebugArrive();
		}
		Check(TEXT("SAVE"), TEXT("a defeat keeps the save, and the panel offers it"),
			Match->GetPhase() == EBDMatchPhase::Defeat && UBDMatchSave::Exists() && Match->HasLoadableSave(),
			FString::Printf(TEXT("phase %s, slot %s, loadable %s"), *StaticEnum<EBDMatchPhase>()->GetNameStringByValue(static_cast<int64>(Match->GetPhase())),
				UBDMatchSave::Exists() ? TEXT("kept") : TEXT("GONE"), Match->HasLoadableSave() ? TEXT("yes") : TEXT("no")));

		// Another difficulty's save, or another match's, is not this match's to load.
		UBDMatchSave* Own = UBDMatchSave::LoadFromSlot();
		bool bForeignRefused = false;
		if (Own != nullptr)
		{
			UBDMatchSave* OtherDifficulty = DuplicateObject<UBDMatchSave>(Own, GetTransientPackage());
			OtherDifficulty->Difficulty = Own->Difficulty == EBDDifficulty::Hard ? EBDDifficulty::Easy : EBDDifficulty::Hard;
			UBDMatchSave* OtherMatch = DuplicateObject<UBDMatchSave>(Own, GetTransientPackage());
			OtherMatch->MatchStart = Own->MatchStart + FTimespan::FromHours(1.0);
			bForeignRefused = !Match->CanLoadSave(*OtherDifficulty, false) && !Match->CanLoadSave(*OtherMatch, false) && Match->CanLoadSave(*Own, false);
		}
		Check(TEXT("SAVE"), TEXT("a save of another difficulty or of another match is refused; the match's own is not"), bForeignRefused,
			Own != nullptr ? (bForeignRefused ? TEXT("other difficulty refused, other match refused, own accepted") : TEXT("WRONG")) : TEXT("no save to read"));

		// The defeat panel's load: the building phase of the saved wave, as it stood.
		const bool bLoaded = Match->LoadMatch();
		const bool bSame = bLoaded && Match->GetPhase() == EBDMatchPhase::Building && Match->GetCurrentWave() == SavedWave
			&& MouthsOf() == SavedMouths && FallenOf() == SavedFallen && Match->GetLevelDownCount() == SavedLevelDowns
			&& FMath::IsNearlyEqual(Match->GetTimeUntilNextWave(), SavedClock, 0.01f) && Match->GetSavesRemaining() == SavedSaves
			&& Match->GetVotesBlue() == SavedBlue && Match->GetVotesRed() == SavedRed;
		Check(TEXT("SAVE"), TEXT("the load reopens the saved wave as it stood: mouths, fallen, levelling, clock, count and saves, spending none"), bSame,
			FString::Printf(TEXT("wave %d/%d, mouths %s (%s; after the wave %s), fallen %s/%s, levellings %d/%d, clock %.1f/%.1f, saves %d/%d, count %d-%d/%d-%d"),
				Match->GetCurrentWave(), SavedWave, MouthsOf() == SavedMouths ? TEXT("same") : TEXT("DIFFERENT"), *SavedMouths,
				MouthsAfterWave == SavedMouths ? TEXT("unmoved") : TEXT("had moved"), *FallenOf(), *SavedFallen, Match->GetLevelDownCount(), SavedLevelDowns,
				Match->GetTimeUntilNextWave(), SavedClock, Match->GetSavesRemaining(), SavedSaves, Match->GetVotesBlue(), Match->GetVotesRed(), SavedBlue, SavedRed));
		Check(TEXT("SAVE"), TEXT("a load takes the ministers off and fills the STF again"),
			Ministers->GetMinisters().Num() == 0 && Ministers->GetLine() == FullLine,
			FString::Printf(TEXT("%d out, %d in the STF"), Ministers->GetMinisters().Num(), Ministers->GetLine().Num()));

		// A save with red ahead and the fallen on record sends nobody out on load.
		if (Own != nullptr)
		{
			UBDMatchSave* RedAhead = DuplicateObject<UBDMatchSave>(Own, GetTransientPackage());
			RedAhead->VotesRed = RedAhead->VotesBlue + 25;
			Match->RestoreMatch(*RedAhead);
		}
		Check(TEXT("SAVE"), TEXT("loading red ahead with the fallen on record sends no candidate out in the building phase"),
			Own != nullptr && Match->GetVotesRed() > Match->GetVotesBlue() && Candidates->GetFallenCount() > 0
				&& !Candidates->HasCandidateOnBoard() && !Candidates->IsAwaitingBus() && !Candidates->IsReturnActive(),
			FString::Printf(TEXT("count %d-%d, %d fallen, candidate %s, parade %s"), Match->GetVotesBlue(), Match->GetVotesRed(), Candidates->GetFallenCount(),
				Candidates->HasCandidateOnBoard() || Candidates->IsAwaitingBus() ? TEXT("OUT") : TEXT("none"), Candidates->IsReturnActive() ? TEXT("ON") : TEXT("none")));
		WaitTicks = 10;
		break;
	}

	case 10:
	{
		// Some ticks later still nobody: nothing but a vote turns the count.
		Check(TEXT("SAVE"), TEXT("ticks after that load still nobody walks out"),
			!Candidates->HasCandidateOnBoard() && !Candidates->IsAwaitingBus() && !Candidates->IsReturnActive(),
			Candidates->HasCandidateOnBoard() || Candidates->IsReturnActive() ? TEXT("SOMEONE CAME OUT") : TEXT("nobody"));
		Match->EqualizeVotesDown(TEXT("BD.Test.Regression"));
		Match->AddVotesBlue(5);

		// The count at the last wave waits on a candidate owed to a later wave.
		Match->DebugSetWave(Match->GetWavesToWin());
		FBDCandidateSaveState Owed = Candidates->CaptureState();
		Owed.bSchedulePending = true;
		Candidates->RestoreState(Owed);
		const bool bHeldOnOwed = !Match->IsEndDue();
		Owed.bSchedulePending = false;
		Candidates->RestoreState(Owed);
		const bool bDueWithout = Match->IsEndDue();
		Check(TEXT("APURACAO"), TEXT("at the last wave the count waits on a candidate still owed, and settles once none is"),
			bHeldOnOwed && bDueWithout, FString::Printf(TEXT("wave %d: owed -> %s, none owed -> %s"), Match->GetCurrentWave(),
				bHeldOnOwed ? TEXT("waits") : TEXT("SETTLES"), bDueWithout ? TEXT("settles") : TEXT("STILL WAITS")));

		// Three ministers standing see the win and the endless through untouched.
		UBDMinisterSubsystem* Ministers = GetWorld()->GetSubsystem<UBDMinisterSubsystem>();
		Ministers->SpawnMinisters(3);
		const TArray<ABDMinister*> Standing = Ministers->GetMinisters();
		TArray<FVector> Where;
		TArray<int32> Seats;
		for (const ABDMinister* Minister : Standing) { Where.Add(Minister->GetActorLocation()); Seats.Add(Minister->GetSeat()); }
		const TArray<int32> LineBefore = Ministers->GetLine();
		Match->DeclareVictory();
		const bool bSlotGone = !UBDMatchSave::Exists();
		Check(TEXT("SAVE"), TEXT("a victory deletes the save"), Match->GetPhase() == EBDMatchPhase::Victory && bSlotGone,
			FString::Printf(TEXT("phase %s, slot %s"), *StaticEnum<EBDMatchPhase>()->GetNameStringByValue(static_cast<int64>(Match->GetPhase())),
				bSlotGone ? TEXT("deleted") : TEXT("STILL THERE")));
		Match->ContinueEndless();
		const TArray<ABDMinister*> After = Ministers->GetMinisters();
		bool bKept = Standing.Num() == 3 && After.Num() == 3 && Ministers->GetLine() == LineBefore;
		for (int32 Index = 0; Index < Standing.Num() && bKept; ++Index)
		{
			bKept = After.Contains(Standing[Index]) && FVector::Dist(Standing[Index]->GetActorLocation(), Where[Index]) < 1.0f
				&& Standing[Index]->GetSeat() == Seats[Index];
		}
		const int32 NextSeat = LineBefore.Num() > 0 ? LineBefore[0] : 0;
		int32 ReleasedSeat = 0;
		if (ABDCandidate* Felled = Candidates->SpawnCandidate(TEXT("BD.Test.Regression")))
		{
			Felled->ApplyDamage(Felled->GetMaxHealth() * 10.0f, nullptr);
		}
		for (const ABDMinister* Minister : Ministers->GetMinisters())
		{
			if (!Standing.Contains(Minister)) { ReleasedSeat = Minister->GetSeat(); }
		}
		Check(TEXT("MINISTRO"), TEXT("the ministers standing at the win are still there in the endless, same place, same seat, and the next kill frees the next seat"),
			Match->IsEndless() && bKept && ReleasedSeat == NextSeat && NextSeat > 0,
			FString::Printf(TEXT("%d of %d kept, seats %s, next seat out %d (expected %d)"), After.Num(), Standing.Num(), bKept ? TEXT("same") : TEXT("CHANGED"),
				ReleasedSeat, NextSeat));

		break;
	}

	case 11:
	{
		// The rally, on the endless board the win left: nothing else on it, the count far in
		// the blue so no arrival turns it, the Mito awake on a long bar.
		Waves->DespawnAll();
		Match->AddVotesBlue(5000);

		// The numbers of the cast: the first and the twentieth, as the scaffold wrote them.
		const UBDCandidateData* First = UBDCandidateData::LoadForOrdinal(1);
		const UBDCandidateData* Last = UBDCandidateData::LoadForOrdinal(FBDRallyParams::MaxOrdinal);
		const auto Reads = [](const UBDCandidateData* Data, const float Every, const int32 Count)
		{
			return Data != nullptr && Data->Rally.bRallies && FMath::IsNearlyEqual(Data->Rally.RallyEvery, Every)
				&& Data->Rally.RallyCount == Count && FMath::IsNearlyEqual(Data->Rally.RallyFirstDelay, 6.0f)
				&& FMath::IsNearlyEqual(Data->Rally.RallyTellSeconds, 1.5f) && FMath::IsNearlyEqual(Data->Rally.RallyDuration, 4.0f)
				&& Data->Rally.RallyComposition.Num() == 1 && Data->Rally.RallyComposition[0].Enemy.GetAssetName() == TEXT("DA_Horda_01");
		};
		Check(TEXT("COMICIO"), TEXT("every candidate's DA carries his rallies: the first every 20 s raising 2, the twentieth every 10 s raising 40, 6 s off the bus, tell 1.5 s, speech 4 s, DA_Horda_01"),
			Reads(First, 20.0f, 2) && Reads(Last, 10.0f, 40),
			FString::Printf(TEXT("01: %s every %.1fs x%d; 20: %s every %.1fs x%d"),
				First != nullptr ? TEXT("candidate data") : TEXT("NOT candidate data"), First != nullptr ? First->Rally.RallyEvery : 0.0f, First != nullptr ? First->Rally.RallyCount : 0,
				Last != nullptr ? TEXT("candidate data") : TEXT("NOT candidate data"), Last != nullptr ? Last->Rally.RallyEvery : 0.0f, Last != nullptr ? Last->Rally.RallyCount : 0));

		ABDCandidate* Candidate = Candidates->SpawnCandidate(TEXT("BD.Test.Regression"));
		UBDRallyComponent* Rally = Candidate != nullptr ? Candidate->GetRally() : nullptr;
		if (Rally == nullptr)
		{
			Check(TEXT("COMICIO"), TEXT("a candidate walks out with his rallies"), false, TEXT("no candidate could be sent"));
			return false;
		}
		const FBDRallyParams Expected = UBDCandidateData::RallyForOrdinal(Candidate->Ordinal);
		Check(TEXT("COMICIO"), TEXT("he walks out with the rallies of his own number, armed, the first one RallyFirstDelay after the bus"),
			Rally->IsEnabled() && Rally->GetState() == EBDRallyState::Walking && Rally->GetParams().RallyCount == Expected.RallyCount
				&& FMath::IsNearlyEqual(Rally->GetParams().RallyEvery, Expected.RallyEvery) && FMath::IsNearlyEqual(Rally->GetNextRallyIn(), Expected.RallyFirstDelay, 0.05f),
			FString::Printf(TEXT("candidate %d: %s, %d a crowd every %.1fs, first in %.1fs"), Candidate->Ordinal, Rally->IsEnabled() ? TEXT("armed") : TEXT("OFF"),
				Rally->GetParams().RallyCount, Rally->GetParams().RallyEvery, Rally->GetNextRallyIn()));

		// Then sooner and smaller, with the warning and the speech as the DA has them.
		FBDRallyParams Quick = Rally->GetParams();
		Quick.RallyFirstDelay = 0.5f;
		Quick.RallyCount = 6;
		Rally->Setup(Quick, /*bInEnabled*/ true, 7);

		// The console: BD.Candidate.Rally makes him due at once, BD.Candidate.Status tells it.
		GEngine->Exec(World, TEXT("BD.Candidate.Rally"));
		GEngine->Exec(World, TEXT("BD.Candidate.Status"));
		Check(TEXT("COMICIO"), TEXT("BD.Candidate.Rally makes the candidate on the board due for his rally now"),
			Rally->GetState() == EBDRallyState::Walking && Rally->GetNextRallyIn() <= 0.0f,
			FString::Printf(TEXT("%s, next in %.1fs"), UBDRallyComponent::StateName(Rally->GetState()), Rally->GetNextRallyIn()));

		RallyAgent.Reset();
		for (TActorIterator<ABDPalace> It(World); It; ++It)
		{
			if (ABDAgent* Agent = It->GetAgent())
			{
				RallyAgent = Agent;
			}
		}
		if (!RallyAgent.IsValid())
		{
			// The palace went with the sale earlier on: one more, for the Mito it brings.
			Match->AddPublicMoney(UBDGameBalanceSettings::Get().PalaceCost * 2, TEXT("BD.Test.Regression"));
			FBDCellCoord PalaceCell;
			PlaceNear(FindPiece(EBDPieceKind::Palace), UrnCell, 12, PalaceCell);
			Placement->CancelSelection();
			for (TActorIterator<ABDPalace> It(World); It; ++It)
			{
				if (ABDAgent* Agent = It->GetAgent())
				{
					RallyAgent = Agent;
				}
			}
		}
		if (ABDAgent* Agent = RallyAgent.Get())
		{
			Agent->DebugWake();
			Agent->DebugSetPatrolRemaining(100000.0f);
		}
		RallyA = Candidate;
		TrackedState = EBDRallyState::Walking;
		TrackedSince = RallyStepStart = World->GetTimeSeconds();
		MeasuredTell = MeasuredSpeech = MeasuredRise = -1.0f;
		bChasedBeforeSpeech = bChasedDuringSpeech = bSpeechChecked = bCrowdChecked = false;
		SpeechTicks = 0;
		RaisedBefore = Waves->GetMatchTotals().CreepsRaised;
		SpawnsLeftBefore = Waves->GetWaveSpawnsRemaining();
		StepTicks = 0;
		break;
	}

	case 12:
	{
		// His rally watched frame by frame: Tell -> Speaking -> Spawning -> Walking.
		ABDCandidate* Candidate = RallyA.Get();
		UBDRallyComponent* Rally = Candidate != nullptr ? Candidate->GetRally() : nullptr;
		if (Rally == nullptr)
		{
			Check(TEXT("COMICIO"), TEXT("a rally goes Tell -> Speaking -> crowd -> Walking in the DA's times"), false, TEXT("he was gone before his rally was over"));
			return false;
		}
		TrackRally(*Rally);
		ABDAgent* Agent = RallyAgent.Get();
		UBDCreepSoundSubsystem* Sounds = UBDCreepSoundSubsystem::Get(World);
		const bool bAfterHim = Agent != nullptr && Agent->DebugFindCandidate() == Candidate
			&& (Agent->GetState() == EBDAgentState::Chasing || Agent->GetState() == EBDAgentState::Shooting);
		const FBDRallyParams& Params = Rally->GetParams();

		switch (Rally->GetState())
		{
		case EBDRallyState::Walking:
		case EBDRallyState::Tell:
			bChasedBeforeSpeech |= bAfterHim && !bCrowdChecked;
			break;
		case EBDRallyState::Speaking:
			// The Mito drops him from his second frame of the speech on.
			if (++SpeechTicks > 2)
			{
				bChasedDuringSpeech |= Agent != nullptr && (Agent->GetState() == EBDAgentState::Chasing || Agent->DebugFindCandidate() == Candidate);
			}
			if (!bSpeechChecked)
			{
				bSpeechChecked = true;
				const float HealthBefore = Candidate->GetCurrentHealth();
				const float WastedBefore = Waves->GetMatchTotals().DamageWasted;
				Candidate->ApplyDamage(1000.0f, nullptr);
				const bool bNull = FMath::IsNearlyEqual(Candidate->GetCurrentHealth(), HealthBefore) && Waves->GetMatchTotals().DamageWasted > WastedBefore;
				int32 Defenders = 0, Shooters = 0, Aimers = 0;
				for (TActorIterator<ABDTowerBase> It(World); It; ++It)
				{
					++Defenders;
					Shooters += It->IsA<ABDShooter>() ? 1 : 0;
					Aimers += It->DebugCouldTarget(Candidate) ? 1 : 0;
				}
				const bool bMito = Agent != nullptr && (Agent->DebugCouldTarget(Candidate) || Agent->DebugFindCandidate() == Candidate);
				Check(TEXT("COMICIO"), TEXT("while he speaks nothing lands - the hit is lost as null votes - and no tower, no Nicole and no Mito can aim at him"),
					bNull && Candidate->IsUntouchable() && Aimers == 0 && !bMito && Defenders > 0,
					FString::Printf(TEXT("health %.0f -> %.0f, wasted +%.0f; %d of %d defender(s) (%d shooter(s)) could aim, Mito %s"),
						HealthBefore, Candidate->GetCurrentHealth(), Waves->GetMatchTotals().DamageWasted - WastedBefore, Aimers, Defenders, Shooters,
						Agent == nullptr ? TEXT("not on the board") : (bMito ? TEXT("SEES HIM") : TEXT("does not see him"))));

				const UBDPalaceData* PalaceData = LoadObject<UBDPalaceData>(nullptr, TEXT("/Game/BD/Data/DA_PalaceData.DA_PalaceData"));
				const int32 RefusalsBefore = Sounds != nullptr ? Sounds->GetFloorRefusals() : 0;
				const bool bKickSaid = Sounds != nullptr && Agent != nullptr && PalaceData != nullptr && Sounds->SayAlways(*Agent, PalaceData->AgentKickSound);
				Check(TEXT("FALA"), TEXT("while he speaks the floor is his: one sentence at most, and a line always said (the Mito's kick) is refused"),
					Sounds != nullptr && Sounds->GetFloorHolder() == Candidate && Sounds->GetSpeechBudget() == 1 && !bKickSaid && Sounds->GetFloorRefusals() > RefusalsBefore,
					FString::Printf(TEXT("floor %s, budget %d, kick line %s, %d refusal(s)"), Sounds != nullptr ? *GetNameSafe(Sounds->GetFloorHolder()) : TEXT("-"),
						Sounds != nullptr ? Sounds->GetSpeechBudget() : 0, bKickSaid ? TEXT("SAID") : TEXT("refused"),
						Sounds != nullptr ? Sounds->GetFloorRefusals() - RefusalsBefore : 0));
			}
			break;
		case EBDRallyState::Spawning:
			if (!bCrowdChecked)
			{
				bCrowdChecked = true;
				const TArray<TWeakObjectPtr<ABDEnemyBase>> Crowd = Rally->GetLastCrowd();
				FString CrowdMeasured;
				const bool bSound = IsCrowdSound(Crowd, CrowdMeasured);
				const bool bRising = !Crowd.ContainsByPredicate([](const TWeakObjectPtr<ABDEnemyBase>& Each) { return !Each.IsValid() || !Each->IsRising(); });
				Check(TEXT("COMICIO"), TEXT("at the end of the speech RallyCount militants rise around him: one a cell, on free ground leading to the urn, routed, out of the ground"),
					Rally->GetLastRaised() == Params.RallyCount && Crowd.Num() == Params.RallyCount && bSound && bRising,
					FString::Printf(TEXT("%d of %d raised, %s, %s"), Rally->GetLastRaised(), Params.RallyCount, *CrowdMeasured, bRising ? TEXT("rising") : TEXT("NOT RISING")));
				const int32 Raised = Waves->GetMatchTotals().CreepsRaised - RaisedBefore;
				Check(TEXT("COMICIO"), TEXT("the crowd is not the wave's count: what the wave still had to send is untouched, the match counts them as raised"),
					Waves->GetWaveSpawnsRemaining() == SpawnsLeftBefore && Raised == Params.RallyCount,
					FString::Printf(TEXT("wave still to send %d -> %d, %d raised"), SpawnsLeftBefore, Waves->GetWaveSpawnsRemaining(), Raised));

				// They vote like anyone: blue when one falls, red when one reaches the urn.
				ABDEnemyBase* Felled = Crowd.Num() > 1 ? Crowd[0].Get() : nullptr;
				ABDEnemyBase* Leaker = Crowd.Num() > 1 ? Crowd[1].Get() : nullptr;
				const int32 BlueBefore = Match->GetVotesBlue();
				const int32 RedBefore = Match->GetVotesRed();
				if (Felled != nullptr) { Felled->ApplyDamage(Felled->GetMaxHealth() * 10.0f, nullptr); }
				if (Leaker != nullptr) { Leaker->DebugArrive(); }
				Check(TEXT("COMICIO"), TEXT("a militant of the crowd votes like any other: blue when it falls, red at the urn"),
					Felled != nullptr && Leaker != nullptr && Match->GetVotesBlue() > BlueBefore && Match->GetVotesRed() > RedBefore,
					FString::Printf(TEXT("blue %d -> %d, red %d -> %d"), BlueBefore, Match->GetVotesBlue(), RedBefore, Match->GetVotesRed()));
			}
			break;
		}

		if (bCrowdChecked && Rally->GetState() == EBDRallyState::Walking)
		{
			const float Rise = UBDGameBalanceSettings::Get().RallyRiseSeconds;
			const float Jitter = Params.RallyEvery * Params.RallyEveryJitter;
			Check(TEXT("COMICIO"), TEXT("Tell -> Speaking -> crowd -> Walking in the DA's times, the next rally RallyEvery away within its draw"),
				FMath::IsNearlyEqual(MeasuredTell, Params.RallyTellSeconds, 0.1f) && FMath::IsNearlyEqual(MeasuredSpeech, Rally->GetSpeechSeconds(), 0.1f)
					&& Rally->GetSpeechSeconds() >= Params.RallyMinDuration && FMath::IsNearlyEqual(MeasuredRise, Rise, 0.1f)
					&& Rally->GetNextRallyIn() >= Params.RallyEvery - Jitter - 0.1f && Rally->GetNextRallyIn() <= Params.RallyEvery + Jitter + 0.1f,
				FString::Printf(TEXT("tell %.2f/%.2fs, speech %.2f/%.2fs, rise %.2f/%.2fs, next in %.1fs (%.1f +- %.1f)"),
					MeasuredTell, Params.RallyTellSeconds, MeasuredSpeech, Rally->GetSpeechSeconds(), MeasuredRise, Rise,
					Rally->GetNextRallyIn(), Params.RallyEvery, Jitter));
			const int32 Budget = Sounds != nullptr ? Sounds->GetSpeechBudget() : 0;
			Check(TEXT("FALA"), TEXT("once he is done the floor is free and the budget back to 2"),
				Sounds != nullptr && Sounds->GetFloorHolder() == nullptr && Budget == 2 && Budget == UBDCreepSoundSettings::Get().SpeechMaxConcurrent,
				FString::Printf(TEXT("floor %s, budget %d (settings %d)"), Sounds != nullptr ? *GetNameSafe(Sounds->GetFloorHolder()) : TEXT("-"),
					Budget, UBDCreepSoundSettings::Get().SpeechMaxConcurrent));
			StepTicks = 0;
			break;
		}
		if (World->GetTimeSeconds() - RallyStepStart > 30.0)
		{
			Check(TEXT("COMICIO"), TEXT("a rally goes Tell -> Speaking -> crowd -> Walking in the DA's times"), false,
				FString::Printf(TEXT("stuck in %s after %.0fs"), UBDRallyComponent::StateName(Rally->GetState()), World->GetTimeSeconds() - RallyStepStart));
			return false;
		}
		return true;
	}

	case 13:
	{
		// The Mito, once the speech is over: after him again, whatever the distance.
		ABDCandidate* Candidate = RallyA.Get();
		ABDAgent* Agent = RallyAgent.Get();
		const bool bAfterHim = Agent != nullptr && Candidate != nullptr && Agent->DebugFindCandidate() == Candidate
			&& (Agent->GetState() == EBDAgentState::Chasing || Agent->GetState() == EBDAgentState::Shooting);
		if (!bAfterHim && Agent != nullptr && ++StepTicks <= MaxWaitTicks)
		{
			return true;
		}
		Check(TEXT("COMICIO"), TEXT("the Mito runs after him, leaves him for the patrol while he speaks, and runs after him again once he is done"),
			Agent != nullptr && bChasedBeforeSpeech && !bChasedDuringSpeech && bAfterHim,
			FString::Printf(TEXT("before the speech %s, during %s, after %s; %s"), bChasedBeforeSpeech ? TEXT("after him") : TEXT("NOT after him"),
				bChasedDuringSpeech ? TEXT("STILL AFTER HIM") : TEXT("patrolling"), bAfterHim ? TEXT("after him") : TEXT("NOT after him"),
				Agent != nullptr ? *Agent->Describe() : TEXT("no Agent on the board")));
		break;
	}

	case 14:
	{
		// Two candidates due at the same moment: one rally at a time.
		ABDCandidate* A = RallyA.Get();
		ABDCandidate* B = Candidates->SpawnCandidate(TEXT("BD.Test.Regression"));
		if (A == nullptr || B == nullptr || A->GetRally() == nullptr || B->GetRally() == nullptr)
		{
			Check(TEXT("COMICIO"), TEXT("two candidates due together rally one after the other"), false, TEXT("two candidates could not be had"));
			return false;
		}
		FBDRallyParams Short = A->GetRally()->GetParams();
		Short.RallyFirstDelay = 0.0f;
		Short.RallyTellSeconds = 0.2f;
		Short.RallyDuration = 0.3f;
		Short.RallyMinDuration = 0.3f;
		Short.RallyCount = 0;
		A->GetRally()->Setup(Short, /*bInEnabled*/ true, 1);
		B->GetRally()->Setup(Short, /*bInEnabled*/ true, 2);
		RallyB = B;
		RallyStartsA = A->GetRally()->GetRalliesStarted();
		bBothRallying = false;
		RallyStepStart = World->GetTimeSeconds();
		break;
	}

	case 15:
	{
		const UBDRallyComponent* RA = RallyA.IsValid() ? RallyA->GetRally() : nullptr;
		const UBDRallyComponent* RB = RallyB.IsValid() ? RallyB->GetRally() : nullptr;
		if (RA == nullptr || RB == nullptr)
		{
			Check(TEXT("COMICIO"), TEXT("two candidates due together rally one after the other"), false, TEXT("one of them was gone"));
			return false;
		}
		const bool bAIn = RA->GetState() != EBDRallyState::Walking;
		const bool bBIn = RB->GetState() != EBDRallyState::Walking;
		bBothRallying |= bAIn && bBIn;
		if (RA->GetRalliesStarted() > RallyStartsA && RB->GetRalliesStarted() >= 1 && !bAIn && !bBIn)
		{
			Check(TEXT("COMICIO"), TEXT("two candidates due at the same moment rally one after the other: the second waits for the first to finish before his warning"),
				!bBothRallying, FString::Printf(TEXT("%d and %d rally(ies) each, %s"), RA->GetRalliesStarted() - RallyStartsA, RB->GetRalliesStarted(),
					bBothRallying ? TEXT("BOTH AT ONCE at some point") : TEXT("never both at once")));
			break;
		}
		if (World->GetTimeSeconds() - RallyStepStart > 20.0)
		{
			Check(TEXT("COMICIO"), TEXT("two candidates due together rally one after the other"), false,
				FString::Printf(TEXT("A %s (%d), B %s (%d) after 20 s"), UBDRallyComponent::StateName(RA->GetState()), RA->GetRalliesStarted() - RallyStartsA,
					UBDRallyComponent::StateName(RB->GetState()), RB->GetRalliesStarted()));
			return false;
		}
		return true;
	}

	case 16:
	{
		// One of the fallen come back does not rally, with the settings as they ship.
		Waves->DespawnAll();
		if (Candidates->GetFallenCount() == 0)
		{
			if (ABDCandidate* Felled = Candidates->SpawnCandidate(TEXT("BD.Test.Regression")))
			{
				Felled->ApplyDamage(Felled->GetMaxHealth() * 10.0f, nullptr);
			}
		}
		Candidates->BeginReturn(TEXT("BD.Test.Regression"));
		StepTicks = 0;
		break;
	}

	case 17:
	{
		TArray<ABDCandidate*> Living;
		Candidates->GetLivingCandidates(Living);
		ABDCandidate* Returning = nullptr;
		for (ABDCandidate* Each : Living)
		{
			if (Each != nullptr && Each->bReturning) { Returning = Each; }
		}
		if (Returning == nullptr)
		{
			if (++StepTicks > MaxWaitTicks)
			{
				Check(TEXT("COMICIO"), TEXT("a fallen candidate come back does not rally (bRallyOnReturn off)"), false, TEXT("no candidate came back"));
				return false;
			}
			return true;
		}
		FString Why;
		const bool bForced = Returning->GetRally() != nullptr && Returning->GetRally()->ForceRally(Why);
		Check(TEXT("COMICIO"), TEXT("a fallen candidate come back does not rally (bRallyOnReturn off), not even when forced"),
			!UBDGameBalanceSettings::Get().bRallyOnReturn && Returning->GetRally() != nullptr && !Returning->GetRally()->IsEnabled() && !bForced,
			FString::Printf(TEXT("candidate %d returning: rallies %s, forced %s"), Returning->Ordinal,
				Returning->GetRally() != nullptr && Returning->GetRally()->IsEnabled() ? TEXT("ON") : TEXT("off"),
				bForced ? TEXT("ACCEPTED") : *FString::Printf(TEXT("refused (%s)"), *Why)));
		break;
	}

	case 18:
	{
		// Forty in a narrow corridor: his own route, a cell either side, every other free cell
		// turned to scenery, and a crowd of forty at once.
		Waves->DespawnAll();
		Candidates->ResetForNewMatch();
		ABDCandidate* Candidate = Candidates->SpawnCandidate(TEXT("BD.Test.Regression"));
		if (Candidate == nullptr || Candidate->GetRally() == nullptr)
		{
			Check(TEXT("COMICIO"), TEXT("40 militants rise in a narrow corridor without two on one cell"), false, TEXT("no candidate could be sent"));
			return false;
		}
		Corridor.Reset();
		for (const FBDCellCoord& Cell : Candidate->GetPath())
		{
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 DX = -1; DX <= 1; ++DX)
				{
					Corridor.Add(FBDCellCoord(Cell.X + DX, Cell.Y + DY));
				}
			}
		}
		int32 Walled = 0;
		WalledCells.Reset();
		for (int32 Y = 0; Y < Grid->GetSizeY(); ++Y)
		{
			for (int32 X = 0; X < Grid->GetSizeX(); ++X)
			{
				const FBDCellCoord Cell(X, Y);
				if (!Corridor.Contains(Cell) && Grid->GetCellState(Cell) == EBDCellState::Free)
				{
					if (Grid->SetCellState(Cell, EBDCellState::Blocked))
					{
						++Walled;
						WalledCells.Add(Cell);
					}
				}
			}
		}
		FBDRallyParams Forty = Candidate->GetRally()->GetParams();
		Forty.RallyFirstDelay = 0.0f;
		Forty.RallyTellSeconds = 0.1f;
		Forty.RallyDuration = 0.1f;
		Forty.RallyMinDuration = 0.1f;
		Forty.RallyCount = 40;
		Candidate->GetRally()->Setup(Forty, /*bInEnabled*/ true, 40);
		UE_LOG(LogBDDebug, Log, TEXT("REGRESSION rally corridor: route of %d cell(s), %d cell(s) kept around it, %d walled off."),
			Candidate->GetPath().Num(), Corridor.Num(), Walled);
		RallyA = Candidate;
		RallyStepStart = World->GetTimeSeconds();
		break;
	}

	case 19:
	{
		const UBDRallyComponent* Rally = RallyA.IsValid() ? RallyA->GetRally() : nullptr;
		if (Rally != nullptr && Rally->GetLastAsked() == 0 && World->GetTimeSeconds() - RallyStepStart <= 20.0)
		{
			return true;
		}
		FString CrowdMeasured;
		const bool bSound = Rally != nullptr && IsCrowdSound(Rally->GetLastCrowd(), CrowdMeasured);
		int32 Outside = 0;
		if (Rally != nullptr)
		{
			for (const TWeakObjectPtr<ABDEnemyBase>& Each : Rally->GetLastCrowd())
			{
				Outside += Each.IsValid() && Each->GetPath().Num() > 0 && !Corridor.Contains(Each->GetPath()[0])
					&& Grid->GetCellState(Each->GetPath()[0]) != EBDCellState::Spawn ? 1 : 0;
			}
		}
		Check(TEXT("COMICIO"), TEXT("40 militants rise in a narrow corridor (his route, a cell either side) without two on one cell"),
			Rally != nullptr && Rally->GetLastRaised() == 40 && bSound && Outside == 0,
			Rally != nullptr ? FString::Printf(TEXT("%d of %d raised, %s, %d outside the corridor"), Rally->GetLastRaised(), Rally->GetLastAsked(), *CrowdMeasured, Outside)
				: FString(TEXT("the candidate was gone")));
		break;
	}

	case 20:
	{
		// The hordes. The board given back, nothing on it, nobody fallen.
		for (const FBDCellCoord& Cell : WalledCells)
		{
			Grid->SetCellState(Cell, EBDCellState::Free);
		}
		WalledCells.Reset();
		Waves->DespawnAll();
		Candidates->ResetForNewMatch();
		Waves->SetUnlockedOverride(0);
		const UBDEnemyData* Donkey = UBDWaveSettings::Get().ResolveWaveEnemy();
		const UBDEnemyData* BlackBloc = Cast<UBDEnemyData>(BDCast::LoadData(EBDCastKind::Horde, 2));

		const auto CountSpecials = [Donkey](const TArray<const UBDEnemyData*>& Plan)
		{
			return Plan.FilterByPredicate([Donkey](const UBDEnemyData* Kind) { return Kind != Donkey; }).Num();
		};
		{
			int32 Specials = 0;
			for (const int32 Wave : { 1, 6, 50, 100 })
			{
				TArray<const UBDEnemyData*> Plan;
				Waves->PlanWaveKinds(Wave, 60, Waves->GetUnlockedKinds(), Donkey, Plan);
				Specials += CountSpecials(Plan);
			}
			Check(TEXT("HORDA"), TEXT("with no candidate killed only the donkeys come out, on any wave"),
				Waves->GetUnlockedKinds() == 1 && Specials == 0,
				FString::Printf(TEXT("%d kind(s) unlocked, %d special(s) planned over waves 1, 6, 50 and 100"), Waves->GetUnlockedKinds(), Specials));
		}

		Check(TEXT("HORDA"), TEXT("DA_Horda_02 is the Black Bloc: the donkey's body in black, 1.4x speed, 2x health, stops every 4-8 s for 1.5 s after 3 s, its own kind and icon"),
			BlackBloc != nullptr && BlackBloc->HasBody() && FMath::IsNearlyEqual(BlackBloc->SpeedScale, 1.4f) && FMath::IsNearlyEqual(BlackBloc->HealthScale, 2.0f)
				&& BlackBloc->bStops && FMath::IsNearlyEqual(BlackBloc->StopFirstDelay, 3.0f) && FMath::IsNearlyEqual(BlackBloc->StopIntervalMin, 4.0f)
				&& FMath::IsNearlyEqual(BlackBloc->StopIntervalMax, 8.0f) && FMath::IsNearlyEqual(BlackBloc->StopDuration, 1.5f)
				&& Donkey != nullptr && BlackBloc->GetKillType() != Donkey->GetKillType() && !BlackBloc->KillIcon.IsNull() && BlackBloc->KillIcon != Donkey->KillIcon
				&& BlackBloc->DisplayName.ToString() == TEXT("Black Bloc") && BlackBloc->MeshMaterial.GetAssetName() == TEXT("MI_Jumento_BlackBloc"),
			BlackBloc == nullptr ? FString(TEXT("no DA_Horda_02")) : FString::Printf(TEXT("'%s', body %s, x%.1f speed, x%.1f health, stops %s %.0f/%.0f-%.0f/%.1f s, kind %s, icon %s, material %s"),
				*BlackBloc->DisplayName.ToString(), BlackBloc->HasBody() ? TEXT("yes") : TEXT("NO"), BlackBloc->SpeedScale, BlackBloc->HealthScale,
				BlackBloc->bStops ? TEXT("on") : TEXT("OFF"), BlackBloc->StopFirstDelay, BlackBloc->StopIntervalMin, BlackBloc->StopIntervalMax, BlackBloc->StopDuration,
				*BlackBloc->GetKillType().ToString(), *BlackBloc->KillIcon.GetAssetName(), *BlackBloc->MeshMaterial.GetAssetName()));

		// Wave 5 dealt with nobody killed; its candidate falls during it.
		Match->DebugSetWave(4);
		Match->DebugForcePhase(EBDMatchPhase::WaveActive);
		TArray<const UBDEnemyData*> WaveFive;
		for (const UBDEnemyData* Kind : Waves->GetWavePlan()) { WaveFive.Add(Kind); }
		if (ABDCandidate* Felled = Candidates->SpawnCandidate(TEXT("BD.Test.Regression")))
		{
			Felled->ApplyDamage(Felled->GetMaxHealth() * 10.0f, nullptr);
		}
		const int32 AfterKill = Waves->GetUnlockedKinds();
		const int32 FiveSpecialsAfter = Waves->GetWavePlan().FilterByPredicate([Donkey](const UBDEnemyData* Kind) { return Kind != Donkey; }).Num();
		Check(TEXT("HORDA"), TEXT("candidate 1 killed on wave 5 unlocks the Black Bloc, but wave 5 itself keeps its donkeys"),
			Match->GetCurrentWave() == 5 && WaveFive.Num() > 0 && CountSpecials(WaveFive) == 0 && FiveSpecialsAfter == 0 && AfterKill == 2
				&& Candidates->GetFallen().Num() == 1 && Candidates->GetFallen()[0].DeathWave == 5,
			FString::Printf(TEXT("wave %d: %d creep(s), %d special(s) dealt and %d after the kill; %d kind(s) unlocked, candidate fell on wave %d"),
				Match->GetCurrentWave(), WaveFive.Num(), CountSpecials(WaveFive), FiveSpecialsAfter, AfterKill,
				Candidates->GetFallen().Num() > 0 ? Candidates->GetFallen()[0].DeathWave : -1));

		// Wave 6: every tenth of its round robin a Black Bloc, the rest donkeys.
		const int32 NoticesBefore = Waves->GetNewHordeNotices();
		Waves->DespawnAll();
		Match->DebugForcePhase(EBDMatchPhase::WaveActive);
		const TArray<TObjectPtr<const UBDEnemyData>>& Plan = Waves->GetWavePlan();
		int32 Wrong = 0;
		WaveSixSpecials = 0;
		for (int32 Place = 1; Place <= Plan.Num(); ++Place)
		{
			const bool bSpecialPlace = Place % UBDWaveSettings::Get().SpecialEvery == 0;
			Wrong += (bSpecialPlace ? Plan[Place - 1] != BlackBloc : Plan[Place - 1] != Donkey) ? 1 : 0;
			WaveSixSpecials += Plan[Place - 1] == BlackBloc ? 1 : 0;
		}
		Check(TEXT("HORDA"), TEXT("on wave 6 one creep in ten is a Black Bloc - the 10th, 20th, 30th of its round robin - in place of a donkey, the total unchanged"),
			Match->GetCurrentWave() == 6 && Plan.Num() == UBDGameBalanceSettings::Get().GetCreepsPerSpawnPoint(6) * Waves->GetSpawnPointCount()
				&& WaveSixSpecials == Plan.Num() / UBDWaveSettings::Get().SpecialEvery && Wrong == 0,
			FString::Printf(TEXT("wave %d: %d creep(s), %d Black Bloc(s), %d in the wrong place"), Match->GetCurrentWave(), Plan.Num(), WaveSixSpecials, Wrong));
		Check(TEXT("HORDA"), TEXT("the first wave a kind comes out of the buses puts NOVA HORDA with its name on the HUD"),
			Waves->GetNewHordeNotices() == NoticesBefore + 1 && Waves->GetLastNewHorde() == BlackBloc,
			FString::Printf(TEXT("%d notice(s) on this wave, last %s, '%s'"), Waves->GetNewHordeNotices() - NoticesBefore,
				*GetNameSafe(Waves->GetLastNewHorde()), *BDLoc::EnemyName(BlackBloc).ToString()));

		// The draw: same wave, same seed, same crowd; a kind with no body never drawn.
		{
			TArray<const UBDEnemyData*> First, Second, Other, Bodiless;
			UBDEnemyData* Third = Cast<UBDEnemyData>(BDCast::LoadData(EBDCastKind::Horde, 3));
			Waves->PlanWaveKinds(30, 200, 4, Donkey, Bodiless);
			const int32 BodilessDrawn = Bodiless.FilterByPredicate([Donkey, BlackBloc](const UBDEnemyData* Kind) { return Kind != Donkey && Kind != BlackBloc; }).Num();
			const TSoftObjectPtr<USkeletalMesh> ThirdBody = Third != nullptr ? Third->SkeletalMesh : TSoftObjectPtr<USkeletalMesh>();
			if (Third != nullptr && BlackBloc != nullptr)
			{
				// For the length of the check only, the third kind borrows a body.
				Third->SkeletalMesh = BlackBloc->SkeletalMesh;
			}
			Waves->PlanWaveKinds(30, 200, 3, Donkey, First);
			Waves->PlanWaveKinds(30, 200, 3, Donkey, Second);
			Waves->PlanWaveKinds(31, 200, 3, Donkey, Other);
			if (Third != nullptr)
			{
				Third->SkeletalMesh = ThirdBody;
			}
			const int32 Twos = First.FilterByPredicate([BlackBloc](const UBDEnemyData* Kind) { return Kind == BlackBloc; }).Num();
			const int32 Threes = Third != nullptr ? First.FilterByPredicate([Third](const UBDEnemyData* Kind) { return Kind == Third; }).Num() : 0;
			const int32 Others = First.FilterByPredicate([Donkey, BlackBloc, Third](const UBDEnemyData* Kind) { return Kind != Donkey && Kind != BlackBloc && Kind != Third; }).Num();
			Check(TEXT("HORDA"), TEXT("with two specials unlocked the draw is among them only, by the seed: the same wave draws the same crowd"),
				Third != nullptr && Twos > 0 && Threes > 0 && Others == 0 && Twos + Threes == 20 && First == Second && First != Other,
				FString::Printf(TEXT("wave 30: %d Black Bloc, %d Horda_03, %d other; again %s; wave 31 %s"), Twos, Threes, Others,
					First == Second ? TEXT("the same") : TEXT("DIFFERENT"), First != Other ? TEXT("another") : TEXT("THE SAME")));
			Check(TEXT("HORDA"), TEXT("an unlocked kind with no body in its DA is never drawn: the donkey stays in its place"),
				BodilessDrawn == 0 && Bodiless.FilterByPredicate([BlackBloc](const UBDEnemyData* Kind) { return Kind == BlackBloc; }).Num() == 20,
				FString::Printf(TEXT("4 unlocked (03 and 04 without a body): %d of them drawn, %d Black Bloc"), BodilessDrawn,
					Bodiless.FilterByPredicate([BlackBloc](const UBDEnemyData* Kind) { return Kind == BlackBloc; }).Num()));
		}

		// A load with two scheduled candidates fallen: three kinds, from the records alone.
		{
			const FBDCandidateSaveState Before = Candidates->CaptureState();
			FBDCandidateSaveState Loaded = Before;
			FBDCandidateRecord Second = Loaded.Fallen.Num() > 0 ? Loaded.Fallen[0] : FBDCandidateRecord();
			Second.Ordinal = 2;
			Loaded.Sent.Add(Second);
			Loaded.Fallen.Add(Second);
			Candidates->RestoreState(Loaded);
			const int32 OnLoad = Waves->GetUnlockedKinds();
			Candidates->RestoreState(Before);
			Check(TEXT("HORDA"), TEXT("a load with two scheduled candidates fallen has three kinds unlocked, with nothing more in the save"),
				OnLoad == 3 && Waves->GetUnlockedKinds() == 2, FString::Printf(TEXT("%d kind(s) on that load, %d back on this match"), OnLoad, Waves->GetUnlockedKinds()));
		}

		Match->SetGameSpeed(4.0f);
		RallyStepStart = World->GetTimeSeconds();
		break;
	}

	case 21:
	{
		// Wave 6 sends all of it: what came out of the buses is the plan, place by place.
		const int32 Planned = Waves->GetWavePlan().Num();
		if (Waves->GetWaveSent().Num() < Planned && Waves->GetWaveSpawnsRemaining() > 0 && World->GetTimeSeconds() - RallyStepStart <= 120.0)
		{
			return true;
		}
		int32 Mismatch = 0;
		int32 BlackBlocsOut = 0;
		const UBDEnemyData* BlackBloc = Cast<UBDEnemyData>(BDCast::LoadData(EBDCastKind::Horde, 2));
		for (int32 Index = 0; Index < Waves->GetWaveSent().Num(); ++Index)
		{
			Mismatch += Waves->GetWavePlan().IsValidIndex(Index) && Waves->GetWavePlan()[Index] == Waves->GetWaveSent()[Index] ? 0 : 1;
			BlackBlocsOut += Waves->GetWaveSent()[Index] == BlackBloc ? 1 : 0;
		}
		Check(TEXT("HORDA"), TEXT("the buses send wave 6 exactly as planned: a Black Bloc at every tenth place of the round robin"),
			Waves->GetWaveSent().Num() == Planned && Mismatch == 0 && BlackBlocsOut == WaveSixSpecials,
			FString::Printf(TEXT("%d of %d sent, %d off the plan, %d Black Bloc(s) out"), Waves->GetWaveSent().Num(), Planned, Mismatch, BlackBlocsOut));
		Match->SetGameSpeed(1.0f);
		break;
	}

	case 22:
	{
		// One Black Bloc on the longest way to the urn, one beside it; both under cover so
		// the towers leave them be while they are watched.
		Waves->DespawnAll();
		const UBDEnemyData* BlackBloc = Cast<UBDEnemyData>(BDCast::LoadData(EBDCastKind::Horde, 2));
		const TArray<FBDSpawnPoint>& Points = Waves->GetSpawnPoints();
		const FBDSpawnPoint* Longest = nullptr;
		for (const FBDSpawnPoint& Point : Points)
		{
			if (Longest == nullptr || Point.Route.Num() > Longest->Route.Num()) { Longest = &Point; }
		}
		ABDEnemyBase* Far = Longest != nullptr && Longest->Route.Num() > 0 ? Waves->SpawnEnemyAtCell(BlackBloc, Longest->ExitCell, TEXT("BD.Test.Regression")) : nullptr;
		ABDEnemyBase* Near = nullptr;
		if (Longest != nullptr && Longest->Route.Num() >= 2)
		{
			Near = Waves->SpawnEnemyAtCell(BlackBloc, Longest->Route[Longest->Route.Num() - 2], TEXT("BD.Test.Regression"));
		}
		if (Far == nullptr || Near == nullptr)
		{
			Check(TEXT("HORDA"), TEXT("a Black Bloc walks, stops and walks on in the DA's times"), false, TEXT("no Black Bloc could be raised"));
			return false;
		}
		const double Now = World->GetTimeSeconds();
		Far->ShelterUntil(Now + 100000.0);
		Near->ShelterUntil(Now + 100000.0);
		Near->DebugSetNextStop(0.0f);

		const float Expected = BlackBloc->MaxHealth * 2.0f * Match->GetHealthScale();
		Check(TEXT("HORDA"), TEXT("a Black Bloc runs at 1.4x the donkey and takes 2x its health, scaled by the wave like any creep"),
			FMath::IsNearlyEqual(Far->GetMaxHealth(), Expected, 0.01f) && FMath::IsNearlyEqual(Far->DebugBaseMoveSpeed(), BlackBloc->MoveSpeed * 1.4f, 0.001f),
			FString::Printf(TEXT("health %.1f (expected %.1f on wave %d), pace %.2f cells/s (donkey %.2f)"), Far->GetMaxHealth(), Expected, Match->GetCurrentWave(),
				Far->DebugBaseMoveSpeed(), BlackBloc->MoveSpeed));

		StopperA = Far;
		StopperB = Near;
		StopperBorn = Now;
		FirstStopAt = FirstStopEnd = SecondStopAt = -1.0;
		bWasStopping = bStopVulnerable = bStopChecked = bReturnChecked = bNearStopped = bNearGaveUp = false;
		// The return of the fallen, to see that killing one of them unlocks nothing.
		Candidates->BeginReturn(TEXT("BD.Test.Regression"));
		RallyStepStart = Now;
		break;
	}

	case 23:
	{
		const double Now = World->GetTimeSeconds();
		ABDEnemyBase* Far = StopperA.Get();
		ABDEnemyBase* Near = StopperB.Get();

		// One of the fallen come back, killed: no new kind.
		if (!bReturnChecked)
		{
			TArray<ABDCandidate*> Living;
			Candidates->GetLivingCandidates(Living);
			for (ABDCandidate* Each : Living)
			{
				if (Each != nullptr && Each->bReturning && !bReturnChecked)
				{
					bReturnChecked = true;
					const int32 Before = Waves->GetUnlockedKinds();
					Each->ApplyDamage(Each->GetMaxHealth() * 10.0f, nullptr);
					Check(TEXT("HORDA"), TEXT("one of the fallen come back and killed again unlocks nothing"),
						Before == 2 && Waves->GetUnlockedKinds() == 2 && Candidates->GetFallen().Num() == 1,
						FString::Printf(TEXT("%d kind(s) before, %d after, %d fallen"), Before, Waves->GetUnlockedKinds(), Candidates->GetFallen().Num()));
				}
			}
		}

		// The one by the urn, every frame it is still there.
		if (Near != nullptr)
		{
			bNearStopped |= Near->GetStopsMade() > 0;
			bNearGaveUp |= Near->GetNextStopIn() < 0.0f;
		}

		if (Far != nullptr)
		{
			const bool bStopping = Far->IsStopping();
			if (bStopping && !bWasStopping)
			{
				if (FirstStopAt < 0.0) { FirstStopAt = Now; }
				else if (SecondStopAt < 0.0) { SecondStopAt = Now; }
				// Stopped he is a target like any: out from under the cover for a moment.
				if (!bStopVulnerable)
				{
					Far->Unshelter();
					int32 Aimers = 0;
					for (TActorIterator<ABDTowerBase> It(World); It; ++It) { Aimers += It->DebugCouldTarget(Far) ? 1 : 0; }
					const float Health = Far->GetCurrentHealth();
					Far->ApplyDamage(1.0f, nullptr);
					bStopVulnerable = Aimers > 0 && Far->GetCurrentHealth() < Health;
					Far->ShelterUntil(Now + 100000.0);
				}
			}
			if (!bStopping && bWasStopping && FirstStopEnd < 0.0)
			{
				FirstStopEnd = Now;
			}
			bWasStopping = bStopping;
		}

		const bool bDone = SecondStopAt >= 0.0 && bReturnChecked;
		if (!bDone && Far != nullptr && Now - RallyStepStart <= 60.0)
		{
			return true;
		}
		const UBDEnemyData* BlackBloc = Cast<UBDEnemyData>(BDCast::LoadData(EBDCastKind::Horde, 2));
		const float First = static_cast<float>(FirstStopAt - StopperBorn);
		const float Lasted = static_cast<float>(FirstStopEnd - FirstStopAt);
		const float Between = static_cast<float>(SecondStopAt - FirstStopEnd);
		Check(TEXT("HORDA"), TEXT("a Black Bloc stops after 3 s at the earliest, 1.5 s each time, every 4-8 s of walking, and can be shot while stopped"),
			BlackBloc != nullptr && FirstStopAt >= 0.0 && SecondStopAt >= 0.0 && First >= BlackBloc->StopFirstDelay - 0.05f && First <= BlackBloc->StopIntervalMax + 0.1f
				&& FMath::IsNearlyEqual(Lasted, BlackBloc->StopDuration, 0.1f) && Between >= BlackBloc->StopIntervalMin - 0.1f && Between <= BlackBloc->StopIntervalMax + 0.1f
				&& bStopVulnerable,
			FString::Printf(TEXT("first stop at %.2fs, lasted %.2fs, next after %.2fs of walking; %s while stopped%s"), First, Lasted, Between,
				bStopVulnerable ? TEXT("aimed at and hit") : TEXT("NOT A TARGET"), Far == nullptr ? TEXT(" (he was gone)") : TEXT("")));
		Check(TEXT("HORDA"), TEXT("a Black Bloc due to stop two cells from the urn runs on and stops no more"),
			bNearGaveUp && !bNearStopped,
			FString::Printf(TEXT("%s, %s"), bNearStopped ? TEXT("STOPPED") : TEXT("never stopped"), bNearGaveUp ? TEXT("its stops dropped on the last stretch") : TEXT("STOP STILL DUE")));

		// One vote, like any body.
		if (Far != nullptr)
		{
			Far->Unshelter();
			const int32 Blue = Match->GetVotesBlue();
			Far->ApplyDamage(Far->GetMaxHealth() * 10.0f, nullptr);
			Check(TEXT("HORDA"), TEXT("a Black Bloc killed is one blue vote, like a donkey"), Match->GetVotesBlue() == Blue + 1,
				FString::Printf(TEXT("blue %d -> %d"), Blue, Match->GetVotesBlue()));
		}

		FString SlotMeasured;
		Check(TEXT("SAVE"), TEXT("the whole run wrote only the test slots: the player's match save and progress are untouched"), UBDMatchSave::IsTestSlot() && IsPlayerSlotUntouched(SlotMeasured),
			SlotMeasured);

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

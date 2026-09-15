// Brazil Defense. Putting the urn on the board: the Goal cell follows it.

#include "Objective/BDObjectiveSubsystem.h"

#include "BDLog.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Grid/BDGridDebug.h"
#include "Grid/BDGridSubsystem.h"
#include "Objective/BDObjective.h"
#include "Objective/BDObjectiveSettings.h"
#include "Path/BDPathfinder.h"
#include "Placement/BDPlacementSettings.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/ReverbEffect.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "UI/BDUISettings.h"

void UBDObjectiveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UBDGridSubsystem>();
	Collection.InitializeDependency<UBDPathfinder>();
}

UBDObjectiveSubsystem* UBDObjectiveSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;

	return World != nullptr ? World->GetSubsystem<UBDObjectiveSubsystem>() : nullptr;
}

UBDGridSubsystem* UBDObjectiveSubsystem::GetGrid() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
}

const UBDPathfinder* UBDObjectiveSubsystem::GetPathfinder() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDPathfinder>() : nullptr;
}

ABDObjective* UBDObjectiveSubsystem::GetObjective() const
{
	if (!Objective.IsValid())
	{
		Objective = ABDObjective::Get(GetWorld());
	}

	return Objective.Get();
}

FString UBDObjectiveSubsystem::DescribeRefusal(const EBDObjectiveRefusal Refusal)
{
	return StaticEnum<EBDObjectiveRefusal>()->GetNameStringByValue(static_cast<int64>(Refusal));
}

EBDObjectiveRefusal UBDObjectiveSubsystem::EvaluateCell(const FBDCellCoord& Coord) const
{
	const UBDGridSubsystem* Grid = GetGrid();
	if (Grid == nullptr || !Grid->IsValidCoord(Coord))
	{
		return EBDObjectiveRefusal::OffGrid;
	}

	if (!UBDObjectiveSettings::Get().IsInZone(Coord))
	{
		return EBDObjectiveRefusal::OutOfZone;
	}

	// Its own cell is fine again: moving the urn onto where it already stands is a no-op,
	// not a collision.
	const EBDCellState State = Grid->GetCellState(Coord);
	const bool bOwnCell = bPlaced && Coord == GoalCell && State == EBDCellState::Goal;
	if (!bOwnCell && !Grid->IsBuildable(Coord))
	{
		return EBDObjectiveRefusal::CellTaken;
	}

	const UBDPathfinder* Pathfinder = GetPathfinder();
	if (Pathfinder == nullptr || !Pathfinder->CanEverySpawnReach(Grid, Coord))
	{
		return EBDObjectiveRefusal::Unreachable;
	}

	return EBDObjectiveRefusal::None;
}

float UBDObjectiveSubsystem::ResolveGroundZ(const FVector& Point) const
{
	const UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = GetGrid();
	const float PlaneZ = Grid != nullptr ? Grid->GetOrigin().Z : Point.Z;
	if (World == nullptr)
	{
		return PlaneZ;
	}

	const UBDPlacementSettings& Settings = UBDPlacementSettings::Get();
	const FVector Start(Point.X, Point.Y, PlaneZ + Settings.GroundTraceDistance);
	const FVector End(Point.X, Point.Y, PlaneZ - Settings.GroundTraceDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BDObjectiveGround), /*bTraceComplex*/ false);
	if (const ABDObjective* Urn = GetObjective())
	{
		// The urn must not be measured against its own roof when it is only moving.
		Params.AddIgnoredActor(Urn);
	}

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, Settings.GroundTraceChannel, Params))
	{
		return Hit.ImpactPoint.Z;
	}

	return PlaneZ;
}

bool UBDObjectiveSubsystem::PlaceObjective(const FBDCellCoord& Coord, UClass* ActorClass, UStaticMesh* Mesh, EBDObjectiveRefusal& OutRefusal)
{
	OutRefusal = EvaluateCell(Coord);
	if (OutRefusal != EBDObjectiveRefusal::None)
	{
		UE_LOG(LogBDGrid, Warning, TEXT("Objective refused at %s: %s."), *Coord.ToString(), *DescribeRefusal(OutRefusal));
		return false;
	}

	UWorld* World = GetWorld();
	UBDGridSubsystem* Grid = GetGrid();
	check(World != nullptr && Grid != nullptr);

	// The actor first: a failed spawn must not leave a Goal with no urn on it.
	ABDObjective* Urn = GetObjective();
	if (Urn == nullptr)
	{
		UClass* SpawnClass = ActorClass != nullptr && ActorClass->IsChildOf<ABDObjective>()
			? ActorClass
			: ABDObjective::StaticClass();
		if (ActorClass != nullptr && SpawnClass != ActorClass)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Objective actor class %s is not a BD Objective; spawning the base class instead."),
				*ActorClass->GetName());
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Urn = World->SpawnActor<ABDObjective>(SpawnClass, FTransform::Identity, SpawnParams);
		if (Urn == nullptr)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Failed to spawn the objective actor %s."), *SpawnClass->GetName());
			OutRefusal = EBDObjectiveRefusal::OffGrid;
			return false;
		}

		Objective = Urn;

		if (Mesh != nullptr && Urn->GetMesh() != nullptr && Urn->GetMesh()->GetStaticMesh() == nullptr)
		{
			Urn->GetMesh()->SetStaticMesh(Mesh);
		}
	}

	FVector Location = Grid->CellToWorld(Coord);
	Location.Z = ResolveGroundZ(Location);
	Urn->SetActorLocation(Location);

	// Then the grid. The previous Goal cells go back to Free whatever wrote them, the
	// authored layout included: the Goal is wherever the urn is, nowhere else.
	TArray<FBDCellCoord> PreviousGoals;
	UBDPathfinder::GatherCellsWithState(*Grid, EBDCellState::Goal, PreviousGoals);
	for (const FBDCellCoord& Previous : PreviousGoals)
	{
		if (Previous != Coord)
		{
			Grid->SetCellState(Previous, EBDCellState::Free);
		}
	}
	Grid->SetCellState(Coord, EBDCellState::Goal);

	GoalCell = Coord;
	bPlaced = true;

	UE_LOG(LogBDGrid, Log, TEXT("Objective placed at %s (%s), %d previous goal cell(s) freed."),
		*Coord.ToString(), *Location.ToCompactString(), PreviousGoals.Num());
	return true;
}

void UBDObjectiveSubsystem::ClearObjective()
{
	if (!bPlaced)
	{
		return;
	}

	if (UBDGridSubsystem* Grid = GetGrid())
	{
		TArray<FBDCellCoord> Goals;
		UBDPathfinder::GatherCellsWithState(*Grid, EBDCellState::Goal, Goals);
		for (const FBDCellCoord& Goal : Goals)
		{
			Grid->SetCellState(Goal, EBDCellState::Free);
		}
	}

	bPlaced = false;
	UE_LOG(LogBDGrid, Log, TEXT("Objective cleared from %s."), *GoalCell.ToString());
}

void UBDObjectiveSubsystem::PlayVoteSound(const int32 Votes)
{
	const UBDObjectiveSettings& Settings = UBDObjectiveSettings::Get();
	UWorld* World = GetWorld();
	if (World == nullptr || Settings.VoteSound.IsNull())
	{
		return;
	}

	// Real seconds, not game seconds: at 4x a wave arrives four times as fast and the
	// beep would stack just the same.
	const double Now = FPlatformTime::Seconds();
	if (Now - LastVoteSoundTime < Settings.VoteSoundMinInterval)
	{
		return;
	}

	USoundBase* Sound = Settings.VoteSound.LoadSynchronous();
	const ABDObjective* Urn = GetObjective();
	if (Sound == nullptr || Urn == nullptr)
	{
		return;
	}
	LastVoteSoundTime = Now;

	EnsureOutdoorReverb(*World);

	// Only so many at once: past the rate limit a dense wave still stacks a few, and the
	// oldest gives way rather than the whole lot piling up.
	if (VoteConcurrency == nullptr)
	{
		VoteConcurrency = NewObject<USoundConcurrency>(this, TEXT("VoteConcurrency"));
		VoteConcurrency->Concurrency.MaxCount = FMath::Clamp(Settings.VoteSoundMaxConcurrent, 1, 16);
		VoteConcurrency->Concurrency.bLimitToOwner = false;
		VoteConcurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
	}

	// Outdoors, from the urn itself: a 3D source with a natural falloff over the board,
	// air taking the highs with distance, nothing occluding, and a share sent to the
	// open-air reverb that grows with the distance. The listener is the match camera,
	// so the radii are wide: the beep reads from the overview and comes closer with it.
	FSoundAttenuationSettings Attenuation;
	Attenuation.bAttenuate = true;
	Attenuation.bSpatialize = true;
	Attenuation.SpatializationAlgorithm = ESoundSpatializationAlgorithm::SPATIALIZATION_Default;
	Attenuation.StereoSpread = 400.0f;
	Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	Attenuation.dBAttenuationAtMax = Settings.VoteSoundAttenuationAtMax;
	Attenuation.AttenuationShapeExtents = FVector(Settings.VoteSoundInnerRadius, 0.0f, 0.0f);
	Attenuation.FalloffDistance = Settings.VoteSoundFalloffDistance;
	Attenuation.FalloffMode = ENaturalSoundFalloffMode::Hold;
	Attenuation.bEnableListenerFocus = false;
	Attenuation.bAttenuateWithLPF = true;
	Attenuation.bEnableLogFrequencyScaling = true;
	Attenuation.LPFRadiusMin = Settings.VoteSoundInnerRadius;
	Attenuation.LPFRadiusMax = Settings.VoteSoundInnerRadius + Settings.VoteSoundFalloffDistance;
	Attenuation.LPFFrequencyAtMax = 3000.0f;
	Attenuation.bEnableOcclusion = false;
	Attenuation.bEnableReverbSend = true;
	Attenuation.ReverbSendMethod = EReverbSendMethod::Linear;
	Attenuation.ReverbWetLevelMin = Settings.VoteReverbWetNear;
	Attenuation.ReverbWetLevelMax = Settings.VoteReverbWetFar;
	Attenuation.ReverbDistanceMin = Settings.VoteSoundInnerRadius;
	Attenuation.ReverbDistanceMax = Settings.VoteSoundInnerRadius + Settings.VoteSoundFalloffDistance;

	// A costly arrival lands heavier: lower and louder, up to the heavy mark.
	const float Weight = FMath::Clamp(static_cast<float>(Votes) / FMath::Max(1, Settings.VoteSoundHeavyVotes), 0.0f, 1.0f);
	const float Pitch = FMath::Lerp(1.0f, Settings.VoteSoundHeavyPitch, Weight);
	const float Volume = Settings.VoteSoundVolume * FMath::Lerp(1.0f, Settings.VoteSoundHeavyVolume, Weight);

	UAudioComponent* Audio = UGameplayStatics::SpawnSoundAtLocation(World, Sound, Urn->GetActorLocation(), FRotator::ZeroRotator,
		Volume, Pitch, 0.0f, nullptr, VoteConcurrency, /*bAutoDestroy*/ true);
	if (Audio == nullptr)
	{
		return;
	}

	// Through the effects class, so the options slider and mute apply to it.
	if (USoundClass* Effects = UBDUISettings::Get().EffectsSoundClass.LoadSynchronous())
	{
		Audio->SoundClassOverride = Effects;
	}
	Audio->bOverrideAttenuation = true;
	Audio->AttenuationOverrides = Attenuation;
	Audio->Play();
	OnVoteSound.Broadcast();
	UE_LOG(LogBDGrid, Verbose, TEXT("Urn beep at %s."), *Urn->GetActorLocation().ToCompactString());
}

void UBDObjectiveSubsystem::EnsureOutdoorReverb(UWorld& World)
{
	if (bReverbActivated)
	{
		return;
	}
	bReverbActivated = true;

	const UBDObjectiveSettings& Settings = UBDObjectiveSettings::Get();
	VoteReverb = Settings.VoteReverbEffect.LoadSynchronous();
	if (VoteReverb == nullptr)
	{
		// An open square in the plain air: little density and diffusion, a quiet early
		// reflection, a short late tail, the highs absorbed. Nothing of a room.
		VoteReverb = NewObject<UReverbEffect>(this, TEXT("OutdoorReverb"));
		VoteReverb->Density = 0.6f;
		VoteReverb->Diffusion = 0.4f;
		VoteReverb->Gain = 0.3f;
		VoteReverb->GainHF = 0.2f;
		VoteReverb->DecayTime = Settings.VoteReverbDecaySeconds;
		VoteReverb->DecayHFRatio = 0.5f;
		VoteReverb->ReflectionsGain = 0.08f;
		VoteReverb->ReflectionsDelay = 0.03f;
		VoteReverb->LateGain = 0.15f;
		VoteReverb->LateDelay = 0.04f;
		VoteReverb->AirAbsorptionGainHF = 0.99f;
	}

	// Pinned on the world with a low priority: an audio volume placed on the map wins.
	UGameplayStatics::ActivateReverbEffect(&World, VoteReverb, TEXT("BDOutdoor"), /*Priority*/ 0.0f, /*Volume*/ 1.0f, /*FadeTime*/ 0.5f);
	UE_LOG(LogBDGrid, Log, TEXT("Open-air reverb on: %s, %.1fs tail."), *GetNameSafe(VoteReverb), VoteReverb->DecayTime);
}

void UBDObjectiveSubsystem::DrawZone() const
{
	const UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = GetGrid();
	if (World == nullptr || Grid == nullptr || Grid->GetCellCount() <= 0)
	{
		return;
	}

	const UBDObjectiveSettings& Settings = UBDObjectiveSettings::Get();
	if (!Settings.bRestrictToZone)
	{
		return;
	}

	// Clipped to the grid, so a zone authored past the edge does not paint thin air.
	const FBDCellCoord Min(FMath::Max(0, Settings.MinX), FMath::Max(0, Settings.MinY));
	const FBDCellCoord Max(FMath::Min(Grid->GetSizeX() - 1, Settings.MaxX), FMath::Min(Grid->GetSizeY() - 1, Settings.MaxY));
	if (Min.X > Max.X || Min.Y > Max.Y)
	{
		return;
	}

	for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
	{
		for (int32 X = Min.X; X <= Max.X; ++X)
		{
			BDGridDebug::DrawCellFill(*World, *Grid, FBDCellCoord(X, Y), Settings.ZoneFillColor);
		}
	}

	BDGridDebug::DrawCellRectOutline(*World, *Grid, Min, Max, Settings.ZoneBorderColor, Settings.ZoneBorderThickness);
}

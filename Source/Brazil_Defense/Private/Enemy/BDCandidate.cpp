// Brazil Defense. The red candidate: the one creep that ends the match.

#include "Enemy/BDCandidate.h"

#include "Candidate/BDCandidateSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Match/BDGameBalanceSettings.h"

ABDCandidate::ABDCandidate()
{
	// Not a class default the wave subsystem could not override; a reminder of what this is.
	SetCanBeDamaged(true);
}

void ABDCandidate::BeginPlay()
{
	Super::BeginPlay();

	const UWorld* World = GetWorld();
	SpawnTimeSeconds = World != nullptr ? World->GetTimeSeconds() : 0.0f;
}

UBDCandidateSubsystem* ABDCandidate::GetCandidates() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDCandidateSubsystem>() : nullptr;
}

float ABDCandidate::GetBaseMoveSpeed() const
{
	// The balance owns the pace of the candidate; the data asset is the fallback.
	const float Speed = UBDGameBalanceSettings::Get().CandidateSpeed;
	return Speed > 0.0f ? Speed : Super::GetBaseMoveSpeed();
}

float ABDCandidate::GetTimeAlive() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetTimeSeconds() - SpawnTimeSeconds : 0.0f;
}

void ABDCandidate::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ApplyDebugTint();
}

void ABDCandidate::SetDebugTint(const FLinearColor& Color)
{
	PendingTint = Color;
	bTintPending = true;
	ApplyDebugTint();
}

void ABDCandidate::ApplyDebugTint()
{
	UStaticMeshComponent* Body = GetMesh();
	if (!bTintPending || Body == nullptr || Body->GetNumMaterials() == 0 || Body->GetMaterial(0) == nullptr)
	{
		return;
	}

	if (UMaterialInstanceDynamic* Dynamic = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		Dynamic->SetVectorParameterValue(TEXT("TintColor"), PendingTint);
	}
	bTintPending = false;
}

void ABDCandidate::Arrive()
{
	// The subsystem is told first: the defeat has to be declared before the wave hears of
	// an arrival, or an empty board would open the next building phase under it.
	if (UBDCandidateSubsystem* Candidates = GetCandidates())
	{
		Candidates->NotifyCandidateArrived(this);
	}

	Super::Arrive();
}

void ABDCandidate::Die()
{
	if (UBDCandidateSubsystem* Candidates = GetCandidates())
	{
		Candidates->NotifyCandidateKilled(this);
	}

	Super::Die();
}

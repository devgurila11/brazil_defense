// Brazil Defense. The red candidate: the one creep that ends the match.

#include "Enemy/BDCandidate.h"

#include "Candidate/BDCandidateSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Grid/BDGridDebug.h"
#include "Match/BDGameBalanceSettings.h"

namespace BDCandidatePrivate
{
	/** The bar hangs this far over the top of the mesh. */
	static constexpr float BarClearance = 60.0f;
	static constexpr float BarWidth = 360.0f;
	static constexpr float BarThickness = 18.0f;

	static const FColor BarBackground(40, 40, 40, 255);
	static const FColor BarForeground(255, 40, 40, 255);
}

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
	if (IsValid(this))
	{
		DrawHealthBar();
	}
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

void ABDCandidate::DrawHealthBar() const
{
	using namespace BDCandidatePrivate;

	const UWorld* World = GetWorld();
	const UStaticMeshComponent* Body = GetMesh();
	if (World == nullptr || Body == nullptr || GetMaxHealth() <= 0.0f)
	{
		return;
	}

	// Not gated by the grid debug switch: the bar is the one reading of the candidate the
	// player has until the HUD draws it, the way the urn zone paints itself.
	const FBoxSphereBounds Bounds = Body->Bounds;
	const FVector Center(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z + Bounds.BoxExtent.Z + BarClearance);
	const FVector Left = Center - FVector(0.0f, BarWidth * 0.5f, 0.0f);
	const FVector Right = Center + FVector(0.0f, BarWidth * 0.5f, 0.0f);
	const float Fraction = FMath::Clamp(GetCurrentHealth() / GetMaxHealth(), 0.0f, 1.0f);

	DrawDebugLine(World, Left, Right, BarBackground,
		BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, BarThickness);
	if (Fraction > 0.0f)
	{
		DrawDebugLine(World, Left, FMath::Lerp(Left, Right, Fraction), BarForeground,
			BDGridDebug::bPersistentLines, BDGridDebug::SingleFrameLifeTime, BDGridDebug::DepthPriority, BarThickness * 0.7f);
	}
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

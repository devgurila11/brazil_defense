// Brazil Defense. The bag of money a dead candidate drops.

#include "Bribe/BDMoneyBag.h"

#include "Bribe/BDBribeSettings.h"
#include "Bribe/BDBribeSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace BDMoneyBagPrivate
{
	/** Share of the life the bag spends shrinking away at the end. */
	static constexpr float FadeShare = 0.25f;
}

ABDMoneyBag::ABDMoneyBag()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
	RootComponent = Mesh;

	// Never in the way of a click on the board: the player is placing pieces while this
	// thing bounces.
	SetActorEnableCollision(false);
}

void ABDMoneyBag::BeginPlay()
{
	Super::BeginPlay();

	const UBDBribeSettings& Settings = UBDBribeSettings::Get();
	if (UStaticMesh* Asset = Settings.MoneyBagMesh.LoadSynchronous())
	{
		Mesh->SetStaticMesh(Asset);
		RestScale = Settings.MoneyBagScale;
		Mesh->SetRelativeScale3D(RestScale);
	}
	else
	{
		// No mesh authored yet: the bag still falls, still rings and still times the
		// counters. Only nobody sees it.
		Mesh->SetVisibility(false);
	}
}

void ABDMoneyBag::Drop(const float GroundZ)
{
	GroundHeight = GroundZ;
	VerticalSpeed = 0.0f;
	Age = 0.0f;
	Bounces = 0;
	bResting = false;
}

void ABDMoneyBag::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UBDBribeSettings& Settings = UBDBribeSettings::Get();
	Age += DeltaSeconds;

	if (!bResting)
	{
		VerticalSpeed -= Settings.Gravity * DeltaSeconds;

		FVector Location = GetActorLocation();
		Location.Z += VerticalSpeed * DeltaSeconds;

		if (Location.Z <= GroundHeight)
		{
			Location.Z = GroundHeight;
			const float Landing = FMath::Abs(VerticalSpeed);
			if (Landing > Settings.MinBounceSpeed)
			{
				VerticalSpeed = Landing * FMath::Clamp(Settings.Restitution, 0.0f, 0.95f);
				++Bounces;
				PlayCoinDrop();
			}
			else
			{
				// What is left would not lift it: it settles, with one last ring.
				VerticalSpeed = 0.0f;
				bResting = true;
				++Bounces;
				PlayCoinDrop();
			}
		}
		SetActorLocation(Location);

		// Turning while it is in the air, still once it has landed.
		if (!bResting && Settings.SpinRate > 0.0f)
		{
			AddActorLocalRotation(FRotator(0.0f, Settings.SpinRate * DeltaSeconds, 0.0f));
		}
	}

	// The last quarter of its life is spent shrinking to nothing, so it leaves rather
	// than blinking out.
	const float Life = FMath::Max(0.1f, Settings.BagSeconds);
	const float FadeStart = Life * (1.0f - BDMoneyBagPrivate::FadeShare);
	if (Age >= FadeStart && Mesh->GetStaticMesh() != nullptr)
	{
		const float Alpha = FMath::Clamp((Age - FadeStart) / FMath::Max(0.01f, Life - FadeStart), 0.0f, 1.0f);
		Mesh->SetRelativeScale3D(RestScale * (1.0f - Alpha));
	}

	if (Age >= Life)
	{
		Destroy();
	}
}

void ABDMoneyBag::PlayCoinDrop() const
{
	const UBDBribeSettings& Settings = UBDBribeSettings::Get();
	USoundBase* Sound = Settings.CoinDropSound.LoadSynchronous();
	UBDBribeSubsystem* Bribes = UBDBribeSubsystem::Get(this);
	if (Sound == nullptr || Bribes == nullptr)
	{
		return;
	}

	// Each touch is a little quieter and a little higher than the last, the way a bag
	// of coins settles: four bounces of the same sample read as a loop.
	const float Damping = FMath::Clamp(Settings.Restitution, 0.0f, 0.95f);
	const float Remaining = FMath::Pow(Damping, static_cast<float>(FMath::Max(0, Bounces - 1)));
	const float Volume = Settings.CoinDropVolume * FMath::Lerp(FMath::Clamp(Settings.CoinDropFinalVolume, 0.0f, 1.0f), 1.0f, Remaining);
	const float Pitch = FMath::Lerp(1.15f, 1.0f, Remaining);
	Bribes->PlaySoundAt(Sound, GetActorLocation(), Volume, Pitch);
}

// Brazil Defense. The twin lamp post, ready to drop on the map.

#include "Day/BDStreetLamp.h"

#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Day/BDStreetLightComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace BDStreetLampPrivate
{
	static const TCHAR* const PoleMeshPath = TEXT("/Game/BD/Meshs/Twin_Post/Twin_Spot.Twin_Spot");

	// Taken from the post fitted by hand on the Esplanada: the heads hang 1120 cm up,
	// 350 and 370 cm out along the arm, and the spots point straight down.
	static const FVector SpotALocation(350.0f, 0.0f, 1120.0f);
	static const FVector SpotBLocation(-370.0f, 0.0f, 1120.0f);
	static const FRotator SpotRotation(-90.0f, 0.0f, 0.0f);
	static constexpr float SpotCandelas = 10.0f;
	static constexpr float SpotAttenuationRadius = 2197.0f;
	static constexpr float SpotOuterConeAngle = 60.0f;

	static USpotLightComponent* MakeSpot(AActor* Owner, const FName Name, const FVector& Location, USceneComponent* Parent)
	{
		USpotLightComponent* Spot = Owner->CreateDefaultSubobject<USpotLightComponent>(Name);
		Spot->SetupAttachment(Parent);
		Spot->SetRelativeLocationAndRotation(Location, SpotRotation);
		Spot->SetMobility(EComponentMobility::Movable);
		Spot->IntensityUnits = ELightUnits::Candelas;
		Spot->Intensity = SpotCandelas;
		Spot->AttenuationRadius = SpotAttenuationRadius;
		Spot->OuterConeAngle = SpotOuterConeAngle;
		// See UBDStreetLightComponent::bLightsCastShadows, which decides it at play.
		Spot->CastShadows = false;
		return Spot;
	}
}

ABDStreetLamp::ABDStreetLamp()
{
	PrimaryActorTick.bCanEverTick = false;

	Pole = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pole"));
	RootComponent = Pole;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PoleMesh(BDStreetLampPrivate::PoleMeshPath);
	if (PoleMesh.Succeeded())
	{
		Pole->SetStaticMesh(PoleMesh.Object);
	}

	SpotA = BDStreetLampPrivate::MakeSpot(this, TEXT("SpotA"), BDStreetLampPrivate::SpotALocation, Pole);
	SpotB = BDStreetLampPrivate::MakeSpot(this, TEXT("SpotB"), BDStreetLampPrivate::SpotBLocation, Pole);

	StreetLight = CreateDefaultSubobject<UBDStreetLightComponent>(TEXT("StreetLight"));

	SetCanBeDamaged(false);
}

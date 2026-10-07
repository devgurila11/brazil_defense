// Brazil Defense. The twin lamp post, ready to drop on the map.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BDStreetLamp.generated.h"

class UBDStreetLightComponent;
class UPointLightComponent;
class USpotLightComponent;
class UStaticMeshComponent;

/**
 * The whole lamp post in one actor: the Twin_Spot mesh, a spot under each of its two
 * heads, a point light over them for the glow that fades out round the post, and the
 * street light component that hands all three to the day cycle. Drag it from
 * Place Actors or duplicate one already down, and the copy lights at dusk on its own;
 * nothing is set per post.
 *
 * The lights sit where they were first fitted by hand on the Esplanada posts. Their
 * intensity in the editor is their fully lit value; the cycle scales it from there.
 */
UCLASS(meta = (DisplayName = "BD Street Lamp"))
class BRAZIL_DEFENSE_API ABDStreetLamp : public AActor
{
	GENERATED_BODY()

public:
	ABDStreetLamp();

	UStaticMeshComponent* GetPole() const { return Pole; }
	USpotLightComponent* GetSpotA() const { return SpotA; }
	USpotLightComponent* GetSpotB() const { return SpotB; }
	UPointLightComponent* GetTopLight() const { return TopLight; }
	UBDStreetLightComponent* GetStreetLight() const { return StreetLight; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Brazil Defense|Day")
	TObjectPtr<UStaticMeshComponent> Pole;

	/** Under the head on the pole's +X arm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Brazil Defense|Day")
	TObjectPtr<USpotLightComponent> SpotA;

	/** Under the head on the pole's -X arm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Brazil Defense|Day")
	TObjectPtr<USpotLightComponent> SpotB;

	/** Over the heads: the wide, soft light that dies away round the post. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Brazil Defense|Day")
	TObjectPtr<UPointLightComponent> TopLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Brazil Defense|Day")
	TObjectPtr<UBDStreetLightComponent> StreetLight;
};

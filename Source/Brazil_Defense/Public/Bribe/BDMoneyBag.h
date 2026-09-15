// Brazil Defense. The bag of money a dead candidate drops.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BDMoneyBag.generated.h"

class UStaticMeshComponent;

/**
 * The bribe made visible: a bag that falls out of the body, bounces like a ball with the
 * speed damping out at every touch, rings the coins inside on each of them and then
 * fades away. Nothing about it is physics: a single vertical speed, the gravity of the
 * bribe settings and a restitution, because what matters is that it reads as money
 * hitting the ground, not that it rolls correctly.
 *
 * It owns no rule. The bribe is already on the ledger before the bag exists; killing
 * this actor early loses the show, never the money.
 */
UCLASS(Blueprintable, meta = (DisplayName = "BD Money Bag"))
class BRAZIL_DEFENSE_API ABDMoneyBag : public AActor
{
	GENERATED_BODY()

public:
	ABDMoneyBag();

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor interface

	/**
	 * Starts the fall. Called by UBDBribeSubsystem right after the spawn.
	 * @param GroundZ height the bag lands on: where the candidate stood.
	 */
	void Drop(float GroundZ);

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Bribe")
	UStaticMeshComponent* GetMesh() const { return Mesh; }

private:
	/** One touch of the ground: the coins inside. Quieter with every bounce. */
	void PlayCoinDrop() const;

	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense|Bribe")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** The scale the mesh was given, which the last moments shrink to nothing. */
	FVector RestScale = FVector::OneVector;

	float GroundHeight = 0.0f;
	float VerticalSpeed = 0.0f;
	float Age = 0.0f;
	int32 Bounces = 0;
	bool bResting = false;
};

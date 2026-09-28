// Brazil Defense. What is left of a killed creep: a body falling, then sinking away.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BDCreepCorpse.generated.h"

class ABDEnemyBase;
class UAnimSequenceBase;
class UBDEnemyData;
class USkeletalMeshComponentBudgeted;

/**
 * The body a killed creep leaves behind, and nothing more. The creep itself is destroyed
 * the instant it dies, so everything the game asks of a creep (the wave count, the
 * targets of the towers, the route, the votes) is settled before this exists: a corpse
 * is not an ABDEnemyBase, has no collision and is never looked for.
 *
 * It copies the creep's animated body where it stood, plays one of the falls of its data
 * once, lies still a moment, then sinks into the ground and is gone. The material of the
 * horde is opaque, so the sink is what stands in for a fade. A board full of bodies after
 * a wave cleared at once is kept in check by UBDWaveSettings::MaxCorpses: past it the
 * oldest body starts sinking right away.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "BD Creep Corpse"))
class BRAZIL_DEFENSE_API ABDCreepCorpse : public AActor
{
	GENERATED_BODY()

public:
	ABDCreepCorpse();

	/**
	 * Leaves a body where the creep stands, when its data has a fall to play and it wears
	 * an animated body. Call before the creep is destroyed. Null when nothing was left.
	 */
	static ABDCreepCorpse* SpawnFrom(const ABDEnemyBase& Creep);

	//~ Begin AActor interface
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor interface

	/** Starts sinking now, whatever the fall has got to. */
	void BeginSink();

	/** Data of the creep this body was, for the sound of its fall. */
	const UBDEnemyData* GetData() const { return Data; }

	/** Whether the body is on its way out. */
	bool IsSinking() const { return Age >= SinkStart; }

private:
	void Setup(const ABDEnemyBase& Creep, UAnimSequenceBase* Fall);

	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense|Enemy")
	TObjectPtr<USkeletalMeshComponentBudgeted> Body;

	UPROPERTY(Transient)
	TObjectPtr<const UBDEnemyData> Data;

	/** Where the body rests on the root, and how big, when the sink starts from. */
	FVector RestLocation = FVector::ZeroVector;
	FVector RestScale = FVector::OneVector;

	/** How far down the body goes: its own height, so it is gone under the floor. */
	float SinkDepth = 0.0f;

	/** Seconds since the kill, dilated like the game. */
	float Age = 0.0f;

	/** Age at which the sink starts, and how long it takes. */
	float SinkStart = 0.0f;
	float SinkSeconds = 0.4f;
};

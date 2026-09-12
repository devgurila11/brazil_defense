// Brazil Defense. Definition of one kind of enemy.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BDEnemyData.generated.h"

class ABDEnemyBase;
class UStaticMesh;

/**
 * One kind of creep: how tough it is, how fast it walks and what it is worth.
 *
 * Speeds are in cells, not centimetres. The board is the unit the designer reasons
 * in ("crosses the Esplanada in twenty seconds"), and the cell size is a project
 * setting that must be free to change without every enemy asset going stale.
 */
UCLASS(BlueprintType, meta = (DisplayName = "BD Enemy"))
class BRAZIL_DEFENSE_API UBDEnemyData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Asset type used to discover every enemy through the asset manager. */
	static const FPrimaryAssetType EnemyAssetType;

	//~ Begin UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UPrimaryDataAsset interface

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MaxHealth = 100.0f;

	/** Walking speed, in cells per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveSpeed = 1.0f;

	/** How fast the creep reaches MoveSpeed after spawning, in cells per second squared. 0 starts it at full speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Acceleration = 2.0f;

	/** Added to the blue counter when this creep is killed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Votes", meta = (ClampMin = "0", UIMin = "0"))
	int32 VotesOnDeath = 1;

	/** Added to the red counter when this creep reaches the urn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Votes", meta = (ClampMin = "0", UIMin = "0"))
	int32 VotesOnArrival = 1;

	/** Pawn spawned for this enemy. Loaded on demand. Falls back to ABDEnemyBase itself when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TSoftClassPtr<ABDEnemyBase> EnemyClass;

	/**
	 * Mesh handed to the pawn at spawn, so a plain ABDEnemyBase can stand in for a creep
	 * without a Blueprint per enemy. Optional: a Blueprint class with its own mesh leaves
	 * this empty.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/** Scale applied to Mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", meta = (EditCondition = "Mesh != nullptr"))
	FVector MeshScale = FVector::OneVector;
};

// Brazil Defense. The red candidate: the one creep that ends the match.

#pragma once

#include "CoreMinimal.h"
#include "Enemy/BDEnemyBase.h"
#include "BDCandidate.generated.h"

class UBDCandidateSubsystem;

/**
 * The red candidate. Walks the board like any creep, on a route of its own out of a
 * mouth of its own, and is different in what it means: every defender that sees it
 * shoots it before anything else, the wave does not wait for it to clear, and where a
 * creep reaching the urn scores a vote, the candidate reaching it ends the match.
 *
 * It is not a creep of the wave: UBDCandidateSubsystem sends it out when the red
 * counter passes the blue one and decides what its death and its arrival do. This
 * class only knows how to walk, how to show its health and whom to tell.
 */
UCLASS(Blueprintable, meta = (DisplayName = "BD Candidate"))
class BRAZIL_DEFENSE_API ABDCandidate : public ABDEnemyBase
{
	GENERATED_BODY()

public:
	ABDCandidate();

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	//~ End AActor interface

	//~ Begin ABDEnemyBase interface
	virtual bool IsCandidate() const override { return true; }
	virtual float GetBaseMoveSpeed() const override;
	//~ End ABDEnemyBase interface

	/** Seconds since it came out of its mouth. Dilated like everything else. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Candidate")
	float GetTimeAlive() const;

protected:
	//~ Begin ABDEnemyBase interface
	virtual void Arrive() override;
	virtual void Die() override;
	//~ End ABDEnemyBase interface

private:
	UBDCandidateSubsystem* GetCandidates() const;

	/** The health bar over its head, drawn with debug lines until there is a HUD. */
	void DrawHealthBar() const;

	/** World time it was sent out at. */
	float SpawnTimeSeconds = 0.0f;
};

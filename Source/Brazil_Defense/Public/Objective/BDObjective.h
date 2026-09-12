// Brazil Defense. The urn: the one point on the map every creep is walking to.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BDObjective.generated.h"

class UStaticMeshComponent;

/**
 * Marks where the urn actually stands. The Goal cells of the grid say which cells the
 * routes end in; this actor says where, inside them, the creeps converge.
 *
 * The two are different on purpose. The grid is 22 cells tall, so it has no middle
 * cell: the visual center of the Esplanada falls on the boundary between rows 10 and
 * 11, and the urn sits there, half a cell off any cell center. Ending a route on a cell
 * center leaves the creeps standing visibly beside the urn, and with a goal three cells
 * wide, standing in three different places. So a route ends at its Goal cell and the
 * creep then walks to this actor, and arrival is measured against it.
 *
 * Optionally carries the urn mesh itself, so the visual and the point cannot drift
 * apart. Never spatially loaded: an objective that streams out is a match without one.
 */
UCLASS(Blueprintable, meta = (DisplayName = "BD Objective"))
class BRAZIL_DEFENSE_API ABDObjective : public AActor
{
	GENERATED_BODY()

public:
	ABDObjective();

#if WITH_EDITOR
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
#endif

	/** The objective of a world, or null when none is placed. */
	static ABDObjective* Get(const UWorld* World);

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Objective")
	UStaticMeshComponent* GetMesh() const { return Mesh; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Brazil Defense|Objective", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;
};

// Brazil Defense. The buses parked over the mouths, following them along the edge.

#pragma once

#include "CoreMinimal.h"
#include "Grid/BDGridTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDBusSubsystem.generated.h"

class AStaticMeshActor;
struct FBDSpawnPoint;

/** One bus and the mouth it serves. */
struct FBDBus
{
	/** The placed actor. Weak: the level owns it, and a streamed level may take it away. */
	TWeakObjectPtr<AStaticMeshActor> Actor;

	/** Authored exit cell of the mouth this bus serves: the key it was bound by. */
	FBDCellCoord Anchor;

	/** Where the bus was placed, over the anchor. Every later spot is this plus the mouth's slide. */
	FVector Home = FVector::ZeroVector;

	/** The pull up under way: from where, to where, and how far into it. Done once Elapsed reaches Duration. */
	FVector From = FVector::ZeroVector;
	FVector To = FVector::ZeroVector;
	float Elapsed = 0.0f;
	float Duration = 0.0f;

	/** Half the length of the bus along its edge, in centimetres. */
	float HalfLength = 0.0f;

	/** Its footprint on the floor: half size of its bounds, and their center relative to the actor. What MinBusGap is measured between. */
	FVector2D Extent = FVector2D::ZeroVector;
	FVector2D BoundsOffset = FVector2D::ZeroVector;

	bool IsMoving() const { return Elapsed < Duration; }
};

/**
 * The buses of the map, bound to the mouths. The mouths are the logic: they slide along
 * their edge before a wave (UBDWaveSubsystem::WanderSpawnPoints), and the horde comes out
 * of them. The buses are the picture: plain static mesh actors placed by hand over the
 * authored mouths, each bound once to the mouth whose anchor it stands over, and moved
 * with it so the horde always leaves from the caravan.
 *
 * A bus only ever translates along its edge, the way it is parked; it never turns. It
 * cannot drift from its mouth either: its spot is its home plus the mouth's own slide,
 * which the mouth's leash already bounds.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDBusSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	//~ End USubsystem interface

	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject interface

	/**
	 * Sends every bus to where its mouth is now, over BusMoveSeconds. Binds the buses not
	 * bound yet first. Called by the wave subsystem once the mouths have slid for a wave.
	 * @return whether any bus has somewhere to go, so the creeps know to wait for it.
	 */
	bool FollowMouths(const TArray<FBDSpawnPoint>& Points);

	/**
	 * Free floor between the buses of two mouths if they stood at those exits, in cells,
	 * footprint to footprint: negative when they would overlap. Any two mouths, the same
	 * edge or two edges meeting at a corner. Max float when either mouth has no bus.
	 */
	float GetGapCells(const FBDCellCoord& Anchor, const FBDCellCoord& Exit, const FBDCellCoord& OtherAnchor, const FBDCellCoord& OtherExit) const;

	/** Seconds until the last bus on the move has parked. 0 with every bus at its mouth. */
	float GetParkRemaining() const;

	/** The smallest gap between any two buses at the mouths' current exits, in cells. Max float with fewer than two buses. */
	float GetMinGapCells(const TArray<FBDSpawnPoint>& Points) const;

	/**
	 * How many buses are bound, and how many of those are not headed to their mouth's exit.
	 * The second is 0 whenever buses and mouths are together. Regression and debugging.
	 */
	void CountBuses(const TArray<FBDSpawnPoint>& Points, int32& OutBound, int32& OutAstray) const;

private:
	/** Finds the placed bus standing over an anchor, and records it. @return the bus, or null. */
	FBDBus* Bind(const FBDCellCoord& Anchor);

	/** The bus of an anchor, bound on the spot when it is not yet. Null for a mouth with no bus. */
	const FBDBus* FindOrBind(const FBDCellCoord& Anchor) const;

	/** Where a bus parks for a mouth whose exit is at a cell. */
	FVector SpotFor(const FBDBus& Bus, const FBDCellCoord& Exit) const;

	/** The board cell a bus parked just off the edge stands over: its cell, pulled onto the board. */
	FBDCellCoord CellUnder(const FVector& Location) const;

	/** Every bound bus, by the anchor of its mouth. */
	TMap<FBDCellCoord, FBDBus> Buses;

	/** Exit cell of every mouth by its anchor, as last handed over: where a bus that streams back in belongs. */
	TMap<FBDCellCoord, FBDCellCoord> ExitByAnchor;

	/** Seconds until anchors without a bus are looked for again. A streamed bus may turn up later. */
	float RebindTimer = 0.0f;
};

// Brazil Defense. The STF and the ministers it lets out.

#pragma once

#include "CoreMinimal.h"
#include "Grid/BDGridTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDMinisterSubsystem.generated.h"

class ABDMinister;

/**
 * The STF lets the ministers out (slice 3a). It is the building placed in the map, found by
 * its mesh (UBDMinisterSettings::StfMesh), on the side of the board away from the buses.
 * Its door is the board cell nearest to it that a man may stand on; outside the door,
 * against the building, is where a minister appears and where a fleeing one is gone.
 *
 * Without the building in the world (a test map), the door falls on the middle of the
 * board edge no mouth opens on, and a minister appears a little beyond it.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDMinisterSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBDMinisterSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem interface
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	//~ End USubsystem interface

	/** Lets Count ministers out of the STF door, side by side. @return how many came out. */
	int32 SpawnMinisters(int32 Count);

	/** The STF door: its cell on the board, the point outside it, and the building. @return false without a board. */
	bool FindDoor(FBDCellCoord& OutCell, FVector& OutOutside, const AActor*& OutStf);

	/** The ministers out now. */
	TArray<ABDMinister*> GetMinisters() const;

	/** The nearest minister a kick would land on within Radius of a point, or null. */
	ABDMinister* FindKickable(const FVector& From, float Radius) const;

private:
	/** The STF building, found once and kept. Null when the map has none. */
	const AActor* FindStf();

	TWeakObjectPtr<const AActor> Stf;
	bool bStfSearched = false;
};

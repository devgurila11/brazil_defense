// Brazil Defense. The STF and the ministers it lets out.

#pragma once

#include "CoreMinimal.h"
#include "Grid/BDGridTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDMinisterSubsystem.generated.h"

class ABDMinister;

/**
 * The STF lets the ministers out: one for every candidate killed, the next of its line of
 * eleven, each one covering wider than the one before him (slice 3b). A kicked one goes
 * back in at the end of the line, out again only after the others. It is the building placed in the map, found by
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

	/**
	 * A candidate is down: the next minister of the STF's line comes out, if any is inside
	 * (UBDMinisterSettings::bReleaseOnCandidateKilled). @return whether one came out.
	 */
	bool NotifyCandidateKilled();

	/** A minister is back inside (or gone): his seat to the end of the line. */
	void ReturnSeat(int32 Seat);

	/**
	 * Every minister off the board and the STF full again, seats 1 to 11 in order: for when
	 * the board is undone (a rewind to wave 0, a load). The endless run never calls it -
	 * ministers only ever leave the board by the Mito's kick.
	 */
	void ResetForNewMatch();

	/** The seats inside the STF, the next one out first. */
	const TArray<int32>& GetLine();

	/** Ministers let out by candidates killed this match. */
	int32 GetReleasedByKills() const { return ReleasedByKills; }

	/** The STF door: its cell on the board, the point outside it, and the building. @return false without a board. */
	bool FindDoor(FBDCellCoord& OutCell, FVector& OutOutside, const AActor*& OutStf);

	/** The ministers out now. */
	TArray<ABDMinister*> GetMinisters() const;

	/** Ministers on their feet on the board: out, and not kicked on their way back. */
	int32 CountStanding() const;

	/** The nearest minister a kick would land on within Radius of a point, or null. */
	ABDMinister* FindKickable(const FVector& From, float Radius) const;

private:
	/** The STF building, found once and kept. Null when the map has none. */
	const AActor* FindStf();

	TWeakObjectPtr<const AActor> Stf;
	bool bStfSearched = false;
	int32 ReleasedByKills = 0;

	/** The seats inside, in the order they come out; filled with 1..MaxOnBoard the first time it is asked. */
	TArray<int32> Line;
	bool bLineReady = false;
};

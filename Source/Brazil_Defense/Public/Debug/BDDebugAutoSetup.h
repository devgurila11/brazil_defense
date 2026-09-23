// Brazil Defense. A full defense built on Play, so a test never starts from an empty board.

#pragma once

#include "CoreMinimal.h"
#include "Grid/BDGridTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDDebugAutoSetup.generated.h"

class ABDMatchManager;
class UBDPlaceableData;
class UBDPlacementComponent;
class UBDWaveSubsystem;
struct FRandomStream;

/**
 * Debug tooling. When BD.Debug.AutoSetup is on, the first tick of a game world builds a
 * complete defense through the player's own placement component, so it obeys exactly
 * the rules a hand-placed one does: the urn in its zone, every platform the budget
 * allows spread along the routes, every slot filled with characters, every ground tower
 * placed near the routes, and a few fences where the budget still allows. Each Play
 * draws a new seed, logged so a scenario that showed something can be brought back with
 * BD.Debug.AutoSetup.Seed.
 *
 * Spread matters more than cleverness: the routes are cut into stretches and each piece
 * goes to its own stretch, because a defense heaped around the urn tests nothing. On top
 * of that the board is split into regions and the pieces are dealt round robin over them,
 * the platform slots are filled a slot at a time across every platform rather than one
 * platform at a time, and any route left without a defender in range gets the next one:
 * the setup does not have to be optimal, it has to look like somebody thought about it.
 *
 * The same setup runs in headless test sessions, so numbers reported from them come
 * from the board the tester sees on screen.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDDebugAutoSetup : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UWorldSubsystem interface
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	//~ End UWorldSubsystem interface

	/** Builds the defense now, with the seed given (0 draws one) and every defender at a level. Safe to call again after ClearAll. */
	void Run(int32 Seed, int32 DefenderLevel = 1);

	/** Takes everything off the board, creeps included, and hands the budgets back. */
	void ClearAll();

	/**
	 * Spends whatever the board has gained since it was built: platforms still in hand,
	 * slots left empty, and the public money the bosses paid in. Called between waves by the
	 * simulation, because a defense that never reinvests its income is not the defense a
	 * player would have. Does nothing when there is nothing to spend.
	 * @return how many pieces were placed.
	 */
	int32 BuildGrantedBudget();

	/** Seed of the last setup, for the log and for repeating it. */
	int32 GetLastSeed() const { return LastSeed; }

	/** How many times the granted budget has been built, so each pass draws its own spots. */
	int32 GrantedPasses = 0;

private:
	/** The placement component of the first player controller, or null. */
	UBDPlacementComponent* FindPlacement() const;
	ABDMatchManager* FindMatch() const;
	UBDWaveSubsystem* FindWaves() const;

	/** Every placeable asset of the project, by the kind the match budgets it as. */
	void GatherPlaceables();

	/** Flips the debug switches a balancing session wants on. */
	static void ApplyDebugSwitches();

	/**
	 * How many pieces of a palette the public money in hand buys, at a share of it and at
	 * the average price of the kinds. This is what decides how big the automatic
	 * defense gets: nothing counts defenders out any more, so the capital is the whole
	 * answer.
	 */
	int32 AffordableCount(const TArray<TObjectPtr<UBDPlaceableData>>& Pieces, float Share) const;

	bool PlaceObjective(UBDPlacementComponent& Placement, FRandomStream& Stream);
	int32 PlacePlatforms(UBDPlacementComponent& Placement, FRandomStream& Stream);
	int32 FillSlots(UBDPlacementComponent& Placement, FRandomStream& Stream);
	int32 PlaceTowers(UBDPlacementComponent& Placement, FRandomStream& Stream);

	/**
	 * Puts whatever ground budget is left on the routes no defender covers. The spread
	 * passes deal by region and by route, but a piece that found no legal cell leaves its
	 * route open, and an open route is a free lane for the whole match.
	 */
	int32 CoverUncoveredRoutes(UBDPlacementComponent& Placement, FRandomStream& Stream);

	/**
	 * Logs how the finished defense is actually spread: defenders per region of the
	 * board, slots filled per platform, and how many routes have a defender in range.
	 * Counting the pieces says nothing about where they went, and where they went is the
	 * whole point of this setup - this is the line a headless session is read from.
	 */
	void LogDistribution() const;

	/**
	 * The maze, and it comes before every defender: fences that make the horde walk
	 * further, because the longer the walk the longer it is under fire. First a ring
	 * around the urn with one way in, then a serpentine laid across the longest route,
	 * re-read after every fence because every fence moves it.
	 *
	 * The path validation is what keeps this honest: a fence that would seal a mouth off
	 * is refused by the placement itself, so the maze can never close the board.
	 * @return how many fences went down.
	 */
	int32 BuildMaze(UBDPlacementComponent& Placement, FRandomStream& Stream);

	/** Fences the four sides of the urn but one, so the horde has to come in through a door. @return how many went down. */
	int32 RingTheUrn(UBDPlacementComponent& Placement, FRandomStream& Stream);

	/**
	 * Spots to try for the next batch of cell pieces, ranked by how much of the route
	 * they cover: a cell is worth the number of route cells within reach of it, so the
	 * bends of the serpentine - where the horde passes again and again within one range -
	 * outscore a straight stretch on their own, with no special case for a corner.
	 *
	 * FocusUrn tilts the ranking towards the last cells of the route, which is where the
	 * candidate has to be stopped. Called after the maze, so the route it ranks is the
	 * route the horde will actually walk.
	 */
	void BuildCorridorTargets(int32 Count, FRandomStream& Stream, TArray<FBDCellCoord>& OutTargets) const;

	/** Average and longest route over the mouths that have one, for the before/after of the maze. */
	void MeasureRoutes(float& OutAverage, int32& OutLongest, int32& OutRouted) const;

	/** The cell the urn was placed on, remembered so the maze can build around it. */
	FBDCellCoord ObjectiveCell;

	/** Tries a cell piece around a target cell, any rotation, nearest ring first. @return true when placed. */
	bool TryPlaceCellPieceNear(UBDPlacementComponent& Placement, UBDPlaceableData* Piece,
		const FBDCellCoord& Target, FRandomStream& Stream) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBDPlaceableData>> ObjectivePieces;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBDPlaceableData>> PlatformPieces;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBDPlaceableData>> CharacterPieces;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBDPlaceableData>> TowerPieces;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBDPlaceableData>> DividerPieces;

	int32 LastSeed = 0;
};

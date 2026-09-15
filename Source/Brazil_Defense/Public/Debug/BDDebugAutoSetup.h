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
	 * Places whatever platform, character and tower budget has been granted since the
	 * board was built - what the bosses hand out as they fall. Called between waves by
	 * the simulation, because a defense that never spends its new ceilings is not the
	 * defense a player would have. Does nothing when no budget is left.
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

	bool PlaceObjective(UBDPlacementComponent& Placement, FRandomStream& Stream);
	int32 PlacePlatforms(UBDPlacementComponent& Placement, FRandomStream& Stream);
	int32 FillSlots(UBDPlacementComponent& Placement, FRandomStream& Stream);
	int32 PlaceTowers(UBDPlacementComponent& Placement, FRandomStream& Stream);
	int32 PlaceFences(UBDPlacementComponent& Placement, FRandomStream& Stream);

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
	 * Spots to try for the next batch of cell pieces: one per piece, each on its own
	 * stretch of its own route, round robin over the routes. Empty when there are no routes.
	 */
	void BuildStretchTargets(int32 Count, FRandomStream& Stream, TArray<FBDCellCoord>& OutTargets) const;

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

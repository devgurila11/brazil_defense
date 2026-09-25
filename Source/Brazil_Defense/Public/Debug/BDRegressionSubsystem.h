// Brazil Defense. The thermometer: every mechanic that already works, checked in one go.

#pragma once

#include "CoreMinimal.h"
#include "Grid/BDGridTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDRegressionSubsystem.generated.h"

class ABDMatchManager;
class ABDTowerBase;
class UBDPlaceableData;
class UBDPlacementComponent;
class UBDPlatformComponent;

/**
 * BD.Test.Regression: plays a short scripted match on a fresh board and checks, one line
 * each, the invariants the game already relies on - pathfinding, the economy, the divider
 * hand, the candidates, the count, placement and the platforms. Every line is PASS or
 * FAIL with what was measured, and the last line is the total. A FAIL means a recent
 * change broke an old mechanic: run it before calling any batch of work done.
 *
 * The script moves one step per frame, so what the game only settles on its own tick
 * (routes, a platform's height, the parade of the fallen) is read after it settled. It
 * needs a match at wave 0 with no urn down - a fresh headless run or a fresh Play - and
 * it ends that match in a defeat, on purpose: a candidate at the urn is one of the checks.
 * The post-match report is switched off while it runs, so no test row reaches the sheet.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDRegressionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	//~ End FTickableGameObject interface

	//~ Begin USubsystem interface
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	/** Starts the script. @param bQuitWhenDone asks the process to exit once the total is out, for headless runs. */
	void Start(bool bQuitWhenDone);

	bool IsRunning() const { return bRunning; }

private:
	ABDMatchManager* GetMatch() const;
	UBDPlacementComponent* GetPlacement() const;

	/** One line of the report. */
	void Check(const TCHAR* Area, const TCHAR* What, bool bPass, const FString& Measured);

	/** The total, the switches put back, and the exit when asked. */
	void Finish(const FString& Why);

	/** A step of the script. @return false when the script cannot go on (the reason is already a FAIL line). */
	bool RunStep(int32 Index);

	/** Takes a piece into the hand and puts it on the first cell near a point that takes it. @return the cell, or an invalid one. */
	bool PlaceNear(UBDPlaceableData* Piece, const FBDCellCoord& Near, int32 Radius, FBDCellCoord& OutCell);

	/** Places the held fence on an edge, turned the way the edge runs. */
	bool PlaceFence(const FBDEdgeCoord& Edge);

	/** The defender standing that was not in a list taken before. */
	ABDTowerBase* FindNewTower(const TArray<ABDTowerBase*>& Before) const;

	void HandleVotesChanged(int32 Blue, int32 Red);

	bool bRunning = false;
	bool bQuit = false;
	int32 Step = 0;
	int32 WaitTicks = 0;
	int32 StepTicks = 0;
	int32 Passed = 0;
	int32 Failed = 0;

	/** The switches the script turns, and what they were, to put them back. */
	int32 SavedFreezeTimer = 0;
	int32 SavedPostMatch = 1;
	int32 SavedWaveLog = 1;

	//~ What the script built and measured, carried from one step to the next.
	FBDCellCoord UrnCell;
	FBDCellCoord TowerCell;
	TWeakObjectPtr<ABDTowerBase> GroundTower;
	TWeakObjectPtr<UBDPlatformComponent> Stand;
	TArray<TWeakObjectPtr<ABDTowerBase>> Crew;
	float SlotZAtLevelOne = 0.0f;

	/** The count watched the whole run: blue may only go down through the levelling after a parade. */
	FDelegateHandle VotesHandle;
	TWeakObjectPtr<ABDMatchManager> WatchedMatch;
	int32 LastBlue = 0;
	int32 LevelDownsSeen = 0;
	int32 UnexplainedDrops = 0;
};

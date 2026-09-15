// Brazil Defense. A match written down, to be picked up again at the start of a wave.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Grid/BDGridTypes.h"
#include "Match/BDMatchTypes.h"
#include "BDMatchSave.generated.h"

/** One piece as it stood on the board: enough to put it back through the normal placement. */
USTRUCT()
struct FBDSavedPiece
{
	GENERATED_BODY()

	/** The placeable asset. Pieces with no asset behind them are not saved. */
	UPROPERTY()
	FSoftObjectPath Data;

	/** Cell pieces: the origin. Slot pieces: the cell of the platform they stand on. */
	UPROPERTY()
	FBDCellCoord Origin;

	/** Edge pieces: the first edge of the segment; the rest follows from the length. */
	UPROPERTY()
	TArray<FBDEdgeCoord> Edges;

	/** Quarter turns the piece was placed with. */
	UPROPERTY()
	int32 RotationSteps = 0;

	/** Slot pieces: which slot of the platform. INDEX_NONE for everything else. */
	UPROPERTY()
	int32 SlotIndex = INDEX_NONE;

	/** Defenders: the level reached. */
	UPROPERTY()
	int32 Level = 1;

	bool IsOnSlot() const { return SlotIndex != INDEX_NONE; }
	bool IsOnEdge() const { return Edges.Num() > 0; }
};

/**
 * The whole of a match at the start of a wave: where the score stands, what the player
 * has left, what is on the board. One slot per game, overwritten by each save: the
 * saves a difficulty hands out are how many times the player may write it, not how
 * many they keep. Restored in place by ABDMatchManager::RestoreMatch.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDMatchSave : public USaveGame
{
	GENERATED_BODY()

public:
	static const TCHAR* SlotName;
	static constexpr int32 UserIndex = 0;

	/** Whether a saved match exists on disk. */
	static bool Exists();

	/** The saved match, or null when there is none or it cannot be read. */
	static UBDMatchSave* LoadFromSlot();

	/** Writes this to the slot. @return false when the platform refused. */
	bool WriteToSlot() const;

	UPROPERTY()
	EBDDifficulty Difficulty = EBDDifficulty::Normal;

	UPROPERTY()
	int32 ObstacleSeed = 0;

	/** The wave that had been cleared when the save was taken; the match resumes building for the next. */
	UPROPERTY()
	int32 Wave = 0;

	UPROPERTY()
	int32 VotesBlue = 0;

	UPROPERTY()
	int32 VotesRed = 0;

	UPROPERTY()
	int32 VotesNull = 0;

	/** The thief's counter: bribe recovered and not yet minted. */
	UPROPERTY()
	int32 BribeHeld = 0;

	/** The mint's counter: what evolution is paid with. */
	UPROPERTY()
	int32 PublicMoney = 0;

	UPROPERTY()
	int32 EarlyCallBonus = 0;

	UPROPERTY()
	float GameSpeed = 1.0f;

	/** Saves left after this one was taken, so loading does not hand the spent one back. */
	UPROPERTY()
	int32 SavesRemaining = 0;

	UPROPERTY()
	bool bWon = false;

	UPROPERTY()
	bool bEndless = false;

	UPROPERTY()
	int32 DividersRemaining = 0;

	UPROPERTY()
	int32 PlatformsRemaining = 0;

	UPROPERTY()
	int32 TowersRemaining = 0;

	UPROPERTY()
	int32 CharactersRemaining = 0;

	UPROPERTY()
	TArray<FBDSavedPiece> Pieces;

	UPROPERTY()
	FDateTime SavedAt;
};

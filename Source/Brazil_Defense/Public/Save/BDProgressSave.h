// Brazil Defense. What the player has beaten, kept across matches and sessions.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Match/BDMatchTypes.h"
#include "BDProgressSave.generated.h"

/**
 * The difficulties won so far. One slot per game, written on every win. This is what
 * chains the difficulties: a match on the next one up reads it to hand out its starting
 * bonus (UBDDifficultyData::ChainBonus).
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDProgressSave : public USaveGame
{
	GENERATED_BODY()

public:
	static const TCHAR* SlotName;
	static constexpr int32 UserIndex = 0;

	/** Whether a difficulty has been won, from the slot. False when there is no slot. */
	static bool HasWonDifficulty(EBDDifficulty Difficulty);

	/** Marks a difficulty won and writes the slot. Nothing happens when it already was. */
	static void RecordWin(EBDDifficulty Difficulty);

	/** Forgets every win. */
	static void ResetProgress();

	/** The slot, or a fresh empty one when there is none. */
	static UBDProgressSave* LoadOrCreate();

	bool HasWon(EBDDifficulty Difficulty) const;
	void MarkWon(EBDDifficulty Difficulty);
	bool WriteToSlot() const;

	/** One bit per EBDDifficulty value. */
	UPROPERTY()
	uint8 WonMask = 0;
};

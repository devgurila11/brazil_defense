// Brazil Defense. Lines out of a bank without repeats: every line once before any line twice.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDSpeechDeck.generated.h"

class USoundBase;
class USoundCue;
class USoundNode;

/**
 * Draw without replacement over Count items: each draw takes one of those not drawn yet,
 * and only when all have been drawn is the bag refilled. A refill never hands out first
 * the item drawn last, so no line is ever said twice in a row; a bag of one always gives
 * that one. A different Count starts a fresh bag.
 */
struct BRAZIL_DEFENSE_API FBDShuffleBag
{
	int32 Draw(int32 Count, FRandomStream* Stream = nullptr);

	/** Items still to come before the next refill. */
	int32 GetLeft() const { return Left.Num(); }

private:
	TArray<int32> Left;
	int32 LastDrawn = INDEX_NONE;
	int32 BagCount = 0;
};

/** One bank: its lines as cues of their own, and the bag they are drawn from. */
USTRUCT()
struct FBDSpeechDeckEntry
{
	GENERATED_BODY()

	/** A copy of the bank per line, with the random choice cut down to that line. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundCue>> Lines;

	FBDShuffleBag Bag;
};

/**
 * The banks of words - the militants' sentences, the Mito's and Nicole's celebrations,
 * the candidate's speech - are Sound Cues with a random node over their lines. The node
 * draws on its own, inside the audio device, out of reach of the game and of a test. So
 * the choice is made here instead: the line is drawn from the bank's bag, and what plays
 * is a copy of the bank whose random node has only that line left. The copy keeps the
 * rest of the cue - its pitch and volume spread, its class, its attenuation.
 *
 * Draw only when the line is actually going to play: a line drawn and then held back by
 * the speech rule would count as said. A bank with no random node, or one line, plays as
 * it is.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDSpeechDeck : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBDSpeechDeck* Get(const UObject* WorldContextObject);

	/** The next line of Bank: a cue with that line alone, or Bank itself when it holds no choice. Null stays null. */
	USoundBase* DrawLine(USoundBase* Bank);

	/** How many lines Bank has to choose from: 1 for a sound with no random node. */
	int32 GetLineCount(USoundBase* Bank);

	/** Which of Bank's lines a cue handed out by DrawLine is, INDEX_NONE for anything else. For the regression. */
	int32 GetLineIndex(USoundBase* Bank, const USoundBase* Line);

	/** The first random node of a cue's graph, walking from its first node, or null. */
	static USoundNode* FindRandomNode(const USoundCue& Cue);

private:
	/** Builds the entry of a bank the first time it is asked for. Null when it holds no choice. */
	FBDSpeechDeckEntry* FindOrBuild(USoundBase* Bank);

	UPROPERTY(Transient)
	TMap<TObjectPtr<USoundBase>, FBDSpeechDeckEntry> Decks;

	/** Banks already looked at and found to hold no choice, so they are not walked again. */
	UPROPERTY(Transient)
	TSet<TObjectPtr<USoundBase>> Plain;
};

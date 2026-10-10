// Brazil Defense. Who is talking: an exclamation over whoever is saying a sentence, coloured by side.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDSpeechMarks.generated.h"

class UAudioComponent;

/** The side a speaker is on, which is the colour of the mark over him. */
UENUM(BlueprintType)
enum class EBDSpeakerSide : uint8
{
	/** The militants and the candidates: red. */
	Opponent,

	/** The player's own people - the shooters, the Agent: blue. */
	Player,

	/** The ministers, in their black robes. None exists yet; they pass it themselves when they come. */
	Minister,
};

/**
 * The marks of the sentences being said right now: one per speaker, from the moment the
 * words start to the moment their sound stops. ABDMatchHUD draws them. Only words make
 * one - a bray, a hoof, a shot does not - so whoever plays a sentence tells this.
 *
 * A mark lives exactly as long as its audio component plays: stopped, finished or gone,
 * the mark is dropped on the next look, with no fade. A speaker who starts a new sentence
 * keeps his one mark, popped again.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDSpeechMarkSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	struct FMark
	{
		TWeakObjectPtr<AActor> Speaker;
		TWeakObjectPtr<UAudioComponent> Audio;
		EBDSpeakerSide Side = EBDSpeakerSide::Opponent;

		/** Real seconds when the words started: the pop runs from here. */
		double StartTime = 0.0;
	};

	static UBDSpeechMarkSubsystem* Get(const UObject* WorldContextObject);

	//~ Begin USubsystem interface
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	//~ End USubsystem interface

	/** The side of a speaker by what he is: creeps and candidates Opponent, shooters and the Agent Player. */
	static EBDSpeakerSide SideOf(const AActor& Speaker);

	/** A sentence started on this audio component, said by this speaker: his mark shows until it stops. */
	void NoteSpeech(AActor& Speaker, UAudioComponent& Audio);
	void NoteSpeech(AActor& Speaker, UAudioComponent& Audio, EBDSpeakerSide Side);

	/** The marks of the sentences still sounding; drops the finished ones first. */
	const TArray<FMark>& GetLiveMarks();

	/** Marks held for this speaker right now, before the finished are dropped: never more than one. */
	int32 CountMarks(const AActor& Speaker) const;

	/** Marks started since the world began, and how many of each side. */
	int32 GetMarksStarted() const { return MarksStarted; }
	int32 GetMarksStarted(EBDSpeakerSide Side) const { return MarksBySide[static_cast<int32>(Side)]; }

private:
	void Prune();

	TArray<FMark> Marks;
	int32 MarksStarted = 0;
	int32 MarksBySide[3] = { 0, 0, 0 };
};

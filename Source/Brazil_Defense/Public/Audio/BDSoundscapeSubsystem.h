// Brazil Defense. The music of the waves and the ambience under everything.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BDSoundscapeSubsystem.generated.h"

class UAudioComponent;
class USoundBase;
class USoundClass;
struct FStreamableHandle;

/**
 * One endless strand of sound made of clips: a random clip, the next one fading in over
 * its last seconds, never the same twice in a row. Two voices take turns.
 */
USTRUCT()
struct FBDSoundStrand
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> Voices[2];

	/** The voice playing now, INDEX_NONE when the strand is silent. */
	int32 Current = INDEX_NONE;

	/** Index in the list of the clip playing, so the next draw avoids it. */
	int32 LastPick = INDEX_NONE;

	/** Seconds into the current clip, and how long it lasts. */
	float Elapsed = 0.0f;
	float Duration = 0.0f;

	/** The strand's level as last set, eased towards the wanted one. */
	float Gain = 0.0f;

	/** Clips started since the strand came up, for the log and the checks. */
	int32 ClipsStarted = 0;

	/** Name of the clip playing, for the log and the checks. */
	FString CurrentName;

	bool IsPlaying() const { return Current != INDEX_NONE; }
};

/**
 * The soundscape of a match, all of it 2D:
 *
 * - Music while a wave is on: a battle track at random, the next one crossfaded in before
 *   it ends, a fade out when the wave is over and a new track when the next one starts.
 *   On the music class, so the options' music slider and the mute own it.
 * - Ambience always: the city and the birds by day, the night insects and the owl by night,
 *   fewer cars; mixed by the day/night cycle and lowered under the music. On the effects class.
 *
 * Keeps playing through a pause: what the player hears does not stop with the board.
 * What goes in the slots is UBDAudioSettings'.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDSoundscapeSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem interface
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;
	//~ End USubsystem interface

	//~ Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	//~ End FTickableGameObject interface

	static UBDSoundscapeSubsystem* Get(const UObject* WorldContext);

	/** 0 by day, 1 at the dead of night, eased through dusk and dawn, by the match's clock. */
	float GetNightAmount() const;

	/** One line per strand: playing or not, the clip, its level. */
	FString Describe() const;

	const FBDSoundStrand& GetMusic() const { return Music; }

	/** Forces the music on or off whatever the phase; INDEX_NONE gives it back to the phase. */
	void DebugForceMusic(int32 bOn) { ForcedMusic = bOn; }

private:
	/**
	 * Runs a strand one step: starts it, chains the next clip in before the current one
	 * ends, fades it out when no longer wanted, and eases its level.
	 * @param Crossfade seconds the next clip overlaps the last; FadeIn and FadeOut those of the strand as a whole.
	 */
	void StepStrand(FBDSoundStrand& Strand, const TArray<TSoftObjectPtr<USoundBase>>& Clips, bool bWanted, float WantedGain,
		float Crossfade, float FadeIn, float FadeOut, USoundClass* SoundClass, float DeltaSeconds, const TCHAR* Label);

	/** Starts the next clip of a strand on its free voice, fading in over Fade seconds. @return false with no playable clip. */
	bool StartNextClip(FBDSoundStrand& Strand, const TArray<TSoftObjectPtr<USoundBase>>& Clips, float Fade, USoundClass* SoundClass, const TCHAR* Label);

	void StopStrand(FBDSoundStrand& Strand, float Fade);

	/** Whether the music belongs on now: a wave on and the match not over. */
	bool WantsMusic() const;

	UPROPERTY(Transient)
	FBDSoundStrand Music;

	UPROPERTY(Transient)
	FBDSoundStrand City;

	UPROPERTY(Transient)
	FBDSoundStrand Birds;

	UPROPERTY(Transient)
	FBDSoundStrand Night;

	/** The ambience's own duck under the music, eased so it does not jump with the fade. */
	float Duck = 1.0f;

	/** See DebugForceMusic. */
	int32 ForcedMusic = INDEX_NONE;

	/** Every clip of the slots, loaded in the background as the match starts and held for it. */
	TSharedPtr<FStreamableHandle> Preload;

	/** Seconds until the next status line in the log. */
	float ReportTimer = 0.0f;
};

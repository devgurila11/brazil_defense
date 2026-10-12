// Brazil Defense. Tooling: build a random, pitch varied Sound Cue out of a folder of waves.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BDBuildSoundCueCommandlet.generated.h"

/**
 * Builds, or rebuilds in place, a Sound Cue of the shape every creep sound uses: the waves
 * of a folder into a Random node (no repeat until all have played), a Modulator for a
 * light pitch and volume variation so repeats never sound identical, and the output. The
 * cue is routed to one class of the interface settings (-Class; a new cue defaults to the
 * effects, a rebuilt one keeps its own), so that category's slider and mute own it
 * wherever it is played from. Words go on -Class=Voice.
 *
 *   UnrealEditor-Cmd <uproject> -run=BDBuildSoundCue -Cue=/Game/BD/Audio/SCue_X
 *       -Folder=/Game/audio/Militante [-Prefix=VO_Militante_] [-Exclude=Text] [-Pitch=0.05] [-Volume=0.0] [-CuePitch=1.0]
 *       [-Class=Voice|Music|Environment|Effects]
 *
 * -Pitch and -Volume are the half width of the random variation (0.05 is +-5%); -Volume
 * only ever lowers. -CuePitch is the cue's own Pitch Multiplier: the knob to turn on a
 * duplicate to make the same words another character's voice. The graph comes out wired
 * and laid out, as if made by hand in the cue editor, and can be edited there.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDBuildSoundCueCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UBDBuildSoundCueCommandlet();

	//~ Begin UCommandlet interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet interface
};

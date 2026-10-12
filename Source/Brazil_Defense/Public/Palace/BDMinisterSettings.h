// Brazil Defense. What a minister is, until his model and his voice arrive.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BDMinisterSettings.generated.h"

class UMaterialInterface;
class USoundBase;
class UStaticMesh;

/**
 * The ministers of the STF (ABDMinister): their placeholder, their walk and how they take
 * the Agent's kick. A black block for now, already twice the size a man would be, like
 * the Agent and the candidates: the special figures stand over the horde.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Minister"))
class BRAZIL_DEFENSE_API UBDMinisterSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDMinisterSettings();

	static const UBDMinisterSettings& Get();

	//~ The STF ------------------------------------------------------------------

	/** The building they come out of and go back into: the actor in the map with this mesh. */
	UPROPERTY(config, EditAnywhere, Category = "STF")
	TSoftObjectPtr<UStaticMesh> StfMesh;

	//~ Look ---------------------------------------------------------------------

	/** The placeholder block, rested on the floor. */
	UPROPERTY(config, EditAnywhere, Category = "Look")
	TSoftObjectPtr<UStaticMesh> BlockMesh;

	/** Its material; a Color vector parameter on it takes BlockColor. */
	UPROPERTY(config, EditAnywhere, Category = "Look")
	TSoftObjectPtr<UMaterialInterface> BlockMaterial;

	/** The black of the robe. */
	UPROPERTY(config, EditAnywhere, Category = "Look")
	FLinearColor BlockColor = FLinearColor(0.02f, 0.02f, 0.02f, 1.0f);

	/** Scale of the block: twice a 2 x 2 x 4 man, as tall as the doubled Agent. */
	UPROPERTY(config, EditAnywhere, Category = "Look")
	FVector BlockScale = FVector(4.0f, 4.0f, 8.0f);

	//~ Walk ---------------------------------------------------------------------

	/** Walking pace, in cells per second. */
	UPROPERTY(config, EditAnywhere, Category = "Walk", meta = (ClampMin = "0.05"))
	float WalkSpeed = 0.4f;

	/** Degrees a second he turns at. */
	UPROPERTY(config, EditAnywhere, Category = "Walk", meta = (ClampMin = "10.0"))
	float TurnRate = 360.0f;

	/** How far one stretch of his stroll goes, in cells. */
	UPROPERTY(config, EditAnywhere, Category = "Walk", meta = (ClampMin = "1"))
	int32 WalkCellsMin = 1;

	UPROPERTY(config, EditAnywhere, Category = "Walk", meta = (ClampMin = "1"))
	int32 WalkCellsMax = 4;

	/** Chance he stands a while instead of walking on. */
	UPROPERTY(config, EditAnywhere, Category = "Walk", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IdleChance = 0.35f;

	UPROPERTY(config, EditAnywhere, Category = "Walk", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float IdleTimeMin = 1.0f;

	UPROPERTY(config, EditAnywhere, Category = "Walk", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float IdleTimeMax = 3.0f;

	//~ The kick -----------------------------------------------------------------

	/** How close the Agent comes before he kicks, in cells between their centres. */
	UPROPERTY(config, EditAnywhere, Category = "Kick", meta = (ClampMin = "0.1"))
	float KickReachCells = 1.0f;

	/** Seconds into the Agent's kick when his foot lands: the minister flies from then. */
	UPROPERTY(config, EditAnywhere, Category = "Kick", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float KickImpactSeconds = 0.45f;

	/** How high the kick sends him. */
	UPROPERTY(config, EditAnywhere, Category = "Kick", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float FlightHeight = 1500.0f;

	/** Seconds in the air. */
	UPROPERTY(config, EditAnywhere, Category = "Kick", meta = (ClampMin = "0.1", ForceUnits = "s"))
	float FlightSeconds = 1.4f;

	/** How far away from the Agent he lands, in cells, when there is floor there. */
	UPROPERTY(config, EditAnywhere, Category = "Kick", meta = (ClampMin = "0.0"))
	float FlightPushCells = 2.0f;

	/** Turns he makes in the air, in degrees. */
	UPROPERTY(config, EditAnywhere, Category = "Kick")
	float FlightSpinDegrees = 720.0f;

	/** Seconds lying on the floor before he gets up and runs. */
	UPROPERTY(config, EditAnywhere, Category = "Kick", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float DownSeconds = 1.0f;

	/** Multiplier on WalkSpeed while he runs back to the STF. */
	UPROPERTY(config, EditAnywhere, Category = "Kick", meta = (ClampMin = "1.0"))
	float FleeSpeedScale = 2.5f;

	//~ The refuge --------------------------------------------------------------

	/**
	 * How far the first minister's cover reaches, in cells, from his centre. A militant
	 * inside it while he reads is out of every defender's reach and blinks; the candidate
	 * is never covered.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Refuge", meta = (ClampMin = "0.1"))
	float RefugeRadiusCells = 2.0f;

	/** Each minister after the first in the STF's order covers this much further: the next one out is the stronger. */
	UPROPERTY(config, EditAnywhere, Category = "Refuge", meta = (ClampMin = "0.0"))
	float RefugeRadiusStepCells = 0.5f;

	/** The cover of the minister in a seat of the STF's order (1 the first out). */
	float GetRefugeRadiusCells(int32 Seat) const { return RefugeRadiusCells + RefugeRadiusStepCells * FMath::Max(0, Seat - 1); }

	/** Seconds he keeps reading once nobody is left in his radius, before he walks on. */
	UPROPERTY(config, EditAnywhere, Category = "Refuge", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ResumeAfterSeconds = 5.0f;

	/** Blinks a second of a covered militant's body. */
	UPROPERTY(config, EditAnywhere, Category = "Refuge", meta = (ClampMin = "0.5"))
	float ShelterBlinkHz = 5.0f;

	/** Share of each blink the body is drawn. */
	UPROPERTY(config, EditAnywhere, Category = "Refuge", meta = (ClampMin = "0.05", ClampMax = "0.95"))
	float ShelterBlinkShown = 0.55f;

	/** The shield: a dome on the edge of his radius, up while somebody is inside it. A sphere of the engine until the Niagara one. */
	UPROPERTY(config, EditAnywhere, Category = "Refuge")
	TSoftObjectPtr<UStaticMesh> ShieldMesh;

	/** Translucent and unlit, brighter at its rim; a Color vector parameter takes ShieldColor. */
	UPROPERTY(config, EditAnywhere, Category = "Refuge")
	TSoftObjectPtr<UMaterialInterface> ShieldMaterial;

	UPROPERTY(config, EditAnywhere, Category = "Refuge")
	FLinearColor ShieldColor = FLinearColor(0.55f, 0.8f, 1.0f, 1.0f);

	/** The book he reads while he covers: a block in his hands until his animation, BlockMaterial in BookColor. */
	UPROPERTY(config, EditAnywhere, Category = "Refuge")
	FVector BookScale = FVector(0.5f, 1.4f, 1.0f);

	UPROPERTY(config, EditAnywhere, Category = "Refuge")
	FLinearColor BookColor = FLinearColor(0.45f, 0.05f, 0.04f, 1.0f);

	//~ Out of the STF ---------------------------------------------------------

	/** One minister out of the STF for every candidate killed, the next of its line, while any is inside. */
	UPROPERTY(config, EditAnywhere, Category = "STF")
	bool bReleaseOnCandidateKilled = true;

	/**
	 * The STF has eleven, each in his seat of its order. They come out first to last; a
	 * kicked one goes back to the end of the line and comes out again only after the others.
	 */
	UPROPERTY(config, EditAnywhere, Category = "STF", meta = (ClampMin = "0"))
	int32 MaxOnBoard = 11;

	//~ Voice --------------------------------------------------------------------

	/** His cry when kicked. Empty for now: the black exclamation still goes up, for ScreamMarkSeconds. */
	UPROPERTY(config, EditAnywhere, Category = "Voice")
	TSoftObjectPtr<USoundBase> ScreamSound;

	/** How long the black exclamation of a cry with no sound stays up. */
	UPROPERTY(config, EditAnywhere, Category = "Voice", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float ScreamMarkSeconds = 1.6f;
};

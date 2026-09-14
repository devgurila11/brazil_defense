// Brazil Defense. Where the player is allowed to put the urn, and how that is shown.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Grid/BDGridTypes.h"
#include "BDObjectiveSettings.generated.h"

class UBDPlaceableData;
class USoundBase;

/**
 * The zone of the board the urn may stand in, as an inclusive rectangle of cells, and
 * the look of that zone while the player is holding the urn.
 * Edited in Project Settings > Game > Brazil Defense - Objective.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Objective"))
class BRAZIL_DEFENSE_API UBDObjectiveSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDObjectiveSettings();

	static const UBDObjectiveSettings& Get();

	//~ Zone -----------------------------------------------------------------
	// Inclusive on both ends: MinX..MaxX by MinY..MaxY. The far end of the Esplanada,
	// so every spawn mouth has a real walk ahead of it whatever the player picks.

	/** The urn as a placeable, put in the player's hand when a match starts on a board with no urn. */
	UPROPERTY(config, EditAnywhere, Category = "Objective", meta = (AllowedClasses = "/Script/Brazil_Defense.BDPlaceableData"))
	TSoftObjectPtr<UBDPlaceableData> ObjectivePlaceable;

	/**
	 * Whether the urn is held to the zone below. Off, it goes on any free cell that every
	 * mouth can reach, and the zone only tells the obstacle generator where a goal is
	 * likely to be. Off by default: the player decides how deep the urn sits.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Zone")
	bool bRestrictToZone = false;

	UPROPERTY(config, EditAnywhere, Category = "Zone", meta = (ClampMin = "0", UIMin = "0"))
	int32 MinX = 40;

	UPROPERTY(config, EditAnywhere, Category = "Zone", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxX = 47;

	UPROPERTY(config, EditAnywhere, Category = "Zone", meta = (ClampMin = "0", UIMin = "0"))
	int32 MinY = 6;

	UPROPERTY(config, EditAnywhere, Category = "Zone", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxY = 15;

	//~ Zone drawing, while the urn is the selected piece ----------------------

	/** Fill over every cell of the zone. Translucent, so the board stays readable under it. */
	UPROPERTY(config, EditAnywhere, Category = "Drawing")
	FColor ZoneFillColor = FColor(255, 200, 0, 60);

	/** Outline around the whole zone. */
	UPROPERTY(config, EditAnywhere, Category = "Drawing")
	FColor ZoneBorderColor = FColor(255, 200, 0, 255);

	UPROPERTY(config, EditAnywhere, Category = "Drawing", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ZoneBorderThickness = 8.0f;

	//~ The vote sound ------------------------------------------------------------
	// The urn beeps at every arrival, from where it stands, out in the open: a natural
	// falloff over the board, air absorption, no walls. A horde arriving together does not
	// stack the beep: one play per VoteSoundMinInterval, the rest are dropped.

	/** Played at the urn each time the red count goes up. */
	UPROPERTY(config, EditAnywhere, Category = "Vote Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	TSoftObjectPtr<USoundBase> VoteSound;

	/** Real seconds between two beeps, whatever arrives in between. */
	UPROPERTY(config, EditAnywhere, Category = "Vote Sound", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float VoteSoundMinInterval = 0.4f;

	UPROPERTY(config, EditAnywhere, Category = "Vote Sound", meta = (ClampMin = "0.0", ClampMax = "4.0", UIMin = "0.0", UIMax = "4.0"))
	float VoteSoundVolume = 1.0f;

	/** Up to this far the beep is at full volume. */
	UPROPERTY(config, EditAnywhere, Category = "Vote Sound", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "cm"))
	float VoteSoundInnerRadius = 2000.0f;

	/** Past the inner radius the beep fades to nothing over this distance. */
	UPROPERTY(config, EditAnywhere, Category = "Vote Sound", meta = (ClampMin = "100.0", UIMin = "100.0", ForceUnits = "cm"))
	float VoteSoundFalloffDistance = 30000.0f;

	/** Whether a cell lies inside the zone. Says nothing about the grid: an out of grid cell can be "in the zone". */
	bool IsInZone(const FBDCellCoord& Coord) const;

	/** The middle cell of the zone, rounding down: what stands in for the urn before one is placed. */
	FBDCellCoord GetZoneCenter() const;

	/** Every cell of the zone, row major. */
	void GetZoneCells(TArray<FBDCellCoord>& OutCells) const;
};

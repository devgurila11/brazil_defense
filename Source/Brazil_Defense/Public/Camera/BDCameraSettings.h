// Brazil Defense. Where the match is looked at from.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BDCameraSettings.generated.h"

/**
 * The one camera of a match, as numbers: it stands behind the long side of the board,
 * above it, and looks down at its middle. Derived from the grid, so a board of another
 * size is framed the same way. Edited in Project Settings > Game > Brazil Defense - Camera.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Camera"))
class BRAZIL_DEFENSE_API UBDCameraSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBDCameraSettings();

	static const UBDCameraSettings& Get();

	/** Height of the camera above the grid plane. */
	UPROPERTY(config, EditAnywhere, Category = "Camera", meta = (ClampMin = "100.0", UIMin = "100.0", ForceUnits = "cm"))
	float Height = 28000.0f;

	/** How far the camera looks down, in degrees. 90 is straight down. */
	UPROPERTY(config, EditAnywhere, Category = "Camera", meta = (ClampMin = "10.0", ClampMax = "90.0", UIMin = "10.0", UIMax = "90.0"))
	float PitchDegrees = 58.0f;

	/** Horizontal field of view, in degrees. */
	UPROPERTY(config, EditAnywhere, Category = "Camera", meta = (ClampMin = "20.0", ClampMax = "120.0", UIMin = "20.0", UIMax = "120.0"))
	float FieldOfView = 65.0f;

	/**
	 * Which side of the board the camera stands on: it looks along +Y from below the board
	 * when true (the long X side fills the width of the screen), along +X from the left when false.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Camera")
	bool bLookAlongY = true;

	/** Pulls the point looked at along the viewing direction, as a fraction of the board depth. 0 is the middle. */
	UPROPERTY(config, EditAnywhere, Category = "Camera", meta = (ClampMin = "-0.5", ClampMax = "0.5", UIMin = "-0.5", UIMax = "0.5"))
	float LookAtShift = 0.0f;
};

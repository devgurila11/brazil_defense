// Brazil Defense. Where the match is looked at from.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BDCameraSettings.generated.h"

/**
 * The one camera of a match, as numbers: it starts behind the long side of the board,
 * above it, looking down at its middle, and the player moves it from there: WASD slides
 * the point looked at over the board, the wheel (or Q/E) changes the height. The point
 * looked at never leaves the board and the height stays between MinHeight and Height,
 * so the player can go close to a fence or a shooter but never lose the arena. Derived
 * from the grid, so a board of another size is framed the same way. Edited in Project
 * Settings > Game > Brazil Defense - Camera.
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

	//~ Moving it ----------------------------------------------------------------

	/** The lowest the camera goes above the grid plane: close enough to watch a fence, high enough to keep context. */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "100.0", UIMin = "100.0", ForceUnits = "cm"))
	float MinHeight = 3000.0f;

	/** How far the camera looks down at MinHeight; the pitch slides between this and PitchDegrees with the height. */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "10.0", ClampMax = "90.0", UIMin = "10.0", UIMax = "90.0"))
	float PitchDegreesAtMinHeight = 30.0f;

	/**
	 * How far the view may turn, either way, when the camera is at MinHeight. Zooming out
	 * shrinks the allowance to nothing at Height (see YawLimitExponent), and the view
	 * turns back to the board's side on its own: the edge of the world is never shown
	 * from up high.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "180.0"))
	float MaxYawAtMinHeight = 180.0f;

	/** Shape of the yaw allowance against the height: 1 is linear, higher keeps it wide longer near the ground. */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0.25", ClampMax = "8.0", UIMin = "0.25", UIMax = "8.0"))
	float YawLimitExponent = 2.0f;

	/** Degrees per second while Q or E is held. */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float YawSpeedDegreesPerSecond = 90.0f;

	/** Degrees per pixel of a middle button drag. */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0.01", UIMin = "0.01"))
	float YawDragDegreesPerPixel = 0.3f;

	/** Sideways speed of the point looked at, as a fraction of the current height per second: the same feel high and low. */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float PanSpeedPerHeight = 0.8f;

	/** One notch of the wheel scales the height by this. */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "1.01", ClampMax = "2.0", UIMin = "1.01", UIMax = "2.0"))
	float ZoomStep = 1.2f;

	/** Height change per second while Page Up or Page Down is held, as a fraction of the current height. */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float ZoomSpeedPerHeight = 0.8f;

	/** Seconds the camera takes to settle on a new target: 0 snaps. */
	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0", ForceUnits = "s"))
	float SmoothingSeconds = 0.12f;
};

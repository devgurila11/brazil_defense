// Brazil Defense. Player controller that owns the placement gesture.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BDPlayerController.generated.h"

class ACameraActor;
class UBDPlacementComponent;

/**
 * Hosts UBDPlacementComponent and puts the cursor on screen for it.
 * Deliberately thin: it exists so the placement component has a controller to live on.
 */
UCLASS()
class BRAZIL_DEFENSE_API ABDPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ABDPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Puts the camera back where the match started it. */
	void ResetCamera();

	/** Debug: sets the height and, optionally, the cell looked at and the turn. */
	void DebugSetCamera(float Height, const TOptional<FVector>& LookAt, float Yaw);

	/** The fixed camera of the match, spawned at BeginPlay from UBDCameraSettings and the grid. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense")
	ACameraActor* GetMatchCamera() const { return MatchCamera; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense")
	UBDPlacementComponent* GetPlacementComponent() const { return PlacementComponent; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense")
	TObjectPtr<UBDPlacementComponent> PlacementComponent;

	/** Puts the match camera over the board and looks through it. */
	void SetupMatchCamera();

	/** WASD, Q/E and the wheel move the target; the camera follows it each tick. */
	void UpdateCamera(float DeltaSeconds);

	/** Location and rotation for a point looked at, a height and a turn, by the settings' pitch and side. */
	void CameraPose(const FVector& LookAt, float Height, float Yaw, FVector& OutLocation, FRotator& OutRotation) const;

	/** The direction the camera looks along on the plane, turned by the yaw. */
	FVector CameraForward(float Yaw) const;

	/** How far the view may be turned at a height. */
	float YawLimitAtHeight(float Height) const;

	/** Keeps the point looked at on the board and the height in its band. */
	void ClampCameraTarget();

	void HandleZoomIn();
	void HandleZoomOut();
	void HandleRotatePiece();
	void HandleRotatePieceBack();
	void HandleSelectSlot1();
	void HandleSelectSlot2();
	void HandleSelectSlot3();
	void HandleSelectSlot4();
	void HandleSelectSlot5();
	void HandleSelectSlot6();
	void HandleSelectSlot7();
	void HandleSelectSlot8();
	void HandleSelectSlot9();
	void HandleSelectUrn();
	void HandleResetCamera();

	/** Puts the piece of a palette slot in hand, as the HUD button would. */
	void SelectPaletteSlot(int32 Index);

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> MatchCamera;

	/** Where the camera looks, on the grid plane, how high it is and how far it has turned: what the keys move. */
	FVector CameraTarget = FVector::ZeroVector;
	float CameraHeight = 0.0f;
	float CameraYaw = 0.0f;

	/** Where the match started them, for a reset. */
	FVector HomeTarget = FVector::ZeroVector;
	float HomeHeight = 0.0f;

	/** Board rectangle the target is kept inside, on the grid plane. */
	FVector BoardMin = FVector::ZeroVector;
	FVector BoardMax = FVector::ZeroVector;
};

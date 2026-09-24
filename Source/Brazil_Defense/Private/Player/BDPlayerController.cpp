// Brazil Defense. Player controller that owns the placement gesture.

#include "Player/BDPlayerController.h"

#include "BDLog.h"
#include "Camera/BDCameraSettings.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Components/InputComponent.h"
#include "Grid/BDGridSubsystem.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementComponent.h"
#include "Placement/BDPlacementSettings.h"
#include "Objective/BDObjectiveSettings.h"
#include "UI/BDSettingsSave.h"
#include "UI/BDSettingsSubsystem.h"
#include "Engine/GameInstance.h"

ABDPlayerController::ABDPlayerController()
{
	// The whole game is played by pointing at the board.
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	PlacementComponent = CreateDefaultSubobject<UBDPlacementComponent>(TEXT("Placement"));
}

void ABDPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetupMatchCamera();
}

void ABDPlayerController::SetupMatchCamera()
{
	UWorld* World = GetWorld();
	const UBDGridSubsystem* Grid = UBDGridSubsystem::Get(this);
	if (World == nullptr || Grid == nullptr || Grid->GetSizeX() <= 0 || Grid->GetSizeY() <= 0)
	{
		UE_LOG(LogBDGrid, Warning, TEXT("No grid to frame: the match camera is not set up."));
		return;
	}

	// The camera is derived from the board, never placed by hand: the same numbers frame
	// any board. It starts off one long side, above the plane, looking at the middle.
	const UBDCameraSettings& Settings = UBDCameraSettings::Get();
	const float Cell = Grid->GetCellSize();
	const FVector Origin = Grid->GetOrigin();
	BoardMin = Origin;
	BoardMax = Origin + FVector(Grid->GetSizeX() * Cell, Grid->GetSizeY() * Cell, 0.0f);
	const FVector Center = (BoardMin + BoardMax) * 0.5f;

	const FVector Forward = Settings.bLookAlongY ? FVector::YAxisVector : FVector::XAxisVector;
	const float Depth = (Settings.bLookAlongY ? Grid->GetSizeY() : Grid->GetSizeX()) * Cell;
	HomeTarget = Center + Forward * (Settings.LookAtShift * Depth);
	HomeHeight = Settings.Height;
	CameraTarget = HomeTarget;
	CameraHeight = HomeHeight;

	CameraYaw = 0.0f;

	FVector Location;
	FRotator Rotation;
	CameraPose(CameraTarget, CameraHeight, CameraYaw, Location, Rotation);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MatchCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, Params);
	if (MatchCamera == nullptr)
	{
		UE_LOG(LogBDGrid, Error, TEXT("The match camera failed to spawn."));
		return;
	}

	if (UCameraComponent* Camera = MatchCamera->GetCameraComponent())
	{
		Camera->SetFieldOfView(Settings.FieldOfView);
		Camera->SetConstraintAspectRatio(false);
	}

	SetViewTarget(MatchCamera);

	// Every world sound is heard from the camera, not from a pawn there is none of: the
	// zoom is what brings the urn, and later the shooting, near or far.
	if (UCameraComponent* Camera = MatchCamera->GetCameraComponent())
	{
		SetAudioListenerOverride(Camera, FVector::ZeroVector, FRotator::ZeroRotator);
	}

	UE_LOG(LogBDGrid, Log, TEXT("Match camera at %s looking at %s (height %.0f, pitch %.0f, fov %.0f); WASD moves it, the wheel changes the height between %.0f and %.0f, Q/E or a middle drag turn it near the ground."),
		*Location.ToCompactString(), *CameraTarget.ToCompactString(), Settings.Height, Settings.PitchDegrees, Settings.FieldOfView, Settings.MinHeight, Settings.Height);
}

FVector ABDPlayerController::CameraForward(const float Yaw) const
{
	const UBDCameraSettings& Settings = UBDCameraSettings::Get();
	const FVector Base = Settings.bLookAlongY ? FVector::YAxisVector : FVector::XAxisVector;
	return Base.RotateAngleAxis(Yaw, FVector::UpVector);
}

float ABDPlayerController::YawLimitAtHeight(const float Height) const
{
	const UBDCameraSettings& Settings = UBDCameraSettings::Get();
	const float MinHeight = FMath::Min(Settings.MinHeight, Settings.Height);
	const float Alpha = Settings.Height > MinHeight ? FMath::Clamp((Height - MinHeight) / (Settings.Height - MinHeight), 0.0f, 1.0f) : 1.0f;
	return Settings.MaxYawAtMinHeight * FMath::Pow(1.0f - Alpha, Settings.YawLimitExponent);
}

void ABDPlayerController::CameraPose(const FVector& LookAt, const float Height, const float Yaw, FVector& OutLocation, FRotator& OutRotation) const
{
	const UBDCameraSettings& Settings = UBDCameraSettings::Get();
	const FVector Forward = CameraForward(Yaw);

	// Steeper from high up, flatter close in: the overview reads the maze, the close look
	// reads the action.
	const float MinHeight = FMath::Min(Settings.MinHeight, Settings.Height);
	const float Alpha = Settings.Height > MinHeight ? FMath::Clamp((Height - MinHeight) / (Settings.Height - MinHeight), 0.0f, 1.0f) : 1.0f;
	const float Pitch = FMath::Clamp(FMath::Lerp(Settings.PitchDegreesAtMinHeight, Settings.PitchDegrees, Alpha), 10.0f, 90.0f);
	const float Back = Height / FMath::Tan(FMath::DegreesToRadians(Pitch));

	OutLocation = LookAt - Forward * Back + FVector(0.0f, 0.0f, Height);
	OutRotation = (LookAt - OutLocation).Rotation();
}

void ABDPlayerController::ClampCameraTarget()
{
	const UBDCameraSettings& Settings = UBDCameraSettings::Get();
	CameraTarget.X = FMath::Clamp(CameraTarget.X, BoardMin.X, BoardMax.X);
	CameraTarget.Y = FMath::Clamp(CameraTarget.Y, BoardMin.Y, BoardMax.Y);
	CameraTarget.Z = BoardMin.Z;
	CameraHeight = FMath::Clamp(CameraHeight, FMath::Min(Settings.MinHeight, Settings.Height), Settings.Height);

	// The turn is allowed by the height: zooming out narrows it and brings the view back.
	const float YawLimit = YawLimitAtHeight(CameraHeight);
	CameraYaw = FMath::Clamp(CameraYaw, -YawLimit, YawLimit);
}

void ABDPlayerController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
}

void ABDPlayerController::UpdateCamera(const float DeltaSeconds)
{
	if (MatchCamera == nullptr)
	{
		return;
	}

	// Polled rather than bound: the keys are fixed and the board has no other use for
	// them, so there is nothing an input asset would add. Real seconds, so a frozen or
	// slowed match still lets the player look around.
	const float RealDelta = FMath::Min(static_cast<float>(FApp::GetDeltaTime()), 0.1f);
	const UBDCameraSettings& Settings = UBDCameraSettings::Get();

	// Q/E and a middle button drag turn the view; how far is settled in the clamp.
	float Turn = 0.0f;
	if (IsInputKeyDown(EKeys::E)) { Turn += 1.0f; }
	if (IsInputKeyDown(EKeys::Q)) { Turn -= 1.0f; }
	CameraYaw += Turn * Settings.YawSpeedDegreesPerSecond * RealDelta;
	if (IsInputKeyDown(EKeys::MiddleMouseButton))
	{
		float DeltaX = 0.0f;
		float DeltaY = 0.0f;
		GetInputMouseDelta(DeltaX, DeltaY);
		CameraYaw += DeltaX * Settings.YawDragDegreesPerPixel;
	}

	// The keys follow the view: forward is where the camera looks along the plane, right
	// is to its right, unless the player asked for the sideways pair the other way round.
	const FVector Forward = CameraForward(CameraYaw);
	const UBDSettingsSubsystem* PlayerSettings = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UBDSettingsSubsystem>() : nullptr;
	const bool bInvert = PlayerSettings != nullptr && PlayerSettings->Get().bInvertCameraSideways;
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal() * (bInvert ? -1.0f : 1.0f);

	FVector Pan = FVector::ZeroVector;
	if (IsInputKeyDown(EKeys::W)) { Pan += Forward; }
	if (IsInputKeyDown(EKeys::S)) { Pan -= Forward; }
	if (IsInputKeyDown(EKeys::D)) { Pan += Right; }
	if (IsInputKeyDown(EKeys::A)) { Pan -= Right; }
	if (!Pan.IsNearlyZero())
	{
		CameraTarget += Pan.GetSafeNormal() * Settings.PanSpeedPerHeight * CameraHeight * RealDelta;
	}

	float Zoom = 0.0f;
	if (IsInputKeyDown(EKeys::PageDown)) { Zoom -= 1.0f; }
	if (IsInputKeyDown(EKeys::PageUp)) { Zoom += 1.0f; }
	if (Zoom != 0.0f)
	{
		CameraHeight *= 1.0f + Zoom * Settings.ZoomSpeedPerHeight * RealDelta;
	}

	ClampCameraTarget();

	FVector Location;
	FRotator Rotation;
	CameraPose(CameraTarget, CameraHeight, CameraYaw, Location, Rotation);

	// Eased towards the pose, so a wheel notch glides instead of jumping.
	if (Settings.SmoothingSeconds > KINDA_SMALL_NUMBER)
	{
		const float Alpha = FMath::Clamp(RealDelta / Settings.SmoothingSeconds, 0.0f, 1.0f);
		Location = FMath::Lerp(MatchCamera->GetActorLocation(), Location, Alpha);
		Rotation = FMath::Lerp(MatchCamera->GetActorRotation(), Rotation, Alpha);
	}
	MatchCamera->SetActorLocationAndRotation(Location, Rotation);
}

void ABDPlayerController::ResetCamera()
{
	CameraTarget = HomeTarget;
	CameraHeight = HomeHeight;
	CameraYaw = 0.0f;
}

void ABDPlayerController::DebugSetCamera(const float Height, const TOptional<FVector>& LookAt, const float Yaw)
{
	CameraHeight = Height;
	if (LookAt.IsSet())
	{
		CameraTarget = LookAt.GetValue();
	}
	CameraYaw = Yaw;
	ClampCameraTarget();
	UE_LOG(LogBDGrid, Log, TEXT("Camera set: height %.0f, target %s, yaw %.0f."), CameraHeight, *CameraTarget.ToCompactString(), CameraYaw);
}

namespace BDCameraDebug
{
	static void ExecCamera(const TArray<FString>& Args, UWorld* World)
	{
		ABDPlayerController* Controller = World != nullptr ? Cast<ABDPlayerController>(World->GetFirstPlayerController()) : nullptr;
		const UBDGridSubsystem* Grid = UBDGridSubsystem::Get(World);
		if (Controller == nullptr || Args.Num() < 1)
		{
			UE_LOG(LogBDGrid, Error, TEXT("Usage: BD.Camera.Set <height cm> [cell x] [cell y] [yaw]"));
			return;
		}
		TOptional<FVector> LookAt;
		if (Args.Num() >= 3 && Grid != nullptr)
		{
			LookAt = Grid->CellToWorld(FBDCellCoord(FCString::Atoi(*Args[1]), FCString::Atoi(*Args[2])));
		}
		Controller->DebugSetCamera(FCString::Atof(*Args[0]), LookAt, Args.Num() >= 4 ? FCString::Atof(*Args[3]) : 0.0f);
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdCamera(
		TEXT("BD.Camera.Set"),
		TEXT("BD.Camera.Set <height cm> [cell x] [cell y] [yaw]: puts the match camera at a height over a cell, turned by yaw."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecCamera));
}

void ABDPlayerController::HandleRotatePiece()
{
	if (PlacementComponent != nullptr)
	{
		PlacementComponent->RotateSelection(true);
	}
}

void ABDPlayerController::HandleRotatePieceBack()
{
	if (PlacementComponent != nullptr)
	{
		PlacementComponent->RotateSelection(false);
	}
}

void ABDPlayerController::HandleZoomIn()
{
	CameraHeight /= UBDCameraSettings::Get().ZoomStep;
	ClampCameraTarget();
}

void ABDPlayerController::HandleZoomOut()
{
	CameraHeight *= UBDCameraSettings::Get().ZoomStep;
	ClampCameraTarget();
}

void ABDPlayerController::HandleResetCamera()
{
	ResetCamera();
}

//~ Palette keys ----------------------------------------------------------------------

void ABDPlayerController::SelectPaletteSlot(const int32 Index)
{
	const UBDPlacementSettings& Settings = UBDPlacementSettings::Get();
	if (PlacementComponent == nullptr || !Settings.Palette.IsValidIndex(Index))
	{
		return;
	}

	// Through the same door as the build bar: a key must not hand over a piece the bar
	// shows greyed out, like a character with no platform slot to stand on.
	if (UBDPlaceableData* Data = Settings.Palette[Index].LoadSynchronous())
	{
		PlacementComponent->TakeIntoHand(Data);
	}
}

void ABDPlayerController::HandleSelectUrn()
{
	if (PlacementComponent == nullptr)
	{
		return;
	}

	if (UBDPlaceableData* Urn = UBDObjectiveSettings::Get().ObjectivePlaceable.LoadSynchronous())
	{
		PlacementComponent->TakeIntoHand(Urn);
	}
}

void ABDPlayerController::HandleSelectSlot1() { SelectPaletteSlot(0); }
void ABDPlayerController::HandleSelectSlot2() { SelectPaletteSlot(1); }
void ABDPlayerController::HandleSelectSlot3() { SelectPaletteSlot(2); }
void ABDPlayerController::HandleSelectSlot4() { SelectPaletteSlot(3); }
void ABDPlayerController::HandleSelectSlot5() { SelectPaletteSlot(4); }
void ABDPlayerController::HandleSelectSlot6() { SelectPaletteSlot(5); }
void ABDPlayerController::HandleSelectSlot7() { SelectPaletteSlot(6); }
void ABDPlayerController::HandleSelectSlot8() { SelectPaletteSlot(7); }
void ABDPlayerController::HandleSelectSlot9() { SelectPaletteSlot(8); }

void ABDPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Bound here rather than in the component: this is the first moment InputComponent
	// is guaranteed to exist.
	if (PlacementComponent != nullptr)
	{
		PlacementComponent->BindInput(InputComponent);
	}

	if (InputComponent == nullptr)
	{
		return;
	}

	// Fixed keys, no assets: the wheel for the height, Home for the start view, R to turn
	// the piece in hand (Shift+R the other way), the number row for the palette and U for
	// the urn. WASD, Q/E, Page Up/Down and the middle button are polled in the tick.
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &ABDPlayerController::HandleZoomIn);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &ABDPlayerController::HandleZoomOut);
	InputComponent->BindKey(EKeys::Home, IE_Pressed, this, &ABDPlayerController::HandleResetCamera);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ABDPlayerController::HandleRotatePiece);
	InputComponent->BindKey(FInputChord(EKeys::R, /*bShift*/ true, false, false, false), IE_Pressed, this, &ABDPlayerController::HandleRotatePieceBack);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ABDPlayerController::HandleSelectSlot1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ABDPlayerController::HandleSelectSlot2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ABDPlayerController::HandleSelectSlot3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ABDPlayerController::HandleSelectSlot4);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ABDPlayerController::HandleSelectSlot5);
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ABDPlayerController::HandleSelectSlot6);
	InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ABDPlayerController::HandleSelectSlot7);
	InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ABDPlayerController::HandleSelectSlot8);
	InputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &ABDPlayerController::HandleSelectSlot9);
	InputComponent->BindKey(EKeys::U, IE_Pressed, this, &ABDPlayerController::HandleSelectUrn);
}

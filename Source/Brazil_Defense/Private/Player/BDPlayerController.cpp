// Brazil Defense. Player controller that owns the placement gesture.

#include "Player/BDPlayerController.h"

#include "BDLog.h"
#include "Camera/BDCameraSettings.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Grid/BDGridSubsystem.h"
#include "Placement/BDPlacementComponent.h"

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
	// any board. It stands off one long side, above the plane, and looks at the middle.
	const UBDCameraSettings& Settings = UBDCameraSettings::Get();
	const float Cell = Grid->GetCellSize();
	const FVector Origin = Grid->GetOrigin();
	const FVector Center = Origin + FVector(Grid->GetSizeX() * Cell * 0.5f, Grid->GetSizeY() * Cell * 0.5f, 0.0f);

	const FVector Forward = Settings.bLookAlongY ? FVector::YAxisVector : FVector::XAxisVector;
	const float Depth = (Settings.bLookAlongY ? Grid->GetSizeY() : Grid->GetSizeX()) * Cell;
	const FVector LookAt = Center + Forward * (Settings.LookAtShift * Depth);

	const float Pitch = FMath::Clamp(Settings.PitchDegrees, 10.0f, 90.0f);
	const float Back = Settings.Height / FMath::Tan(FMath::DegreesToRadians(Pitch));
	const FVector Location = LookAt - Forward * Back + FVector(0.0f, 0.0f, Settings.Height);
	const FRotator Rotation = (LookAt - Location).Rotation();

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

	UE_LOG(LogBDGrid, Log, TEXT("Match camera at %s looking at %s (height %.0f, pitch %.0f, fov %.0f)."),
		*Location.ToCompactString(), *LookAt.ToCompactString(), Settings.Height, Pitch, Settings.FieldOfView);
}

void ABDPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Bound here rather than in the component: this is the first moment InputComponent
	// is guaranteed to exist.
	if (PlacementComponent != nullptr)
	{
		PlacementComponent->BindInput(InputComponent);
	}
}

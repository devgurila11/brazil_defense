// Brazil Defense. What is drawn straight on the screen over the board: the creeps' health.

#include "UI/BDMatchHUD.h"

#include "BDLog.h"
#include "Camera/PlayerCameraManager.h"
#include "Enemy/BDEnemyBase.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UI/BDUISettings.h"
#include "Wave/BDWaveSubsystem.h"

void ABDMatchHUD::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogBDUI, Log, TEXT("Match HUD up: creep health bars drawn on the canvas."));
}

void ABDMatchHUD::DrawHUD()
{
	Super::DrawHUD();

	const UWorld* World = GetWorld();
	const UBDWaveSubsystem* Waves = World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr;
	if (Canvas == nullptr || Waves == nullptr || PlayerOwner == nullptr || PlayerOwner->PlayerCameraManager == nullptr)
	{
		return;
	}

	const UBDUISettings& Settings = UBDUISettings::Get();
	const FVector CameraRight = FRotationMatrix(PlayerOwner->PlayerCameraManager->GetCameraRotation()).GetUnitAxis(EAxis::Y);
	const FVector HalfWidth = CameraRight * (Settings.CreepBarWorldWidth * 0.5f);
	const FVector Lift(0.0f, 0.0f, Settings.CreepBarWorldHeight);
	int32 Drawn = 0;
	float FirstX = 0.0f, FirstY = 0.0f, FirstW = 0.0f;

	for (const ABDEnemyBase* Enemy : Waves->GetLivingEnemiesRef())
	{
		if (Enemy == nullptr || Enemy->IsCandidate() || Enemy->GetMaxHealth() <= 0.0f)
		{
			continue;
		}
		const float Fraction = FMath::Clamp(Enemy->GetCurrentHealth() / Enemy->GetMaxHealth(), 0.0f, 1.0f);
		if (Fraction >= 1.0f || Fraction <= 0.0f)
		{
			// Untouched or gone: no bar.
			continue;
		}

		// The bar's ends in the world, projected: the zoom sizes it, and too small is nothing.
		const FVector Center = Enemy->GetActorLocation() + Lift;
		const FVector Left = Canvas->Project(Center - HalfWidth);
		const FVector Right = Canvas->Project(Center + HalfWidth);
		if (Left.Z <= 0.0f || Right.Z <= 0.0f)
		{
			continue;
		}
		const float Width = Right.X - Left.X;
		if (Width < Settings.CreepBarMinPixels)
		{
			continue;
		}
		const float Height = FMath::Clamp(Width * Settings.CreepBarAspect, 2.0f, 12.0f);
		const float X = Left.X;
		const float Y = Left.Y - Height * 0.5f;
		if (Drawn == 0) { FirstX = X; FirstY = Y; FirstW = Width; }

		// Dark ground, then the health from green to red as it goes.
		const FLinearColor Fill = FMath::Lerp(FLinearColor(0.85f, 0.15f, 0.12f), FLinearColor(0.20f, 0.80f, 0.25f), Fraction);
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), X - 1.0f, Y - 1.0f, Width + 2.0f, Height + 2.0f);
		DrawRect(Fill, X, Y, Width * Fraction, Height);
		++Drawn;
	}

	static double LastReport = 0.0;
	if (Drawn > 0 && FPlatformTime::Seconds() - LastReport > 1.0)
	{
		LastReport = FPlatformTime::Seconds();
		UE_LOG(LogBDUI, Verbose, TEXT("Creep bars: %d drawn, first at %.0f,%.0f width %.0f."), Drawn, FirstX, FirstY, FirstW);
	}
}

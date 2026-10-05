// Brazil Defense. What is drawn straight on the screen over the board: the creeps' health, the evolution stars.

#include "UI/BDMatchHUD.h"

#include "BDLog.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Enemy/BDEnemyBase.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Palace/BDAgent.h"
#include "Palace/BDPalace.h"
#include "Palace/BDPalaceData.h"
#include "Platform/BDPlatformComponent.h"
#include "Tower/BDTowerBase.h"
#include "Tower/BDTowerData.h"
#include "UObject/UObjectIterator.h"
#include "UI/BDUISettings.h"
#include "Wave/BDWaveSubsystem.h"

void ABDMatchHUD::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogBDUI, Log, TEXT("Match HUD up: creep health bars and palace stars drawn on the canvas."));
}

void ABDMatchHUD::DrawHUD()
{
	Super::DrawHUD();

	DrawPalaceStars();
	DrawPieceStars();
	DrawAgentBars();
	DrawCreepBars();
}

void ABDMatchHUD::DrawCreepBars()
{
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

void ABDMatchHUD::DrawPalaceStars()
{
	UWorld* World = GetWorld();
	if (Canvas == nullptr || World == nullptr)
	{
		return;
	}

	for (TActorIterator<ABDPalace> It(World); It; ++It)
	{
		const ABDPalace* Palace = *It;
		if (Palace->IsHidden())
		{
			// Lifted and travelling with the cursor: the stars come back with it.
			continue;
		}
		DrawStarRow(Palace->GetStarsAnchor(), Palace->GetPalaceLevel(), UBDPalaceData::MaxLevel, 1.0f);
	}
}

void ABDMatchHUD::DrawPieceStars()
{
	UWorld* World = GetWorld();
	if (Canvas == nullptr || World == nullptr)
	{
		return;
	}

	const UBDUISettings& Settings = UBDUISettings::Get();
	const auto TopOf = [](const AActor& Actor) { return Actor.GetComponentsBoundingBox(/*bNonColliding*/ true).Max.Z; };

	// A star is an evolution bought, not the level (GetPieceStarsFilled). A tower on the
	// ground by its own level. One on a platform evolves in step with the rest of the
	// block, so the block's row speaks for it instead of one row a shooter.
	for (TActorIterator<ABDTowerBase> It(World); It; ++It)
	{
		const ABDTowerBase* Tower = *It;
		if (Tower->IsHidden() || Tower->IsOnPlatform())
		{
			continue;
		}
		const FVector Location = Tower->GetActorLocation();
		DrawStarRow(FVector(Location.X, Location.Y, TopOf(*Tower) + Settings.PieceStarLift),
			GetPieceStarsFilled(Tower->GetTowerLevel()), UBDTowerData::MaxLevels, Settings.PieceStarScale);
	}

	for (TObjectIterator<UBDPlatformComponent> It; It; ++It)
	{
		const UBDPlatformComponent* Platform = *It;
		const AActor* Stand = Platform->GetOwner();
		if (Stand == nullptr || Stand->GetWorld() != World || Stand->IsHidden() || Platform->Slots.Num() == 0)
		{
			continue;
		}
		// Over the highest of the deck and the shooters on it.
		float Top = TopOf(*Stand);
		for (int32 Slot = 0; Slot < Platform->Slots.Num(); ++Slot)
		{
			if (const ABDTowerBase* Occupant = Platform->GetSlotOccupant(Slot))
			{
				Top = FMath::Max(Top, TopOf(*Occupant));
			}
		}
		const FVector Location = Stand->GetActorLocation();
		DrawStarRow(FVector(Location.X, Location.Y, Top + Settings.PieceStarLift),
			GetPieceStarsFilled(Platform->GetBlockLevel()), UBDTowerData::MaxLevels, Settings.PieceStarScale);
	}
}

bool ABDMatchHUD::DrawStarRow(const FVector& Anchor, const int32 Filled, const int32 Count, const float Scale)
{
	const UBDUISettings& Settings = UBDUISettings::Get();
	const FVector CameraLocation = PlayerOwner != nullptr && PlayerOwner->PlayerCameraManager != nullptr
		? PlayerOwner->PlayerCameraManager->GetCameraLocation()
		: FVector::ZeroVector;

	// Far from the camera the row fades out: in the overview the rows would run into each
	// other, and nothing there is decided piece by piece.
	const float Opacity = Settings.GetPalaceStarOpacity(FVector::Dist(CameraLocation, Anchor));
	if (Opacity <= KINDA_SMALL_NUMBER)
	{
		return false;
	}
	FLinearColor Color = Settings.PalaceStarColor;
	Color.A *= Opacity;

	// Projected, so the row always faces the camera; behind it, nothing.
	const FVector Screen = Canvas->Project(Anchor);
	if (Screen.Z <= 0.0f)
	{
		return false;
	}

	const float StarHeight = FMath::Max(4.0f, Canvas->ClipY * Settings.PalaceStarHeightFraction * Scale);
	const float Step = StarHeight * (1.0f + Settings.PalaceStarGap);
	const int32 Full = FMath::Clamp(Filled, 0, Count);
	const float FirstX = Screen.X - Step * (Count - 1) * 0.5f;
	for (int32 Star = 0; Star < Count; ++Star)
	{
		DrawStar(FVector2D(FirstX + Step * Star, Screen.Y), StarHeight * 0.5f, Star < Full, Color);
	}
	return true;
}

void ABDMatchHUD::DrawStar(const FVector2D& Center, const float Radius, const bool bFilled, const FLinearColor& Color)
{
	// Ten corners round the center, tips and notches in turn, the first tip straight up.
	const float InnerRadius = Radius * 0.42f;
	FVector2D Corners[10];
	for (int32 Corner = 0; Corner < 10; ++Corner)
	{
		const float Angle = -UE_HALF_PI + Corner * (UE_PI / 5.0f);
		const float Reach = (Corner % 2 == 0) ? Radius : InnerRadius;
		Corners[Corner] = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Reach;
	}

	if (bFilled)
	{
		TArray<FCanvasUVTri> Triangles;
		Triangles.Reserve(10);
		for (int32 Corner = 0; Corner < 10; ++Corner)
		{
			FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
			Triangle.V0_Pos = Center;
			Triangle.V1_Pos = Corners[Corner];
			Triangle.V2_Pos = Corners[(Corner + 1) % 10];
			Triangle.V0_Color = Triangle.V1_Color = Triangle.V2_Color = Color;
		}
		FCanvasTriangleItem Fill(Triangles, GWhiteTexture);
		Fill.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Fill);
	}

	// A dark edge under the colour, so an empty star reads on grass and on concrete alike.
	// Thin next to the star, or an outline closes over the inside and an empty star reads full.
	const float Thickness = FMath::Clamp(Radius * 0.08f, 1.0f, 2.5f);
	// Lines drawn translucent, so the fade reaches the outline as well as the fill.
	const FLinearColor Shadow(0.0f, 0.0f, 0.0f, 0.7f * Color.A);
	const auto DrawOutline = [this, &Corners](const FLinearColor& LineColor, const float LineThickness)
	{
		for (int32 Corner = 0; Corner < 10; ++Corner)
		{
			FCanvasLineItem Line(Corners[Corner], Corners[(Corner + 1) % 10]);
			Line.SetColor(LineColor);
			Line.LineThickness = LineThickness;
			Line.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Line);
		}
	};
	DrawOutline(Shadow, Thickness + 1.5f);
	DrawOutline(Color, Thickness);
}

void ABDMatchHUD::DrawAgentBars()
{
	UWorld* World = GetWorld();
	if (Canvas == nullptr || World == nullptr || PlayerOwner == nullptr || PlayerOwner->PlayerCameraManager == nullptr)
	{
		return;
	}

	const UBDUISettings& Settings = UBDUISettings::Get();
	const FVector CameraRight = FRotationMatrix(PlayerOwner->PlayerCameraManager->GetCameraRotation()).GetUnitAxis(EAxis::Y);
	const FVector HalfWidth = CameraRight * (Settings.AgentBarWorldWidth * 0.5f);

	for (TActorIterator<ABDAgent> It(World); It; ++It)
	{
		const ABDAgent* Agent = *It;

		// Projected ends, as the creep bars: the zoom sizes it, and always facing the camera.
		const FVector Center = Agent->GetBarAnchor();
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
		const float Height = FMath::Clamp(Width * Settings.CreepBarAspect, 3.0f, 14.0f);
		const float X = Left.X;
		const float Y = Left.Y - Height * 0.5f;

		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), X - 1.0f, Y - 1.0f, Width + 2.0f, Height + 2.0f);
		DrawRect(Agent->IsAsleep() ? Settings.AgentRestBarColor : Settings.AgentBarColor, X, Y, Width * Agent->GetBarFraction(), Height);
	}
}

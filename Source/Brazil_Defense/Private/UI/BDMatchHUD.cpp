// Brazil Defense. What is drawn straight on the screen over the board: the creeps' health, the evolution stars.

#include "UI/BDMatchHUD.h"

#include "Audio/BDSpeechMarks.h"
#include "BDLog.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Enemy/BDEnemyBase.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
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
	// Last, over the bars: who is talking reads over everything else.
	DrawSpeechMarks();
}

void ABDMatchHUD::DrawSpeechMarks()
{
	UBDSpeechMarkSubsystem* Marks = UBDSpeechMarkSubsystem::Get(this);
	if (Canvas == nullptr || Marks == nullptr || PlayerOwner == nullptr || PlayerOwner->PlayerCameraManager == nullptr)
	{
		return;
	}
	const TArray<UBDSpeechMarkSubsystem::FMark>& Live = Marks->GetLiveMarks();
	if (Live.IsEmpty())
	{
		return;
	}

	const UBDUISettings& Settings = UBDUISettings::Get();
	const FVector CameraUp = FRotationMatrix(PlayerOwner->PlayerCameraManager->GetCameraRotation()).GetUnitAxis(EAxis::Z);
	const double Now = FPlatformTime::Seconds();
	for (const UBDSpeechMarkSubsystem::FMark& Mark : Live)
	{
		const AActor* Speaker = Mark.Speaker.Get();
		if (Speaker == nullptr || Speaker->IsHidden())
		{
			continue;
		}

		// Over his head, and over the bar he may show: a creep's health, the Agent's patrol.
		const FVector Location = Speaker->GetActorLocation();
		const FBox Box = Speaker->GetComponentsBoundingBox(/*bNonColliding*/ true);
		float HeadZ = Box.IsValid ? Box.Max.Z : Location.Z;
		if (Speaker->IsA<ABDEnemyBase>())
		{
			HeadZ = FMath::Max(HeadZ, Location.Z + Settings.CreepBarWorldHeight);
		}
		else if (const ABDAgent* Agent = Cast<ABDAgent>(Speaker))
		{
			HeadZ = FMath::Max(HeadZ, Agent->GetBarAnchor().Z);
		}
		const FVector Foot(Location.X, Location.Y, HeadZ + Settings.SpeechMarkLift);
		const FVector Top = Foot + CameraUp * Settings.SpeechMarkWorldHeight;

		// Projected: the zoom sizes it, and too small is nothing.
		const FVector FootOnScreen = Canvas->Project(Foot);
		const FVector TopOnScreen = Canvas->Project(Top);
		if (FootOnScreen.Z <= 0.0f || TopOnScreen.Z <= 0.0f)
		{
			continue;
		}
		const FVector2D FootPoint(FootOnScreen.X, FootOnScreen.Y);
		const FVector2D TopPoint(TopOnScreen.X, TopOnScreen.Y);
		const float Height = FVector2D::Distance(FootPoint, TopPoint);
		if (Height < Settings.SpeechMarkMinPixels)
		{
			continue;
		}

		// The pop: up and back within the pulse, from the first word.
		const float Age = static_cast<float>(Now - Mark.StartTime);
		float Scale = 1.0f;
		if (Settings.SpeechMarkPulseSeconds > 0.0f && Age < Settings.SpeechMarkPulseSeconds)
		{
			Scale += Settings.SpeechMarkPulseScale * FMath::Sin(UE_PI * Age / Settings.SpeechMarkPulseSeconds);
		}
		DrawExclamation((FootPoint + TopPoint) * 0.5f, Height * Scale, Settings.GetSpeechMarkColor(Mark.Side));
	}
}

void ABDMatchHUD::DrawExclamation(const FVector2D& Center, const float Height, const FLinearColor& Color)
{
	const UBDUISettings& Settings = UBDUISettings::Get();

	// A dark colour gets a light rim and a white halo, or it would be lost at night.
	const bool bDark = Color.GetLuminance() < 0.15f;
	const FLinearColor Rim = bDark ? FLinearColor(1.0f, 1.0f, 1.0f, 0.85f) : FLinearColor(0.0f, 0.0f, 0.0f, 0.75f);
	const FLinearColor Halo = bDark ? FLinearColor::White : Color;

	// The stroke tapers down to a gap and the dot; all of it Height tall around Center.
	const float Half = Height * 0.5f;
	const float DotRadius = Height * 0.11f;
	const float StrokeTop = Center.Y - Half;
	const float StrokeBottom = Center.Y + Half - DotRadius * 2.0f - Height * 0.10f;
	const FVector2D DotCenter(Center.X, Center.Y + Half - DotRadius);
	const float TopHalfWidth = Height * 0.13f;
	const float BottomHalfWidth = Height * 0.075f;

	TArray<FCanvasUVTri> Triangles;
	const auto AddTriangle = [&Triangles](const FVector2D& A, const FVector2D& B, const FVector2D& C,
		const FLinearColor& ColorA, const FLinearColor& ColorB, const FLinearColor& ColorC)
	{
		FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
		Triangle.V0_Pos = A;
		Triangle.V1_Pos = B;
		Triangle.V2_Pos = C;
		Triangle.V0_Color = ColorA;
		Triangle.V1_Color = ColorB;
		Triangle.V2_Color = ColorC;
	};
	const auto AddDisc = [&AddTriangle](const FVector2D& At, const float Radius, const FLinearColor& Inner, const FLinearColor& Outer)
	{
		constexpr int32 Segments = 20;
		for (int32 Segment = 0; Segment < Segments; ++Segment)
		{
			const float A0 = Segment * UE_TWO_PI / Segments;
			const float A1 = (Segment + 1) * UE_TWO_PI / Segments;
			AddTriangle(At, At + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Radius, At + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Radius,
				Inner, Outer, Outer);
		}
	};
	const auto AddGlyph = [&](const float Grow, const FLinearColor& GlyphColor)
	{
		const FVector2D TopLeft(Center.X - TopHalfWidth - Grow, StrokeTop - Grow);
		const FVector2D TopRight(Center.X + TopHalfWidth + Grow, StrokeTop - Grow);
		const FVector2D BottomLeft(Center.X - BottomHalfWidth - Grow, StrokeBottom + Grow);
		const FVector2D BottomRight(Center.X + BottomHalfWidth + Grow, StrokeBottom + Grow);
		AddTriangle(TopLeft, TopRight, BottomRight, GlyphColor, GlyphColor, GlyphColor);
		AddTriangle(TopLeft, BottomRight, BottomLeft, GlyphColor, GlyphColor, GlyphColor);
		AddDisc(DotCenter, DotRadius + Grow, GlyphColor, GlyphColor);
	};

	// The halo first, fading out from the middle; then the rim, a little larger; then the colour.
	const float Glow = Settings.SpeechMarkGlow;
	if (Glow > 0.0f)
	{
		AddDisc(Center, Height * 0.85f, FLinearColor(Halo.R, Halo.G, Halo.B, Glow), FLinearColor(Halo.R, Halo.G, Halo.B, 0.0f));
	}
	AddGlyph(FMath::Clamp(Height * 0.035f, 1.0f, 3.0f), Rim);
	AddGlyph(0.0f, Color);

	FCanvasTriangleItem Item(Triangles, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
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

FVector ABDMatchHUD::GetPlatformStarAnchor(const AActor& Stand)
{
	// Over the deck and its floors, with room for the crew: never the crew itself, who are
	// actors of their own and so outside the stand's bounds.
	const UBDUISettings& Settings = UBDUISettings::Get();
	const FVector Location = Stand.GetActorLocation();
	const float Top = Stand.GetComponentsBoundingBox(/*bNonColliding*/ true).Max.Z;
	return FVector(Location.X, Location.Y, Top + Settings.PlatformCrewClearance + Settings.PieceStarLift);
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
		DrawStarRow(GetPlatformStarAnchor(*Stand),
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

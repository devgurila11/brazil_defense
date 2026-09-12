// Brazil Defense. The numbers of the match on screen, for testing, until there is a HUD.

#include "Debug/BDDebugScoreboard.h"

#include "CanvasItem.h"
#include "Debug/DebugDrawService.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Match/BDMatchManager.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDDebugScoreboardPrivate
{
	/** Off by default: a Play without the test setup shows the board and nothing else. */
	static int32 GScoreboard = 0;

	static FAutoConsoleVariableRef CVarScoreboard(
		TEXT("BD.HUD.Debug"),
		GScoreboard,
		TEXT("1 draws the test scoreboard in the top left corner: votes, wave, countdown, mouths of the wave, creeps out. 0 to hide."),
		ECVF_Cheat);

	/** Same show flags as the grid labels: PIE and standalone viewports, plus Simulate. */
	static const TCHAR* const ObservedShowFlags[] = { TEXT("Game"), TEXT("Editor") };

	static constexpr float Margin = 24.0f;
	static constexpr float LineGap = 6.0f;
	static constexpr float TextScale = 1.6f;

	/** The red counter starts at this many characters of the blue one, whatever the numbers. */
	static const TCHAR* const BlueColumnMeasure = TEXT("AZUL 000000    ");

	static const FLinearColor BlueColor(0.30f, 0.60f, 1.0f);
	static const FLinearColor RedColor(1.0f, 0.32f, 0.32f);
	static const FLinearColor TextColor(0.95f, 0.95f, 0.95f);
	static const FLinearColor MutedColor(0.7f, 0.7f, 0.7f);

	static const TCHAR* const Separator = TEXT("  \u00B7  ");
}

void UBDDebugScoreboard::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	for (const TCHAR* ShowFlagName : BDDebugScoreboardPrivate::ObservedShowFlags)
	{
		CanvasDrawHandles.Add(UDebugDrawService::Register(
			ShowFlagName,
			FDebugDrawDelegate::CreateUObject(this, &UBDDebugScoreboard::Draw)));
	}

	FreezeTimerVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("BD.Match.FreezeTimer"));
}

void UBDDebugScoreboard::Deinitialize()
{
	for (const FDelegateHandle& Handle : CanvasDrawHandles)
	{
		UDebugDrawService::Unregister(Handle);
	}

	CanvasDrawHandles.Reset();

	Super::Deinitialize();
}

bool UBDDebugScoreboard::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

float UBDDebugScoreboard::DrawLine(UCanvas& Canvas, const UFont& Font, const float X, const float Y,
	const FString& Text, const FLinearColor& Color) const
{
	using namespace BDDebugScoreboardPrivate;

	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), &Font, Color);
	Item.Scale = FVector2D(TextScale, TextScale);
	Item.EnableShadow(FLinearColor::Black);
	Canvas.DrawItem(Item);

	return Font.GetMaxCharHeight() * TextScale + LineGap;
}

void UBDDebugScoreboard::Draw(UCanvas* Canvas, APlayerController* PlayerController)
{
	using namespace BDDebugScoreboardPrivate;

	const UWorld* World = GetWorld();
	if (GScoreboard == 0 || Canvas == nullptr || Canvas->SceneView == nullptr || World == nullptr)
	{
		return;
	}

	// The delegate fires for every viewport being rendered, including the PIE one while
	// the editor world is still loaded. Only draw into the world we were given.
	const FSceneInterface* Scene = Canvas->SceneView->Family != nullptr ? Canvas->SceneView->Family->Scene : nullptr;
	if (Scene == nullptr || Scene->GetWorld() != World)
	{
		return;
	}

	const UFont* Font = GEngine != nullptr ? GEngine->GetMediumFont() : nullptr;
	if (Font == nullptr)
	{
		return;
	}

	const ABDMatchManager* Match = ABDMatchManager::Get(World);
	const UBDWaveSubsystem* Waves = World->GetSubsystem<UBDWaveSubsystem>();

	float Y = Margin;
	if (Match == nullptr)
	{
		DrawLine(*Canvas, *Font, Margin, Y, TEXT("SEM PARTIDA"), MutedColor);
		return;
	}

	// Line 1: the two counters, each in its color. The red one sits on a fixed column so
	// it does not slide as the blue number grows.
	const float RedColumn = Margin + Font->GetStringSize(BlueColumnMeasure) * TextScale;
	DrawLine(*Canvas, *Font, RedColumn, Y, FString::Printf(TEXT("VERMELHO %d"), Match->GetVotesRed()), RedColor);
	Y += DrawLine(*Canvas, *Font, Margin, Y, FString::Printf(TEXT("AZUL %d"), Match->GetVotesBlue()), BlueColor);

	// Line 2: where the match stands. The wave number is the last one sent; between
	// waves the countdown to the next one runs, and a frozen one says so rather than
	// looking stuck.
	FString WaveLine;
	switch (Match->GetPhase())
	{
	case EBDMatchPhase::Setup:
		WaveLine = TEXT("Preparando o tabuleiro");
		break;
	case EBDMatchPhase::Building:
		WaveLine = FString::Printf(TEXT("Onda %d%spr\u00F3xima em %ds"),
			Match->GetCurrentWave(), Separator, FMath::CeilToInt(Match->GetTimeUntilNextWave()));
		if (FreezeTimerVariable != nullptr && FreezeTimerVariable->GetInt() != 0)
		{
			WaveLine += TEXT(" (congelado)");
		}
		break;
	case EBDMatchPhase::WaveActive:
		WaveLine = FString::Printf(TEXT("Onda %d%sem curso"), Match->GetCurrentWave(), Separator);
		if (Waves != nullptr && Waves->GetWaveSpawnsRemaining() > 0)
		{
			WaveLine += FString::Printf(TEXT("%sa sair: %d"), Separator, Waves->GetWaveSpawnsRemaining());
		}
		break;
	case EBDMatchPhase::Defeat:
		WaveLine = FString::Printf(TEXT("DERROTA na onda %d"), Match->GetCurrentWave());
		break;
	case EBDMatchPhase::Victory:
		WaveLine = FString::Printf(TEXT("VIT\u00D3RIA na onda %d"), Match->GetCurrentWave());
		break;
	default:
		break;
	}
	Y += DrawLine(*Canvas, *Font, Margin, Y, WaveLine, TextColor);

	// Line 3: the mouths the wave drew and what is out on the board.
	FString Mouths;
	if (Waves != nullptr)
	{
		for (const int32 Index : Waves->GetActiveSpawnPoints())
		{
			Mouths += Mouths.IsEmpty() ? FString::FromInt(Index) : FString::Printf(TEXT(", %d"), Index);
		}
	}
	if (Mouths.IsEmpty())
	{
		Mouths = TEXT("\u2014");
	}
	Y += DrawLine(*Canvas, *Font, Margin, Y, FString::Printf(TEXT("Bocas: %s%svivos: %d"),
		*Mouths, Separator, Waves != nullptr ? Waves->GetLivingEnemyCount() : 0), TextColor);

	// The candidate line ("CANDIDATO  HP x/y") and the frozen count ("APURACAO CONGELADA
	// Ns") go here once the candidate exists. There is no such system on the board yet.
}

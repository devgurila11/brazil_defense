// Brazil Defense. The numbers of the match over the board, and what a selection offers.

#include "UI/BDHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Candidate/BDCandidateSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Enemy/BDCandidate.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Match/BDMatchManager.h"
#include "Misc/App.h"
#include "Objective/BDObjectiveSettings.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementComponent.h"
#include "Placement/BDPlacementSettings.h"
#include "Tower/BDTowerBase.h"
#include "Tower/BDTowerData.h"
#include "UI/BDUISettings.h"
#include "UI/BDUISubsystem.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDHUDPrivate
{
	static constexpr int32 ScoreFontSize = 28;
	static constexpr int32 LineFontSize = 18;
	static constexpr int32 SmallFontSize = 14;
	static constexpr int32 NoticeFontSize = 26;
	static constexpr float Margin = 16.0f;
	static constexpr float PanelWidth = 320.0f;
	static constexpr float CandidateBarWidth = 480.0f;
	static constexpr float CandidateBarHeight = 18.0f;
	static const float Speeds[] = { 1.0f, 2.0f, 4.0f };


	/** The refusal keys, in enum order. */
	static const TCHAR* const RefusalKeys[] = {
		TEXT("Refusal.None"), TEXT("Refusal.NoSelection"), TEXT("Refusal.NotHoveringGrid"), TEXT("Refusal.MatchRefused"),
		TEXT("Refusal.NoBudgetLeft"), TEXT("Refusal.OffGrid"), TEXT("Refusal.CellTaken"), TEXT("Refusal.EdgeOnBorder"),
		TEXT("Refusal.EdgeTaken"), TEXT("Refusal.WouldBlockPath"), TEXT("Refusal.ObjectiveMissing"), TEXT("Refusal.ObjectiveOutOfZone"),
		TEXT("Refusal.SlotTaken"), TEXT("Refusal.TowerCannotGoOnSlot"), TEXT("Refusal.CharacterNeedsPlatform"), TEXT("Refusal.CannotAffordMove") };
}

//~ Lookups -------------------------------------------------------------------------

ABDMatchManager* UBDHUDWidget::GetMatch() const
{
	return ABDMatchManager::Get(GetWorld());
}

UBDWaveSubsystem* UBDHUDWidget::GetWaves() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDWaveSubsystem>() : nullptr;
}

UBDCandidateSubsystem* UBDHUDWidget::GetCandidates() const
{
	const UWorld* World = GetWorld();
	return World != nullptr ? World->GetSubsystem<UBDCandidateSubsystem>() : nullptr;
}

UBDPlacementComponent* UBDHUDWidget::GetPlacement() const
{
	const APlayerController* Controller = GetOwningPlayer();
	return Controller != nullptr ? Controller->FindComponentByClass<UBDPlacementComponent>() : nullptr;
}

//~ Tree ---------------------------------------------------------------------------

void UBDHUDWidget::BuildTree()
{
	using namespace BDHUDPrivate;

	// The canvas covers the whole screen and must not take a single click from the
	// board: only the buttons and the panels are solid.
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Canvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	//~ Top center: scoreboard, wave line, speed, mouths.
	UVerticalBox* Top = MakeColumn();

	UHorizontalBox* Scores = MakeRow();
	BlueScore = MakeText(ScoreFontSize, ColorBlue);
	RedScore = MakeText(ScoreFontSize, ColorRed);
	Scores->AddChildToHorizontalBox(BlueScore);
	UHorizontalBoxSlot* RedSlot = Scores->AddChildToHorizontalBox(RedScore);
	RedSlot->SetPadding(FMargin(32.0f, 0.0f, 0.0f, 0.0f));
	UVerticalBoxSlot* ScoresSlot = Top->AddChildToVerticalBox(Scores);
	ScoresSlot->SetHorizontalAlignment(HAlign_Center);

	WaveLine = MakeText(LineFontSize);
	WaveLine->SetJustification(ETextJustify::Center);
	Top->AddChildToVerticalBox(WaveLine);

	UHorizontalBox* SpeedRow = MakeRow();
	SpeedLabel = MakeText(SmallFontSize, ColorMuted);
	UHorizontalBoxSlot* SpeedLabelSlot = SpeedRow->AddChildToHorizontalBox(SpeedLabel);
	SpeedLabelSlot->SetVerticalAlignment(VAlign_Center);
	SpeedLabelSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
	for (int32 Index = 0; Index < 3; ++Index)
	{
		SpeedButtons[Index] = MakeButton(SpeedButtonLabels[Index], SmallFontSize);
		UHorizontalBoxSlot* ButtonSlot = SpeedRow->AddChildToHorizontalBox(SpeedButtons[Index]);
		ButtonSlot->SetPadding(FMargin(2.0f, 0.0f));
	}
	SpeedButtons[0]->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleSpeed1);
	SpeedButtons[1]->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleSpeed2);
	SpeedButtons[2]->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleSpeed4);
	UVerticalBoxSlot* SpeedSlot = Top->AddChildToVerticalBox(SpeedRow);
	SpeedSlot->SetHorizontalAlignment(HAlign_Center);
	SpeedSlot->SetPadding(FMargin(0.0f, 4.0f));

	MouthsLine = MakeText(SmallFontSize, ColorMuted);
	MouthsLine->SetJustification(ETextJustify::Center);
	Top->AddChildToVerticalBox(MouthsLine);

	//~ Candidate, under the top block: bar, line, notice, frozen count.
	UVerticalBox* CandidateColumn = MakeColumn();

	CandidateBox = MakeBox(ColorPanel, 8.0f);
	UVerticalBox* CandidateInner = MakeColumn();
	CandidateLine = MakeText(LineFontSize, ColorRed);
	CandidateLine->SetJustification(ETextJustify::Center);
	CandidateInner->AddChildToVerticalBox(CandidateLine);
	CandidateBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	CandidateBar->SetFillColorAndOpacity(ColorRed);
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarBox->SetWidthOverride(CandidateBarWidth);
	BarBox->SetHeightOverride(CandidateBarHeight);
	BarBox->AddChild(CandidateBar);
	UVerticalBoxSlot* BarSlot = CandidateInner->AddChildToVerticalBox(BarBox);
	BarSlot->SetHorizontalAlignment(HAlign_Center);
	CandidateBox->AddChild(CandidateInner);
	UVerticalBoxSlot* CandidateBoxSlot = CandidateColumn->AddChildToVerticalBox(CandidateBox);
	CandidateBoxSlot->SetHorizontalAlignment(HAlign_Center);

	CandidateNotice = MakeText(NoticeFontSize, ColorRed);
	CandidateNotice->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* NoticeSlot = CandidateColumn->AddChildToVerticalBox(CandidateNotice);
	NoticeSlot->SetHorizontalAlignment(HAlign_Center);
	NoticeSlot->SetPadding(FMargin(0.0f, 8.0f));

	FrozenLine = MakeText(LineFontSize, ColorHighlight);
	FrozenLine->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* FrozenSlot = CandidateColumn->AddChildToVerticalBox(FrozenLine);
	FrozenSlot->SetHorizontalAlignment(HAlign_Center);

	UVerticalBoxSlot* CandidateColumnSlot = Top->AddChildToVerticalBox(CandidateColumn);
	CandidateColumnSlot->SetHorizontalAlignment(HAlign_Center);
	CandidateColumnSlot->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));

	UCanvasPanelSlot* TopSlot = Canvas->AddChildToCanvas(Top);
	TopSlot->SetAnchors(FAnchors(0.5f, 0.0f));
	TopSlot->SetAlignment(FVector2D(0.5f, 0.0f));
	TopSlot->SetAutoSize(true);
	TopSlot->SetPosition(FVector2D(0.0f, Margin));

	//~ Right: the defender panel and the placement panel, stacked.
	UVerticalBox* Right = MakeColumn();

	DefenderBox = MakeBox(ColorPanel, 12.0f);
	UVerticalBox* DefenderColumn = MakeColumn();
	DefenderName = MakeText(LineFontSize, ColorHighlight);
	DefenderColumn->AddChildToVerticalBox(DefenderName);
	DefenderStats = MakeText(SmallFontSize);
	DefenderStats->SetAutoWrapText(true);
	DefenderColumn->AddChildToVerticalBox(DefenderStats);
	UpgradeButton = MakeButton(UpgradeLabel, SmallFontSize);
	UpgradeButton->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleUpgrade);
	UVerticalBoxSlot* UpgradeSlot = DefenderColumn->AddChildToVerticalBox(UpgradeButton);
	UpgradeSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 2.0f));
	UpgradeResult = MakeText(SmallFontSize);
	UpgradeResult->SetAutoWrapText(true);
	DefenderColumn->AddChildToVerticalBox(UpgradeResult);
	SellButton = MakeButton(SellLabel, SmallFontSize);
	SellButton->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleSell);
	UVerticalBoxSlot* SellSlot = DefenderColumn->AddChildToVerticalBox(SellButton);
	SellSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 2.0f));
	SellResult = MakeText(SmallFontSize);
	SellResult->SetAutoWrapText(true);
	DefenderColumn->AddChildToVerticalBox(SellResult);
	DefenderBox->AddChild(DefenderColumn);
	Right->AddChildToVerticalBox(DefenderBox);

	PlacementBox = MakeBox(ColorPanel, 12.0f);
	UVerticalBox* PlacementColumn = MakeColumn();
	PlacementName = MakeText(LineFontSize, ColorHighlight);
	PlacementColumn->AddChildToVerticalBox(PlacementName);
	PlacementInfo = MakeText(SmallFontSize);
	PlacementInfo->SetAutoWrapText(true);
	PlacementColumn->AddChildToVerticalBox(PlacementInfo);
	PlacementRefusal = MakeText(SmallFontSize, ColorRed);
	PlacementRefusal->SetAutoWrapText(true);
	PlacementColumn->AddChildToVerticalBox(PlacementRefusal);
	MoveResult = MakeText(SmallFontSize);
	MoveResult->SetAutoWrapText(true);
	PlacementColumn->AddChildToVerticalBox(MoveResult);
	UButton* CancelButton = MakeButton(CancelLabel, SmallFontSize);
	CancelButton->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleCancelSelection);
	UVerticalBoxSlot* CancelSlot = PlacementColumn->AddChildToVerticalBox(CancelButton);
	CancelSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	PlacementBox->AddChild(PlacementColumn);
	UVerticalBoxSlot* PlacementBoxSlot = Right->AddChildToVerticalBox(PlacementBox);
	PlacementBoxSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));

	USizeBox* RightBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RightBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RightBox->SetWidthOverride(PanelWidth);
	RightBox->AddChild(Right);

	UCanvasPanelSlot* RightSlot = Canvas->AddChildToCanvas(RightBox);
	RightSlot->SetAnchors(FAnchors(1.0f, 0.0f));
	RightSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	RightSlot->SetAutoSize(true);
	RightSlot->SetPosition(FVector2D(-Margin, Margin));

	//~ Center: the end of the match, won or lost. Solid: nothing on the board is clickable
	// under it, since the board is frozen anyway.
	EndBox = MakeBox(ColorPanel, 24.0f);
	UVerticalBox* EndColumn = MakeColumn();
	EndTitle = MakeText(NoticeFontSize, ColorHighlight);
	EndTitle->SetJustification(ETextJustify::Center);
	EndColumn->AddChildToVerticalBox(EndTitle);
	EndLine = MakeText(LineFontSize);
	EndLine->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* EndLineSlot = EndColumn->AddChildToVerticalBox(EndLine);
	EndLineSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 16.0f));
	EndlessButton = MakeButton(EndlessLabel, LineFontSize);
	EndlessButton->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleEndless);
	UVerticalBoxSlot* EndlessSlot = EndColumn->AddChildToVerticalBox(EndlessButton);
	EndlessSlot->SetHorizontalAlignment(HAlign_Center);
	EndlessSlot->SetPadding(FMargin(0.0f, 4.0f));
	LoadButton = MakeButton(LoadLabel, LineFontSize);
	LoadButton->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleLoad);
	UVerticalBoxSlot* LoadSlot = EndColumn->AddChildToVerticalBox(LoadButton);
	LoadSlot->SetHorizontalAlignment(HAlign_Center);
	LoadSlot->SetPadding(FMargin(0.0f, 4.0f));
	UButton* EndMenuButton = MakeButton(EndMenuLabel, LineFontSize);
	EndMenuButton->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleReturnToMenu);
	UVerticalBoxSlot* EndMenuSlot = EndColumn->AddChildToVerticalBox(EndMenuButton);
	EndMenuSlot->SetHorizontalAlignment(HAlign_Center);
	EndMenuSlot->SetPadding(FMargin(0.0f, 4.0f));
	EndBox->AddChild(EndColumn);

	UCanvasPanelSlot* EndSlot = Canvas->AddChildToCanvas(EndBox);
	EndSlot->SetAnchors(FAnchors(0.5f, 0.5f));
	EndSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	EndSlot->SetAutoSize(true);
	EndSlot->SetPosition(FVector2D::ZeroVector);

	//~ Left: what can be built. The urn first, then the palette in its order; the number
	// keys follow the same order.
	BuildBox = MakeBox(ColorPanel, 12.0f);
	UVerticalBox* BuildColumn = MakeColumn();
	BuildTitle = MakeText(SmallFontSize, ColorMuted);
	BuildColumn->AddChildToVerticalBox(BuildTitle);
	UrnButton = MakeButton(UrnLabel, SmallFontSize);
	UrnButton->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleBuildUrn);
	UVerticalBoxSlot* UrnSlot = BuildColumn->AddChildToVerticalBox(UrnButton);
	UrnSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	UrnSlot->SetHorizontalAlignment(HAlign_Fill);
	for (const TSoftObjectPtr<UBDPlaceableData>& Entry : UBDPlacementSettings::Get().Palette)
	{
		if (PaletteData.Num() >= MaxPaletteButtons)
		{
			break;
		}
		if (UBDPlaceableData* Data = Entry.LoadSynchronous())
		{
			PaletteData.Add(Data);
		}
	}
	for (int32 Index = 0; Index < PaletteData.Num(); ++Index)
	{
		BuildButtons[Index] = MakeButton(BuildLabels[Index], SmallFontSize);
		UVerticalBoxSlot* BuildSlot = BuildColumn->AddChildToVerticalBox(BuildButtons[Index]);
		BuildSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		BuildSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	if (PaletteData.Num() > 0) { BuildButtons[0]->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleBuild0); }
	if (PaletteData.Num() > 1) { BuildButtons[1]->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleBuild1); }
	if (PaletteData.Num() > 2) { BuildButtons[2]->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleBuild2); }
	if (PaletteData.Num() > 3) { BuildButtons[3]->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleBuild3); }
	if (PaletteData.Num() > 4) { BuildButtons[4]->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleBuild4); }
	if (PaletteData.Num() > 5) { BuildButtons[5]->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleBuild5); }
	if (PaletteData.Num() > 6) { BuildButtons[6]->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleBuild6); }
	if (PaletteData.Num() > 7) { BuildButtons[7]->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleBuild7); }
	if (PaletteData.Num() > 8) { BuildButtons[8]->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleBuild8); }
	BuildBox->AddChild(BuildColumn);

	USizeBox* BuildSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BuildSizeBox->SetWidthOverride(PanelWidth * 0.75f);
	BuildSizeBox->AddChild(BuildBox);
	UCanvasPanelSlot* BuildSlotOnCanvas = Canvas->AddChildToCanvas(BuildSizeBox);
	BuildSlotOnCanvas->SetAnchors(FAnchors(0.0f, 0.0f));
	BuildSlotOnCanvas->SetAlignment(FVector2D(0.0f, 0.0f));
	BuildSlotOnCanvas->SetAutoSize(true);
	BuildSlotOnCanvas->SetPosition(FVector2D(Margin, Margin + 48.0f));

	//~ Top left: the game menu and the save, side by side.
	UHorizontalBox* CornerRow = MakeRow();
	UButton* MenuButton = MakeButton(MenuLabel, SmallFontSize);
	MenuButton->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleMenu);
	CornerRow->AddChildToHorizontalBox(MenuButton);
	SaveButton = MakeButton(SaveLabel, SmallFontSize);
	SaveButton->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleSave);
	UHorizontalBoxSlot* SaveSlot = CornerRow->AddChildToHorizontalBox(SaveButton);
	SaveSlot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
	UCanvasPanelSlot* MenuSlot = Canvas->AddChildToCanvas(CornerRow);
	MenuSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	MenuSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	MenuSlot->SetAutoSize(true);
	MenuSlot->SetPosition(FVector2D(Margin, Margin));

	WidgetTree->RootWidget = Canvas;

	// The board is played with the mouse under this: the HUD never takes the keyboard.
	SetIsFocusable(false);
	CandidateBox->SetVisibility(ESlateVisibility::Collapsed);
	CandidateNotice->SetVisibility(ESlateVisibility::Collapsed);
	FrozenLine->SetVisibility(ESlateVisibility::Collapsed);
	DefenderBox->SetVisibility(ESlateVisibility::Collapsed);
	PlacementBox->SetVisibility(ESlateVisibility::Collapsed);
	EndBox->SetVisibility(ESlateVisibility::Collapsed);
}

void UBDHUDWidget::RefreshTexts()
{
	SpeedLabel->SetText(Loc(TEXT("HUD.Speed")));
	CandidateNotice->SetText(Loc(TEXT("HUD.Candidate.Notice")));
	CancelLabel->SetText(Loc(TEXT("HUD.Placement.Cancel")));
	MenuLabel->SetText(Loc(TEXT("HUD.Menu")));
	EndlessLabel->SetText(Loc(TEXT("HUD.End.Endless")));
	EndMenuLabel->SetText(Loc(TEXT("HUD.End.MainMenu")));
	LoadLabel->SetText(Loc(TEXT("HUD.End.Load")));
	BuildTitle->SetText(Loc(TEXT("HUD.Build")));
	UpdateSaveButton();
	UpdateBuildPanel();

	// Everything else carries numbers and is written on tick, in the current language.
	UpdateScoreboard();
	UpdateWaveLine();
	UpdateSpeedButtons();
	UpdateMouths();
	UpdateEndPanel();
}

//~ Lifetime -------------------------------------------------------------------------

void UBDHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindMatch();
}

void UBDHUDWidget::NativeDestruct()
{
	if (ABDMatchManager* Match = BoundMatch.Get())
	{
		Match->OnVotesChanged.Remove(VotesChangedHandle);
		Match->OnPhaseChanged.Remove(PhaseChangedHandle);
		Match->OnWaveStarted.Remove(WaveStartedHandle);
	}
	BoundMatch.Reset();

	Super::NativeDestruct();
}

void UBDHUDWidget::BindMatch()
{
	if (BoundMatch.IsValid())
	{
		return;
	}

	ABDMatchManager* Match = GetMatch();
	if (Match == nullptr)
	{
		return;
	}

	VotesChangedHandle = Match->OnVotesChanged.AddUObject(this, &UBDHUDWidget::HandleVotesChanged);
	PhaseChangedHandle = Match->OnPhaseChanged.AddUObject(this, &UBDHUDWidget::HandlePhaseChanged);
	WaveStartedHandle = Match->OnWaveStarted.AddUObject(this, &UBDHUDWidget::HandleWaveStarted);
	BoundMatch = Match;

	UpdateScoreboard();
	UpdateWaveLine();
	UpdateSpeedButtons();
	UpdateMouths();
	UpdateEndPanel();
}

void UBDHUDWidget::HandleVotesChanged(const int32 Blue, const int32 Red)
{
	UpdateScoreboard();
}

void UBDHUDWidget::HandlePhaseChanged(const EBDMatchPhase NewPhase)
{
	UpdateWaveLine();
	UpdateMouths();
	UpdateEndPanel();
	UpdateSaveButton();
}

void UBDHUDWidget::HandleWaveStarted(const int32 Wave)
{
	UpdateWaveLine();
	UpdateMouths();
}

void UBDHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The match manager is spawned by the game mode, possibly after this widget went up.
	BindMatch();

	UpdateWaveLine();
	UpdateDefenderPanel();
	UpdatePlacementPanel();
	UpdateCandidate(static_cast<float>(FApp::GetDeltaTime()));
	UpdateBuildPanel();
}

//~ Always visible -------------------------------------------------------------------

void UBDHUDWidget::UpdateScoreboard()
{
	const ABDMatchManager* Match = GetMatch();
	FFormatNamedArguments Args;
	Args.Add(TEXT("Votes"), Match != nullptr ? Match->GetVotesBlue() : 0);
	BlueScore->SetText(BDLoc::Format(TEXT("HUD.Score.Blue"), Args));
	Args[TEXT("Votes")] = FFormatArgumentValue(Match != nullptr ? Match->GetVotesRed() : 0);
	RedScore->SetText(BDLoc::Format(TEXT("HUD.Score.Red"), Args));
}

void UBDHUDWidget::UpdateWaveLine()
{
	const ABDMatchManager* Match = GetMatch();
	if (Match == nullptr)
	{
		WaveLine->SetText(Loc(TEXT("HUD.Wave.NoMatch")));
		return;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Wave"), Match->GetCurrentWave());
	Args.Add(TEXT("Seconds"), FMath::CeilToInt(Match->GetTimeUntilNextWave()));

	FText Text;
	switch (Match->GetPhase())
	{
	case EBDMatchPhase::Setup:
		Text = Loc(TEXT("HUD.Wave.Setup"));
		break;
	case EBDMatchPhase::Building:
	{
		Text = BDLoc::Format(TEXT("HUD.Wave.Building"), Args);
		static const IConsoleVariable* FreezeTimer = IConsoleManager::Get().FindConsoleVariable(TEXT("BD.Match.FreezeTimer"));
		if (FreezeTimer != nullptr && FreezeTimer->GetInt() != 0)
		{
			Text = FText::Format(FText::FromString(TEXT("{0} {1}")), Text, Loc(TEXT("HUD.Wave.Frozen")));
		}
		break;
	}
	case EBDMatchPhase::WaveActive:
	{
		Text = BDLoc::Format(TEXT("HUD.Wave.Active"), Args);
		const UBDWaveSubsystem* Waves = GetWaves();
		if (Waves != nullptr && Waves->GetWaveSpawnsRemaining() > 0)
		{
			Args.Add(TEXT("Count"), Waves->GetWaveSpawnsRemaining());
			Text = FText::Format(FText::FromString(TEXT("{0} {1}")), Text, BDLoc::Format(TEXT("HUD.Wave.ToSend"), Args));
		}
		break;
	}
	case EBDMatchPhase::Defeat:
		Text = BDLoc::Format(TEXT("HUD.Wave.Defeat"), Args);
		break;
	case EBDMatchPhase::Victory:
		Text = BDLoc::Format(TEXT("HUD.Wave.Victory"), Args);
		break;
	default:
		break;
	}

	WaveLine->SetText(Text);
}

void UBDHUDWidget::UpdateSpeedButtons()
{
	using namespace BDHUDPrivate;

	const ABDMatchManager* Match = GetMatch();
	const float Current = Match != nullptr ? Match->GetGameSpeed() : 1.0f;

	for (int32 Index = 0; Index < 3; ++Index)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Speed"), FMath::RoundToInt(Speeds[Index]));
		const bool bCurrent = FMath::IsNearlyEqual(Speeds[Index], Current);
		SpeedButtonLabels[Index]->SetText(BDLoc::Format(TEXT("HUD.Speed.Button"), Args));
		SpeedButtons[Index]->SetBackgroundColor(bCurrent ? ColorButton : ColorButtonIdle);
		SpeedButtonLabels[Index]->SetColorAndOpacity(FSlateColor(bCurrent ? ColorPanelDark : ColorText));
	}
}

void UBDHUDWidget::UpdateMouths()
{
	const UBDWaveSubsystem* Waves = GetWaves();
	FString Mouths;
	if (Waves != nullptr)
	{
		for (const int32 Index : Waves->GetActiveSpawnPoints())
		{
			Mouths += Mouths.IsEmpty() ? FString::FromInt(Index) : FString::Printf(TEXT(", %d"), Index);
		}
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Mouths"), Mouths.IsEmpty() ? Loc(TEXT("HUD.Mouths.None")) : FText::FromString(Mouths));
	MouthsLine->SetText(BDLoc::Format(TEXT("HUD.Mouths"), Args));
}

//~ Candidate -------------------------------------------------------------------------

void UBDHUDWidget::UpdateCandidate(const float RealDeltaSeconds)
{
	const UBDCandidateSubsystem* Candidates = GetCandidates();
	ABDCandidate* Candidate = Candidates != nullptr ? Candidates->GetCandidate() : nullptr;

	// The notice goes up the frame a candidate appears and comes down on its own.
	if (Candidate != nullptr && Candidate != LastCandidate.Get())
	{
		NoticeRemaining = UBDUISettings::Get().CandidateNoticeSeconds;
	}
	LastCandidate = Candidate;

	NoticeRemaining = FMath::Max(0.0f, NoticeRemaining - RealDeltaSeconds);
	CandidateNotice->SetVisibility(NoticeRemaining > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (Candidate != nullptr && Candidate->GetMaxHealth() > 0.0f)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Health"), FMath::CeilToInt(Candidate->GetCurrentHealth()));
		Args.Add(TEXT("MaxHealth"), FMath::CeilToInt(Candidate->GetMaxHealth()));
		CandidateLine->SetText(BDLoc::Format(TEXT("HUD.Candidate.Health"), Args));
		CandidateBar->SetPercent(FMath::Clamp(Candidate->GetCurrentHealth() / Candidate->GetMaxHealth(), 0.0f, 1.0f));
		CandidateBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		CandidateBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Candidates != nullptr && Candidates->IsCountFrozen())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Seconds"), FMath::CeilToInt(Candidates->GetPauseRemaining()));
		FrozenLine->SetText(BDLoc::Format(TEXT("HUD.Candidate.Frozen"), Args));
		FrozenLine->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		FrozenLine->SetVisibility(ESlateVisibility::Collapsed);
	}
}

//~ Panels ---------------------------------------------------------------------------

FText UBDHUDWidget::ResultText(const int32 Delta, bool& bOutInverts) const
{
	const ABDMatchManager* Match = GetMatch();
	bOutInverts = false;
	if (Match == nullptr)
	{
		return FText::GetEmpty();
	}

	const int32 Blue = Match->GetVotesBlue();
	bOutInverts = Delta < 0 && Match->WouldInvertScoreboard(-Delta);

	FFormatNamedArguments Args;
	Args.Add(TEXT("Blue"), Blue);
	Args.Add(TEXT("After"), Blue + Delta);
	Args.Add(TEXT("Red"), Match->GetVotesRed());
	return BDLoc::Format(bOutInverts ? TEXT("HUD.Result.Inverts") : TEXT("HUD.Result"), Args);
}

void UBDHUDWidget::SetResultLine(UTextBlock* Line, const int32 Delta)
{
	bool bInverts = false;
	Line->SetText(ResultText(Delta, bInverts));
	Line->SetColorAndOpacity(FSlateColor(bInverts ? ColorRed : ColorMuted));
}

void UBDHUDWidget::UpdateDefenderPanel()
{
	const UBDPlacementComponent* Placement = GetPlacement();
	ABDTowerBase* Tower = Placement != nullptr ? Placement->GetSelectedDefender() : nullptr;
	const ABDMatchManager* Match = GetMatch();

	if (Tower == nullptr || Tower->GetData() == nullptr || Match == nullptr)
	{
		DefenderBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	DefenderBox->SetVisibility(ESlateVisibility::Visible);

	const UBDTowerData* Data = Tower->GetData();
	const FBDTowerLevel* Level = Tower->GetCurrentLevel();

	FFormatNamedArguments Args;
	Args.Add(TEXT("Name"), BDLoc::PieceName(Data));
	Args.Add(TEXT("Level"), Tower->GetTowerLevel());
	Args.Add(TEXT("Damage"), FMath::RoundToInt(Tower->GetEffectiveDamage()));
	Args.Add(TEXT("Range"), FText::AsNumber(Tower->GetEffectiveRangeCells(), &FNumberFormattingOptions().SetMaximumFractionalDigits(1)));
	Args.Add(TEXT("FireRate"), FText::AsNumber(Level != nullptr ? Level->FireRate : 0.0f, &FNumberFormattingOptions().SetMaximumFractionalDigits(2)));
	DefenderName->SetText(BDLoc::Format(TEXT("HUD.Defender.Name"), Args));
	DefenderStats->SetText(BDLoc::Format(TEXT("HUD.Defender.Stats"), Args));

	// Upgrade: the cost, and the scoreboard it leaves.
	FString Reason;
	const bool bCanUpgrade = Tower->CanUpgrade(Reason);
	if (Tower->IsMaxLevel())
	{
		UpgradeLabel->SetText(Loc(TEXT("HUD.Defender.MaxLevel")));
		UpgradeResult->SetText(FText::GetEmpty());
	}
	else
	{
		const int32 Cost = Tower->GetUpgradeCost();
		Args.Add(TEXT("Cost"), Cost);
		Args.Add(TEXT("NextLevel"), Tower->GetTowerLevel() + 1);
		Args.Add(TEXT("NextDamage"), FMath::RoundToInt(Tower->GetDamageAtNextLevel()));
		UpgradeLabel->SetText(BDLoc::Format(TEXT("HUD.Defender.Upgrade"), Args));
		SetResultLine(UpgradeResult, -Cost);
	}
	UpgradeButton->SetIsEnabled(bCanUpgrade);

	// Sell: what comes back, and the scoreboard it leaves.
	const UBDPlaceableData* Placeable = Placement->FindPlaceableOfActor(Tower);
	const int32 Refund = Placeable != nullptr ? Match->GetSellRefund(Placeable->GetPieceKind(), Placeable->GetBuildCost()) : 0;
	Args.Add(TEXT("Refund"), Refund);
	SellLabel->SetText(BDLoc::Format(TEXT("HUD.Defender.Sell"), Args));
	SetResultLine(SellResult, Refund);
	SellButton->SetIsEnabled(Placeable != nullptr && Match->CanRemove(Placeable->GetPieceKind()));
}

FText UBDHUDWidget::RefusalText(const EBDPlacementRefusal Refusal) const
{
	using namespace BDHUDPrivate;

	const int32 Index = static_cast<int32>(Refusal);
	return Index >= 0 && Index < UE_ARRAY_COUNT(RefusalKeys) ? Loc(RefusalKeys[Index]) : FText::GetEmpty();
}

void UBDHUDWidget::UpdatePlacementPanel()
{
	const UBDPlacementComponent* Placement = GetPlacement();
	const UBDPlaceableData* Selection = Placement != nullptr ? Placement->GetCurrentSelection() : nullptr;
	const ABDMatchManager* Match = GetMatch();

	if (Selection == nullptr)
	{
		PlacementBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	PlacementBox->SetVisibility(ESlateVisibility::Visible);

	FFormatNamedArguments Args;
	Args.Add(TEXT("Name"), BDLoc::PieceName(Selection));
	Args.Add(TEXT("Cost"), Selection->GetBuildCost());
	if (Selection->bOccupiesEdge)
	{
		Args.Add(TEXT("Footprint"), FText::AsNumber(FMath::Max(1, Selection->SegmentLength)));
		PlacementInfo->SetText(BDLoc::Format(TEXT("HUD.Placement.InfoEdge"), Args));
	}
	else
	{
		const FIntPoint Footprint = Placement->GetEffectiveFootprint();
		Args.Add(TEXT("Footprint"), FText::FromString(FString::Printf(TEXT("%d x %d"), Footprint.X, Footprint.Y)));
		PlacementInfo->SetText(BDLoc::Format(TEXT("HUD.Placement.Info"), Args));
	}
	PlacementName->SetText(BDLoc::Format(Placement->IsMoving() ? TEXT("HUD.Placement.Moving") : TEXT("HUD.Placement.Placing"), Args));

	// The refusal, when the preview is red; the resulting scoreboard, when a move is on.
	const EBDPlacementRefusal Refusal = Placement->GetCurrentRefusal();
	if (Placement->IsCurrentPlacementValid() || Refusal == EBDPlacementRefusal::None)
	{
		PlacementRefusal->SetText(FText::GetEmpty());
	}
	else
	{
		PlacementRefusal->SetText(RefusalText(Refusal));
	}

	if (Placement->IsMoving() && Match != nullptr)
	{
		// The cost of the drop and the scoreboard it leaves, on one line.
		const int32 Cost = Placement->GetMoveCost();
		bool bInverts = false;
		FFormatNamedArguments MoveArgs;
		MoveArgs.Add(TEXT("Cost"), Cost);
		MoveArgs.Add(TEXT("Result"), ResultText(-Cost, bInverts));
		MoveResult->SetText(BDLoc::Format(TEXT("HUD.Placement.MoveCost"), MoveArgs));
		MoveResult->SetColorAndOpacity(FSlateColor(bInverts ? ColorRed : ColorMuted));
		MoveResult->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		MoveResult->SetVisibility(ESlateVisibility::Collapsed);
	}
}

//~ Clicks ----------------------------------------------------------------------------

void UBDHUDWidget::HandleSpeed1()
{
	if (ABDMatchManager* Match = GetMatch())
	{
		Match->SetGameSpeed(1.0f);
		UpdateSpeedButtons();
	}
}

void UBDHUDWidget::HandleSpeed2()
{
	if (ABDMatchManager* Match = GetMatch())
	{
		Match->SetGameSpeed(2.0f);
		UpdateSpeedButtons();
	}
}

void UBDHUDWidget::HandleSpeed4()
{
	if (ABDMatchManager* Match = GetMatch())
	{
		Match->SetGameSpeed(4.0f);
		UpdateSpeedButtons();
	}
}

void UBDHUDWidget::HandleUpgrade()
{
	if (UBDPlacementComponent* Placement = GetPlacement())
	{
		Placement->UpgradeSelectedDefender();
	}
}

void UBDHUDWidget::HandleSell()
{
	UBDPlacementComponent* Placement = GetPlacement();
	ABDTowerBase* Tower = Placement != nullptr ? Placement->GetSelectedDefender() : nullptr;
	if (Tower != nullptr)
	{
		Placement->TrySellActor(Tower);
	}
}

void UBDHUDWidget::HandleCancelSelection()
{
	if (UBDPlacementComponent* Placement = GetPlacement())
	{
		Placement->CancelSelection();
	}
}

void UBDHUDWidget::HandleMenu()
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->OpenPauseMenu();
	}
}

//~ End of the match ---------------------------------------------------------------

void UBDHUDWidget::UpdateEndPanel()
{
	using namespace BDHUDPrivate;

	const ABDMatchManager* Match = GetMatch();
	const EBDMatchPhase Phase = Match != nullptr ? Match->GetPhase() : EBDMatchPhase::Setup;
	if (Phase != EBDMatchPhase::Victory && Phase != EBDMatchPhase::Defeat)
	{
		EndBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Wave"), Match->GetCurrentWave());
	Args.Add(TEXT("Count"), Match->GetPrisonersFreed());
	Args.Add(TEXT("Blue"), Match->GetVotesBlue());
	Args.Add(TEXT("Red"), Match->GetVotesRed());

	// A win is told by its reward and offers the endless run; a loss by the final score.
	const bool bWon = Phase == EBDMatchPhase::Victory;
	EndTitle->SetText(BDLoc::Format(bWon ? TEXT("HUD.Wave.Victory") : TEXT("HUD.Wave.Defeat"), Args));
	EndTitle->SetColorAndOpacity(FSlateColor(bWon ? ColorBlue : ColorRed));
	EndLine->SetText(BDLoc::Format(bWon ? TEXT("HUD.End.Prisoners") : TEXT("HUD.End.Score"), Args));
	EndlessButton->SetVisibility(bWon ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	// A loss offers the saved match, when there is one; a win has nothing to go back for.
	LoadButton->SetVisibility(!bWon && ABDMatchManager::HasSavedMatch() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	EndBox->SetVisibility(ESlateVisibility::Visible);
}

void UBDHUDWidget::UpdateSaveButton()
{
	const ABDMatchManager* Match = GetMatch();
	FFormatNamedArguments Args;
	Args.Add(TEXT("Count"), Match != nullptr ? Match->GetSavesRemaining() : 0);
	SaveLabel->SetText(BDLoc::Format(TEXT("HUD.Save"), Args));
	SaveButton->SetIsEnabled(Match != nullptr && Match->CanSaveMatch());
}

//~ Build panel ---------------------------------------------------------------------

void UBDHUDWidget::UpdateBuildPanel()
{
	using namespace BDHUDPrivate;

	const ABDMatchManager* Match = GetMatch();
	const UBDPlacementComponent* Placement = GetPlacement();
	const UBDPlaceableData* Held = Placement != nullptr ? Placement->GetCurrentSelection() : nullptr;
	const bool bOver = Match == nullptr || Match->IsMatchOver();

	// The urn: there until it is down, then gone.
	const UBDPlaceableData* Urn = UBDObjectiveSettings::Get().ObjectivePlaceable.Get();
	const bool bUrnLeft = Match != nullptr && Match->GetObjectivesRemaining() > 0;
	UrnButton->SetVisibility(bUrnLeft ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bUrnLeft)
	{
		UrnLabel->SetText(Urn != nullptr ? BDLoc::PieceName(Urn) : Loc(TEXT("HUD.Build.Urn")));
		UrnLabel->SetColorAndOpacity(FSlateColor(Held != nullptr && Held == Urn ? ColorHighlight : FLinearColor::White));
		UrnButton->SetIsEnabled(!bOver);
	}

	for (int32 Index = 0; Index < PaletteData.Num(); ++Index)
	{
		const UBDPlaceableData* Data = PaletteData[Index];
		const EBDPieceKind Kind = Data->GetPieceKind();
		FFormatNamedArguments Args;
		Args.Add(TEXT("Key"), Index + 1);
		Args.Add(TEXT("Name"), BDLoc::PieceName(Data));
		Args.Add(TEXT("Count"), Match != nullptr ? Match->GetBudgetRemaining(Kind) : 0);
		BuildLabels[Index]->SetText(BDLoc::Format(TEXT("HUD.Build.Entry"), Args));
		BuildLabels[Index]->SetColorAndOpacity(FSlateColor(Held == Data ? ColorHighlight : FLinearColor::White));
		// Placeable now, by the match's rules for the kind: the urn first, dividers only before wave 1.
		BuildButtons[Index]->SetIsEnabled(!bOver && Match != nullptr && Match->CanPlace(Kind));
	}
}

void UBDHUDWidget::SelectBuild(const int32 Index)
{
	UBDPlacementComponent* Placement = GetPlacement();
	if (Placement == nullptr || !PaletteData.IsValidIndex(Index))
	{
		return;
	}

	// Clicking the piece already in hand puts it down again.
	if (Placement->GetCurrentSelection() == PaletteData[Index])
	{
		Placement->CancelSelection();
		return;
	}
	Placement->SelectPlaceable(PaletteData[Index]);
}

void UBDHUDWidget::HandleBuildUrn()
{
	UBDPlacementComponent* Placement = GetPlacement();
	UBDPlaceableData* Urn = UBDObjectiveSettings::Get().ObjectivePlaceable.LoadSynchronous();
	if (Placement != nullptr && Urn != nullptr)
	{
		Placement->SelectPlaceable(Urn);
	}
}

void UBDHUDWidget::HandleBuild0() { SelectBuild(0); }
void UBDHUDWidget::HandleBuild1() { SelectBuild(1); }
void UBDHUDWidget::HandleBuild2() { SelectBuild(2); }
void UBDHUDWidget::HandleBuild3() { SelectBuild(3); }
void UBDHUDWidget::HandleBuild4() { SelectBuild(4); }
void UBDHUDWidget::HandleBuild5() { SelectBuild(5); }
void UBDHUDWidget::HandleBuild6() { SelectBuild(6); }
void UBDHUDWidget::HandleBuild7() { SelectBuild(7); }
void UBDHUDWidget::HandleBuild8() { SelectBuild(8); }

void UBDHUDWidget::HandleSave()
{
	if (ABDMatchManager* Match = GetMatch())
	{
		Match->SaveMatch();
		UpdateSaveButton();
	}
}

void UBDHUDWidget::HandleLoad()
{
	if (ABDMatchManager* Match = GetMatch())
	{
		Match->LoadMatch();
	}
}

void UBDHUDWidget::HandleEndless()
{
	if (ABDMatchManager* Match = GetMatch())
	{
		Match->ContinueEndless();
	}
}

void UBDHUDWidget::HandleReturnToMenu()
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->ReturnToMenu();
	}
}

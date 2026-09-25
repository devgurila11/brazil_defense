// Brazil Defense. The numbers of the match over the board, and what a selection offers.

#include "UI/BDHUDWidget.h"

#include "BDBuildInfo.h"
#include "BDLog.h"
#include "Blueprint/WidgetTree.h"
#include "Candidate/BDCandidateSubsystem.h"
#include "Day/BDDayCycleComponent.h"
#include "Day/BDDaySettings.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Objective/BDObjective.h"
#include "Engine/Texture2D.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Enemy/BDCandidate.h"
#include "Enemy/BDEnemyData.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Match/BDGameBalanceSettings.h"
#include "Match/BDMatchManager.h"
#include "Misc/App.h"
#include "Objective/BDObjectiveSettings.h"
#include "Objective/BDObjectiveSubsystem.h"
#include "Placement/BDPlaceableData.h"
#include "Placement/BDPlacementComponent.h"
#include "Placement/BDPlacementSettings.h"
#include "Platform/BDPlatformComponent.h"
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
	/** The build label in the corner: there to be read when asked for, never to be noticed. */
	static constexpr int32 BuildFontSize = 10;
	/** The purse is the one number the player spends, so it reads first on its row. */
	static constexpr int32 MintFontSize = 22;
	static constexpr int32 NoticeFontSize = 26;
	static constexpr float Margin = 16.0f;
	static constexpr float PanelWidth = 320.0f;
	static constexpr float CandidateBarWidth = 480.0f;
	static constexpr float CandidateBarHeight = 18.0f;
	/** Slate units for an item icon before the viewport fraction takes over. */
	static constexpr float ItemIconSize = 72.0f;
	static constexpr float ItemGap = 6.0f;
	/** Slate units for a scoreboard picture before the viewport fraction takes over. */
	static constexpr float ScoreIconSize = 44.0f;

	/** The vote feedback: how long, how big, how far it wobbles. */
	static constexpr float VotePulseSeconds = 0.2f;
	static constexpr float VotePulseScale = 0.15f;
	static constexpr float VotePulseDegrees = 5.0f;
	static constexpr float CountTickScale = 0.10f;
	static constexpr float UrnPulseSeconds = 0.25f;
	static constexpr float UrnPulseScale = 0.06f;

	/** The count bar and the numbers that rise from the urn. */
	static constexpr float ScoreBarWidth = 360.0f;
	static constexpr float ScoreBarHeight = 14.0f;
	static const FLinearColor ColorNull = FLinearColor(0.82f, 0.82f, 0.80f);
	static constexpr float FloaterSeconds = 1.3f;
	static constexpr float FloaterMergeSeconds = 0.3f;
	static constexpr float FloaterRise = 130.0f;
	static constexpr int32 FloaterBaseFont = 22;
	static constexpr float FloaterFontPerDecade = 16.0f;
	static const float Speeds[] = { 1.0f, 2.0f, 4.0f };


	/** The refusal keys, in enum order. */
	static const TCHAR* const RefusalKeys[] = {
		TEXT("Refusal.None"), TEXT("Refusal.NoSelection"), TEXT("Refusal.NotHoveringGrid"), TEXT("Refusal.MatchRefused"),
		TEXT("Refusal.NoBudgetLeft"), TEXT("Refusal.OffGrid"), TEXT("Refusal.CellTaken"), TEXT("Refusal.EdgeOnBorder"),
		TEXT("Refusal.EdgeTaken"), TEXT("Refusal.WouldBlockPath"), TEXT("Refusal.ObjectiveMissing"), TEXT("Refusal.ObjectiveOutOfZone"),
		TEXT("Refusal.SlotTaken"), TEXT("Refusal.TowerCannotGoOnSlot"), TEXT("Refusal.CharacterNeedsPlatform"), TEXT("Refusal.CannotAffordMove"),
		TEXT("Refusal.NoFunds"), TEXT("Refusal.NoFreeSlot"), TEXT("Refusal.NotUnlocked") };
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
	RootCanvas = Canvas;

	//~ Top center: scoreboard, wave line, speed, mouths.
	UVerticalBox* Top = MakeColumn();

	// Blue ballot, blue count, the urn, red count, red ballot: the pictures are the
	// HUD's own, from the interface settings, each in a scale box sized by the screen.
	const UBDUISettings& UISettings = UBDUISettings::Get();
	UHorizontalBox* Scores = MakeRow();
	BlueScore = MakeText(ScoreFontSize, ColorBlue);
	RedScore = MakeText(ScoreFontSize, ColorRed);
	BlueIconBox = MakePicture(BlueIcon, UISettings.ScoreBlueIcon.LoadSynchronous(), ScoreIconSize);
	UrnScoreIconBox = MakePicture(UrnScoreIcon, UISettings.ScoreUrnIcon.LoadSynchronous(), ScoreIconSize * UISettings.ScoreUrnIconScale);
	RedIconBox = MakePicture(RedIcon, UISettings.ScoreRedIcon.LoadSynchronous(), ScoreIconSize);
	const auto AddScorePart = [Scores](UWidget* Widget, const float LeftPad)
	{
		UHorizontalBoxSlot* PartSlot = Scores->AddChildToHorizontalBox(Widget);
		PartSlot->SetVerticalAlignment(VAlign_Center);
		PartSlot->SetPadding(FMargin(LeftPad, 0.0f, 0.0f, 0.0f));
	};
	// The count bar between the numbers: three bands, blue for the kills, white for the
	// waste, red for the arrivals, each as wide as its share. The urn stands on it.
	ScoreBarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ScoreBarBox->SetWidthOverride(ScoreBarWidth);
	ScoreBarBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlay* BarOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UBorder* BarFrame = MakeBox(ColorPanel, 3.0f);
	USizeBox* BandsBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BandsBox->SetHeightOverride(ScoreBarHeight);
	UHorizontalBox* Bands = MakeRow();
	const auto MakeBand = [this, Bands](const FLinearColor& Color) -> UBorder*
	{
		UBorder* Band = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Band->SetBrushColor(Color);
		Band->SetPadding(FMargin(0.0f));
		UHorizontalBoxSlot* BandSlot = Bands->AddChildToHorizontalBox(Band);
		BandSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		return Band;
	};
	BlueBand = MakeBand(ColorBlue);
	NullBand = MakeBand(ColorNull);
	RedBand = MakeBand(ColorRed);
	BandsBox->AddChild(Bands);
	BarFrame->AddChild(BandsBox);
	UOverlaySlot* FrameSlot = BarOverlay->AddChildToOverlay(BarFrame);
	FrameSlot->SetHorizontalAlignment(HAlign_Fill);
	FrameSlot->SetVerticalAlignment(VAlign_Center);
	UOverlaySlot* UrnOverlaySlot = BarOverlay->AddChildToOverlay(UrnScoreIconBox);
	UrnOverlaySlot->SetHorizontalAlignment(HAlign_Center);
	UrnOverlaySlot->SetVerticalAlignment(VAlign_Center);
	ScoreBarBox->AddChild(BarOverlay);

	AddScorePart(BlueIconBox, 0.0f);
	AddScorePart(BlueScore, 8.0f);
	AddScorePart(ScoreBarBox, 16.0f);
	AddScorePart(RedScore, 16.0f);
	AddScorePart(RedIconBox, 8.0f);
	UVerticalBoxSlot* ScoresSlot = Top->AddChildToVerticalBox(Scores);
	ScoresSlot->SetHorizontalAlignment(HAlign_Center);

	// The null count under the bar: what the defense threw away, shown and never scored.
	UHorizontalBox* NullRow = MakeRow();
	NullIconBox = MakePicture(NullIcon, UISettings.ScoreNullIcon.LoadSynchronous(), ScoreIconSize * 0.6f);
	NullScore = MakeText(SmallFontSize, ColorMuted);
	UHorizontalBoxSlot* NullIconSlot = NullRow->AddChildToHorizontalBox(NullIconBox);
	NullIconSlot->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* NullTextSlot = NullRow->AddChildToHorizontalBox(NullScore);
	NullTextSlot->SetVerticalAlignment(VAlign_Center);
	NullTextSlot->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
	UVerticalBoxSlot* NullRowSlot = Top->AddChildToVerticalBox(NullRow);
	NullRowSlot->SetHorizontalAlignment(HAlign_Center);

	// What is in hand, on one row under the count and never mixed with it. The mint's
	// public money is the one thing the player spends - every piece, level and move - so
	// it is the row. The blue votes are not repeated here: they are never spent, and the
	// count bar above is where the election is read.
	//
	// Ahead of the mint, and only while something is crossing, the thief with the bribe he
	// has just been relieved of and an arrow into the mint. That group is the animation of
	// the gain, not another resource - nothing held by the thief can be spent, which is why
	// it leaves the row entirely the moment the bag is empty.
	UHorizontalBox* MoneyRow = MakeRow();
	BribeIconBox = MakePicture(BribeIcon, UISettings.BribeIcon.LoadSynchronous(), ScoreIconSize * 0.8f);
	BribeScore = MakeText(LineFontSize, ColorMuted);
	MoneyArrow = MakeText(LineFontSize, ColorMuted);
	MoneyArrow->SetText(FText::FromString(TEXT("→")));
	MintIconBox = MakePicture(MintIcon, UISettings.MintIcon.LoadSynchronous(), ScoreIconSize * 0.8f);
	MintScore = MakeText(MintFontSize, ColorText);
	const auto AddPart = [](UHorizontalBox* Row, UWidget* Widget, const float LeftPad)
	{
		UHorizontalBoxSlot* PartSlot = Row->AddChildToHorizontalBox(Widget);
		PartSlot->SetVerticalAlignment(VAlign_Center);
		PartSlot->SetPadding(FMargin(LeftPad, 0.0f, 0.0f, 0.0f));
	};
	// The thief's own little row, so showing and hiding him is one call and the meters
	// close up behind him instead of leaving a hole.
	BribeGroup = MakeRow();
	AddPart(BribeGroup, BribeIconBox, 0.0f);
	AddPart(BribeGroup, BribeScore, 6.0f);
	AddPart(BribeGroup, MoneyArrow, 8.0f);
	BribeGroup->SetVisibility(ESlateVisibility::Collapsed);
	AddPart(MoneyRow, BribeGroup, 0.0f);

	AddPart(MoneyRow, MintIconBox, 8.0f);
	AddPart(MoneyRow, MintScore, 6.0f);
	UVerticalBoxSlot* MoneyRowSlot = Top->AddChildToVerticalBox(MoneyRow);
	MoneyRowSlot->SetHorizontalAlignment(HAlign_Center);
	MoneyRowSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));

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

	ClockLine = MakeText(SmallFontSize, ColorHighlight);
	ClockLine->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* ClockSlot = Top->AddChildToVerticalBox(ClockLine);
	ClockSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	//~ Candidate, under the top block: bar, line, notice, frozen count.
	UVerticalBox* CandidateColumn = MakeColumn();

	// One row per candidate walking, each his own name, number and colour: with a
	// candidate at the urn ending the match, every one coming has to be seen. Rows are
	// built once and shown as needed; past the last row a line counts the rest.
	CandidateBox = MakeBox(ColorPanel, 8.0f);
	CandidateRows = MakeColumn();
	for (int32 Index = 0; Index < MaxCandidateRows; ++Index)
	{
		CandidateLines[Index] = MakeText(LineFontSize, ColorRed);
		CandidateLines[Index]->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* LineSlot = CandidateRows->AddChildToVerticalBox(CandidateLines[Index]);
		LineSlot->SetPadding(FMargin(0.0f, Index == 0 ? 0.0f : 6.0f, 0.0f, 0.0f));
		CandidateBars[Index] = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		CandidateBars[Index]->SetFillColorAndOpacity(ColorRed);
		CandidateBarBoxes[Index] = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CandidateBarBoxes[Index]->SetWidthOverride(CandidateBarWidth);
		CandidateBarBoxes[Index]->SetHeightOverride(CandidateBarHeight);
		CandidateBarBoxes[Index]->AddChild(CandidateBars[Index]);
		UVerticalBoxSlot* BarSlot = CandidateRows->AddChildToVerticalBox(CandidateBarBoxes[Index]);
		BarSlot->SetHorizontalAlignment(HAlign_Center);
		CandidateLines[Index]->SetVisibility(ESlateVisibility::Collapsed);
		CandidateBarBoxes[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
	CandidateBarBox = CandidateBarBoxes[0];
	CandidateOverflow = MakeText(SmallFontSize, ColorMuted);
	CandidateOverflow->SetJustification(ETextJustify::Center);
	CandidateOverflow->SetVisibility(ESlateVisibility::Collapsed);
	CandidateRows->AddChildToVerticalBox(CandidateOverflow);
	CandidateBox->AddChild(CandidateRows);
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
	CostResult = MakeText(SmallFontSize);
	CostResult->SetAutoWrapText(true);
	PlacementColumn->AddChildToVerticalBox(CostResult);
	UButton* CancelButton = MakeButton(CancelLabel, SmallFontSize);
	CancelButton->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleCancelSelection);
	UVerticalBoxSlot* CancelSlot = PlacementColumn->AddChildToVerticalBox(CancelButton);
	CancelSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	PlacementBox->AddChild(PlacementColumn);
	UVerticalBoxSlot* PlacementBoxSlot = Right->AddChildToVerticalBox(PlacementBox);
	PlacementBoxSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));

	SidePanelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SidePanelBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SidePanelBox->SetWidthOverride(PanelWidth);
	SidePanelBox->AddChild(Right);

	UCanvasPanelSlot* RightSlot = Canvas->AddChildToCanvas(SidePanelBox);
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

	//~ Bottom center: the item bar. The urn first, then the palette in its order; the
	// number keys follow the same order. Each item is a picture in a scale box over its
	// words, so a 512 px icon comes down to a share of the screen, never a pixel size.
	BuildBox = MakeBox(ColorPanel, 8.0f);
	UVerticalBox* BuildColumn = MakeColumn();
	BuildTitle = MakeText(SmallFontSize, ColorMuted);
	BuildTitle->SetJustification(ETextJustify::Center);
	BuildColumn->AddChildToVerticalBox(BuildTitle);
	UHorizontalBox* ItemRow = MakeRow();
	UVerticalBoxSlot* ItemRowSlot = BuildColumn->AddChildToVerticalBox(ItemRow);
	ItemRowSlot->SetHorizontalAlignment(HAlign_Center);
	ItemRowSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

	UrnButton = MakeItem(UrnIcon, UrnIconBox, UrnLabel, UrnCount);
	UrnButton->OnClicked.AddDynamic(this, &UBDHUDWidget::HandleBuildUrn);
	UHorizontalBoxSlot* UrnSlot = ItemRow->AddChildToHorizontalBox(UrnButton);
	UrnSlot->SetPadding(FMargin(ItemGap * 0.5f, 0.0f));
	UrnSlot->SetVerticalAlignment(VAlign_Fill);
	if (const UBDPlaceableData* Urn = UBDObjectiveSettings::Get().ObjectivePlaceable.LoadSynchronous())
	{
		if (UTexture2D* Texture = Urn->Icon.LoadSynchronous())
		{
			UrnIcon->SetBrushFromTexture(Texture, /*bMatchSize*/ false);
		}
		else
		{
			UrnIconBox->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

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
		BuildButtons[Index] = MakeItem(BuildIcons[Index], BuildIconBoxes[Index], BuildLabels[Index], BuildCounts[Index]);
		UHorizontalBoxSlot* BuildSlot = ItemRow->AddChildToHorizontalBox(BuildButtons[Index]);
		BuildSlot->SetPadding(FMargin(ItemGap * 0.5f, 0.0f));
		BuildSlot->SetVerticalAlignment(VAlign_Fill);
		if (UTexture2D* Texture = PaletteData[Index]->Icon.LoadSynchronous())
		{
			BuildIcons[Index]->SetBrushFromTexture(Texture, /*bMatchSize*/ false);
		}
		else
		{
			// No picture yet: the words stand alone until the art lands.
			BuildIconBoxes[Index]->SetVisibility(ESlateVisibility::Collapsed);
		}
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

	UCanvasPanelSlot* BuildSlotOnCanvas = Canvas->AddChildToCanvas(BuildBox);
	BuildSlotOnCanvas->SetAnchors(FAnchors(0.5f, 1.0f));
	BuildSlotOnCanvas->SetAlignment(FVector2D(0.5f, 1.0f));
	BuildSlotOnCanvas->SetAutoSize(true);
	BuildSlotOnCanvas->SetPosition(FVector2D(0.0f, -Margin));

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

	//~ The kill boards, down the sides: the horde's kinds on the left, under the menu, and
	// the candidates on the right, under the panels. Both start empty and grow down.
	HordeKillColumn = MakeColumn();
	HordeKillColumn->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* HordeKillSlot = Canvas->AddChildToCanvas(HordeKillColumn);
	HordeKillSlot->SetAnchors(FAnchors(0.0f, 0.2f));
	HordeKillSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	HordeKillSlot->SetAutoSize(true);
	HordeKillSlot->SetPosition(FVector2D(Margin, 0.0f));

	CandidateKillColumn = MakeColumn();
	CandidateKillColumn->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* CandidateKillSlot = Canvas->AddChildToCanvas(CandidateKillColumn);
	CandidateKillSlot->SetAnchors(FAnchors(1.0f, 0.55f));
	CandidateKillSlot->SetAlignment(FVector2D(1.0f, 0.0f));
	CandidateKillSlot->SetAutoSize(true);
	CandidateKillSlot->SetPosition(FVector2D(-Margin, 0.0f));

	//~ Bottom left: which build this is, as small as it can be and still be read.
	UTextBlock* Build = MakeText(BuildFontSize, ColorMuted);
	Build->SetText(FText::FromString(BDBuildInfo::GetLabel()));
	Build->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* BuildLabelSlot = Canvas->AddChildToCanvas(Build);
	BuildLabelSlot->SetAnchors(FAnchors(0.0f, 1.0f));
	BuildLabelSlot->SetAlignment(FVector2D(0.0f, 1.0f));
	BuildLabelSlot->SetAutoSize(true);
	BuildLabelSlot->SetPosition(FVector2D(Margin * 0.5f, -Margin * 0.5f));

	WidgetTree->RootWidget = Canvas;

	// The board is played with the mouse under this: the HUD never takes the keyboard.
	SetIsFocusable(false);
	CandidateBox->SetVisibility(ESlateVisibility::Collapsed);
	CandidateNotice->SetVisibility(ESlateVisibility::Collapsed);
	FrozenLine->SetVisibility(ESlateVisibility::Collapsed);
	DefenderBox->SetVisibility(ESlateVisibility::Collapsed);
	PlacementBox->SetVisibility(ESlateVisibility::Collapsed);
	EndBox->SetVisibility(ESlateVisibility::Collapsed);
	ClockLine->SetVisibility(ESlateVisibility::Collapsed);
}

USizeBox* UBDHUDWidget::MakePicture(TObjectPtr<UImage>& OutImage, UTexture2D* Texture, const float Size)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Size);
	Box->SetHeightOverride(Size);
	Box->SetVisibility(ESlateVisibility::HitTestInvisible);
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	Scale->SetStretch(EStretch::ScaleToFit);
	OutImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	OutImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Texture != nullptr)
	{
		OutImage->SetBrushFromTexture(Texture, /*bMatchSize*/ false);
	}
	else
	{
		// No picture set: the box takes no room rather than showing a blank.
		Box->SetVisibility(ESlateVisibility::Collapsed);
	}
	Scale->AddChild(OutImage);
	Box->AddChild(Scale);
	return Box;
}

UButton* UBDHUDWidget::MakeItem(TObjectPtr<UImage>& OutIcon, TObjectPtr<USizeBox>& OutIconBox, TObjectPtr<UTextBlock>& OutLabel, TObjectPtr<UTextBlock>& OutInfo)
{
	using namespace BDHUDPrivate;

	// Same face as every other button, with a column inside instead of one label.
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(ColorButton);
	UVerticalBox* Column = MakeColumn();

	// The picture: a size box says how much of the screen, the scale box keeps the aspect.
	OutIconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	OutIconBox->SetWidthOverride(ItemIconSize);
	OutIconBox->SetHeightOverride(ItemIconSize);
	UScaleBox* Scale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
	Scale->SetStretch(EStretch::ScaleToFit);
	OutIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	OutIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	Scale->AddChild(OutIcon);
	OutIconBox->AddChild(Scale);
	UVerticalBoxSlot* IconSlot = Column->AddChildToVerticalBox(OutIconBox);
	IconSlot->SetHorizontalAlignment(HAlign_Center);

	OutLabel = MakeText(SmallFontSize, ColorPanelDark);
	OutLabel->SetJustification(ETextJustify::Center);
	Column->AddChildToVerticalBox(OutLabel);
	// The info line: what is left and what it costs, or why the piece cannot be taken.
	// It wraps, because a reason is words and a count is a digit.
	OutInfo = MakeText(SmallFontSize, ColorPanelDark);
	OutInfo->SetJustification(ETextJustify::Center);
	OutInfo->SetAutoWrapText(true);
	Column->AddChildToVerticalBox(OutInfo);

	Button->AddChild(Column);
	return Button;
}

void UBDHUDWidget::ApplyResponsiveSizes()
{
	using namespace BDHUDPrivate;

	// Local size is already past the DPI curve: a fraction of it is a fraction of the
	// screen at every resolution the curve covers.
	const FVector2D Size = GetCachedGeometry().GetLocalSize();
	if (Size.X <= 1.0f || Size.Y <= 1.0f || Size.Equals(LastViewportSize, 0.5f))
	{
		return;
	}
	LastViewportSize = Size;

	const UBDUISettings& Settings = UBDUISettings::Get();
	const float Icon = FMath::Max(24.0f, Size.Y * Settings.ItemIconHeightFraction);
	UrnIconBox->SetWidthOverride(Icon);
	UrnIconBox->SetHeightOverride(Icon);
	for (int32 Index = 0; Index < PaletteData.Num(); ++Index)
	{
		BuildIconBoxes[Index]->SetWidthOverride(Icon);
		BuildIconBoxes[Index]->SetHeightOverride(Icon);
	}
	const float ScoreIcon = FMath::Max(16.0f, Size.Y * Settings.ScoreIconHeightFraction);
	BlueIconBox->SetWidthOverride(ScoreIcon);
	BlueIconBox->SetHeightOverride(ScoreIcon);
	RedIconBox->SetWidthOverride(ScoreIcon);
	RedIconBox->SetHeightOverride(ScoreIcon);
	UrnScoreIconBox->SetWidthOverride(ScoreIcon * Settings.ScoreUrnIconScale);
	UrnScoreIconBox->SetHeightOverride(ScoreIcon * Settings.ScoreUrnIconScale);
	NullIconBox->SetWidthOverride(ScoreIcon * 0.6f);
	NullIconBox->SetHeightOverride(ScoreIcon * 0.6f);
	const float MoneyIcon = FMath::Max(16.0f, Size.Y * Settings.MoneyIconHeightFraction);
	BribeIconBox->SetWidthOverride(MoneyIcon);
	BribeIconBox->SetHeightOverride(MoneyIcon);
	MintIconBox->SetWidthOverride(MoneyIcon);
	MintIconBox->SetHeightOverride(MoneyIcon);
	ScoreBarBox->SetWidthOverride(FMath::Max(120.0f, Size.X * Settings.ScoreBarWidthFraction));
	SidePanelBox->SetWidthOverride(FMath::Max(200.0f, Size.X * Settings.SidePanelWidthFraction));
	for (int32 Index = 0; Index < MaxCandidateRows; ++Index)
	{
		CandidateBarBoxes[Index]->SetWidthOverride(FMath::Max(160.0f, Size.X * Settings.CandidateBarWidthFraction));
	}
	KillIconSize = FMath::Max(20.0f, Size.Y * Settings.KillIconHeightFraction);
	for (const FBDKillEntry& Entry : HordeKills) { SizeKillEntry(Entry); }
	for (const FBDKillEntry& Entry : CandidateKills) { SizeKillEntry(Entry); }
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
	UpdateMoneyCounters();
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
		Match->OnMoneyChanged.Remove(MoneyChangedHandle);
		Match->OnPhaseChanged.Remove(PhaseChangedHandle);
		Match->OnWaveStarted.Remove(WaveStartedHandle);
	}
	BoundMatch.Reset();
	if (UBDObjectiveSubsystem* Objectives = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UBDObjectiveSubsystem>() : nullptr)
	{
		Objectives->OnVoteSound.Remove(VoteSoundHandle);
	}
	VoteSoundHandle.Reset();

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
	MoneyChangedHandle = Match->OnMoneyChanged.AddUObject(this, &UBDHUDWidget::HandleMoneyChanged);
	LastBlueVotes = Match->GetVotesBlue();
	LastRedVotes = Match->GetVotesRed();
	LastBribe = Match->GetBribeHeld();
	LastPublicMoney = Match->GetPublicMoney();
	bVotesSeen = true;
	if (UBDObjectiveSubsystem* Objectives = GetWorld() != nullptr ? GetWorld()->GetSubsystem<UBDObjectiveSubsystem>() : nullptr)
	{
		if (!VoteSoundHandle.IsValid())
		{
			VoteSoundHandle = Objectives->OnVoteSound.AddUObject(this, &UBDHUDWidget::HandleVoteSound);
		}
	}
	PhaseChangedHandle = Match->OnPhaseChanged.AddUObject(this, &UBDHUDWidget::HandlePhaseChanged);
	WaveStartedHandle = Match->OnWaveStarted.AddUObject(this, &UBDHUDWidget::HandleWaveStarted);
	BoundMatch = Match;

	UpdateScoreboard();
	UpdateMoneyCounters();
	UpdateWaveLine();
	UpdateSpeedButtons();
	UpdateMouths();
	UpdateEndPanel();
}

void UBDHUDWidget::HandleVotesChanged(const int32 Blue, const int32 Red)
{
	// Only the side that went up shivers. The first report of a match, and a load, set
	// the baseline without a pulse.
	if (bVotesSeen)
	{
		if (Blue > LastBlueVotes) { BluePulse.Trigger(); }
		if (Red > LastRedVotes)
		{
			RedPulse.Trigger();
			SpawnOrGrowFloater(Red - LastRedVotes);
		}
	}
	LastBlueVotes = Blue;
	LastRedVotes = Red;
	bVotesSeen = true;

	UpdateScoreboard();
}

void UBDHUDWidget::HandleVoteSound()
{
	UrnPulse.Trigger();
}

void UBDHUDWidget::HandleMoneyChanged(const int32 Bribe, const int32 PublicMoney)
{
	// Only the counter that went up shivers. A conversion moves both, one down and one
	// up, so this reads as the value crossing rather than as two unrelated numbers.
	if (Bribe > LastBribe) { BribePulse.Trigger(); }
	if (PublicMoney > LastPublicMoney) { MintPulse.Trigger(); }
	LastBribe = Bribe;
	LastPublicMoney = PublicMoney;

	UpdateMoneyCounters();
}

//~ Numbers from the urn -------------------------------------------------------------

void UBDHUDWidget::SetFloaterLook(FBDFloater& Floater) const
{
	using namespace BDHUDPrivate;

	// Bigger with the value, by decades: +5 reads small, +340 reads large, +3000 larger still.
	FSlateFontInfo Font = Floater.Text->GetFont();
	Font.Size = FloaterBaseFont + FMath::RoundToInt(FloaterFontPerDecade * FMath::LogX(10.0f, 1.0f + static_cast<float>(Floater.Value)));
	Floater.Text->SetFont(Font);
	Floater.Text->SetText(FText::FromString(FString::Printf(TEXT("+%d"), Floater.Value)));
}

void UBDHUDWidget::SpawnOrGrowFloater(const int32 Votes)
{
	using namespace BDHUDPrivate;

	if (Votes <= 0 || RootCanvas == nullptr)
	{
		return;
	}

	// Arrivals a few frames apart are one number, not a stack of them.
	if (Floaters.Num() > 0 && Floaters.Last().Age < FloaterMergeSeconds)
	{
		Floaters.Last().Value += Votes;
		SetFloaterLook(Floaters.Last());
		return;
	}

	FBDFloater& Floater = Floaters.AddDefaulted_GetRef();
	Floater.Text = MakeText(FloaterBaseFont, ColorRed);
	Floater.Text->SetJustification(ETextJustify::Center);
	Floater.Value = Votes;
	UCanvasPanelSlot* FloaterSlot = RootCanvas->AddChildToCanvas(Floater.Text);
	FloaterSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	FloaterSlot->SetAlignment(FVector2D(0.5f, 1.0f));
	FloaterSlot->SetAutoSize(true);
	FloaterSlot->SetZOrder(5);
	SetFloaterLook(Floater);
}

void UBDHUDWidget::UpdateFloaters(const float RealDeltaSeconds)
{
	using namespace BDHUDPrivate;

	if (Floaters.Num() == 0)
	{
		return;
	}

	// Anchored on the urn, in canvas units (the projection is in pixels, past the DPI).
	APlayerController* Controller = GetOwningPlayer();
	const ABDObjective* Urn = ABDObjective::Get(GetWorld());
	FVector2D Screen = FVector2D::ZeroVector;
	const bool bOnScreen = Controller != nullptr && Urn != nullptr
		&& UGameplayStatics::ProjectWorldToScreen(Controller, Urn->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f), Screen, /*bPlayerViewportRelative*/ true);
	const float Scale = FMath::Max(0.01f, UWidgetLayoutLibrary::GetViewportScale(this));

	for (int32 Index = Floaters.Num() - 1; Index >= 0; --Index)
	{
		FBDFloater& Floater = Floaters[Index];
		Floater.Age += RealDeltaSeconds;
		if (Floater.Age >= FloaterSeconds || Floater.Text == nullptr)
		{
			if (Floater.Text != nullptr)
			{
				Floater.Text->RemoveFromParent();
			}
			Floaters.RemoveAt(Index);
			continue;
		}

		const float T = Floater.Age / FloaterSeconds;
		if (UCanvasPanelSlot* FloaterSlot = Cast<UCanvasPanelSlot>(Floater.Text->Slot))
		{
			FloaterSlot->SetPosition(Screen / Scale + FVector2D(0.0f, -FloaterRise * T));
		}
		// Up fast, then fading over the second half.
		Floater.Text->SetRenderOpacity(bOnScreen ? FMath::Clamp((1.0f - T) * 2.0f, 0.0f, 1.0f) : 0.0f);
	}
}

//~ Vote feedback -------------------------------------------------------------------

void UBDHUDWidget::FBDPulse::Trigger()
{
	if (bRunning)
	{
		// Kept, not restarted: the running pulse finishes and one more follows.
		bPending = true;
		return;
	}
	bRunning = true;
	Elapsed = 0.0f;
}

bool UBDHUDWidget::FBDPulse::Advance(const float DeltaSeconds, const float Duration)
{
	if (!bRunning)
	{
		return false;
	}

	Elapsed += DeltaSeconds;
	if (Elapsed >= Duration)
	{
		bRunning = bPending;
		bPending = false;
		Elapsed = 0.0f;
		// One frame at rest between two pulses, so they read as two.
		return true;
	}
	return true;
}

float UBDHUDWidget::FBDPulse::Bump(const float Duration) const
{
	if (!bRunning || Duration <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Sin(PI * FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f));
}

float UBDHUDWidget::FBDPulse::Wobble(const float Duration) const
{
	if (!bRunning || Duration <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Sin(2.0f * PI * FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f));
}

void UBDHUDWidget::ApplyPulse(UWidget* Icon, UWidget* Count, const FBDPulse& Pulse)
{
	using namespace BDHUDPrivate;

	const float Bump = Pulse.Bump(VotePulseSeconds);
	if (Icon != nullptr)
	{
		Icon->SetRenderScale(FVector2D(1.0f + VotePulseScale * Bump));
		Icon->SetRenderTransformAngle(VotePulseDegrees * Pulse.Wobble(VotePulseSeconds));
	}
	if (Count != nullptr)
	{
		Count->SetRenderScale(FVector2D(1.0f + CountTickScale * Bump));
	}
}

void UBDHUDWidget::UpdateVotePulses(const float RealDeltaSeconds)
{
	using namespace BDHUDPrivate;

	// Advanced then applied, so the last frame of a pulse lands back on 1.0 and 0 degrees.
	const bool bBlueWas = BluePulse.bRunning;
	const bool bRedWas = RedPulse.bRunning;
	const bool bUrnWas = UrnPulse.bRunning;
	BluePulse.Advance(RealDeltaSeconds, VotePulseSeconds);
	RedPulse.Advance(RealDeltaSeconds, VotePulseSeconds);
	UrnPulse.Advance(RealDeltaSeconds, UrnPulseSeconds);

	const bool bBribeWas = BribePulse.bRunning;
	const bool bMintWas = MintPulse.bRunning;
	BribePulse.Advance(RealDeltaSeconds, VotePulseSeconds);
	MintPulse.Advance(RealDeltaSeconds, VotePulseSeconds);

	if (bBlueWas || BluePulse.bRunning) { ApplyPulse(BlueIconBox, BlueScore, BluePulse); }
	if (bRedWas || RedPulse.bRunning) { ApplyPulse(RedIconBox, RedScore, RedPulse); }
	if (bBribeWas || BribePulse.bRunning) { ApplyPulse(BribeIconBox, BribeScore, BribePulse); }
	if (bMintWas || MintPulse.bRunning) { ApplyPulse(MintIconBox, MintScore, MintPulse); }
	if (bUrnWas || UrnPulse.bRunning)
	{
		UrnScoreIconBox->SetRenderScale(FVector2D(1.0f + UrnPulseScale * UrnPulse.Bump(UrnPulseSeconds)));
	}
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
	UpdateClock();
	ApplyResponsiveSizes();
	UpdateVotePulses(static_cast<float>(FApp::GetDeltaTime()));
	UpdateKillBoards(static_cast<float>(FApp::GetDeltaTime()));
	UpdateFloaters(static_cast<float>(FApp::GetDeltaTime()));
}

//~ Kill boards ----------------------------------------------------------------------

UBDHUDWidget::FBDKillEntry UBDHUDWidget::MakeKillEntry(UVerticalBox* Column, const FName Type, UTexture2D* Icon, const bool bIconFirst)
{
	using namespace BDHUDPrivate;

	FBDKillEntry Entry;
	Entry.Type = Type;
	TObjectPtr<UImage> Image;
	Entry.IconBox = MakePicture(Image, Icon, ScoreIconSize);
	Entry.Count = MakeText(MintFontSize, ColorText);

	UHorizontalBox* Row = MakeRow();
	const auto AddPart = [Row](UWidget* Widget, const float LeftPad)
	{
		UHorizontalBoxSlot* PartSlot = Row->AddChildToHorizontalBox(Widget);
		PartSlot->SetVerticalAlignment(VAlign_Center);
		PartSlot->SetPadding(FMargin(LeftPad, 0.0f, 0.0f, 0.0f));
	};
	// The icon stands on the edge of the screen and the number looks inwards.
	if (bIconFirst)
	{
		AddPart(Entry.IconBox, 0.0f);
		AddPart(Entry.Count, 8.0f);
	}
	else
	{
		AddPart(Entry.Count, 0.0f);
		AddPart(Entry.IconBox, 8.0f);
	}
	UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row);
	RowSlot->SetHorizontalAlignment(bIconFirst ? HAlign_Left : HAlign_Right);
	RowSlot->SetPadding(FMargin(0.0f, 4.0f));

	SizeKillEntry(Entry);
	UE_LOG(LogBDUI, Log, TEXT("Kill board: %s gets its row (%s)."), *Type.ToString(), Icon != nullptr ? *Icon->GetName() : TEXT("no icon"));
	return Entry;
}

void UBDHUDWidget::SizeKillEntry(const FBDKillEntry& Entry) const
{
	if (Entry.IconBox != nullptr && KillIconSize > 0.0f)
	{
		Entry.IconBox->SetWidthOverride(KillIconSize);
		Entry.IconBox->SetHeightOverride(KillIconSize);
	}
}

void UBDHUDWidget::UpdateKillBoards(const float RealDeltaSeconds)
{
	using namespace BDHUDPrivate;

	// A count that went up pulses its own icon and ticks its number. Advanced then applied,
	// so the last frame of a pulse lands back at rest, as on the ballots.
	const auto Refresh = [this, RealDeltaSeconds](FBDKillEntry& Entry, const int32 Kills, const FText& Text)
	{
		if (Kills > Entry.Shown)
		{
			Entry.Pulse.Trigger();
		}
		if (Kills != Entry.Shown || Entry.Count->GetText().IsEmpty())
		{
			Entry.Count->SetText(Text);
			Entry.Shown = Kills;
		}
		const bool bWas = Entry.Pulse.bRunning;
		Entry.Pulse.Advance(RealDeltaSeconds, VotePulseSeconds);
		if (bWas || Entry.Pulse.bRunning)
		{
			ApplyPulse(Entry.IconBox, Entry.Count, Entry.Pulse);
		}
	};

	// The horde: one row per kind, in the order the kinds were first killed. A rewound
	// match empties the tallies, and the board with them.
	const UBDWaveSubsystem* Waves = GetWaves();
	const TArray<FBDKillTally>* Tallies = Waves != nullptr ? &Waves->GetMatchTotals().KillsByType : nullptr;
	if (Tallies == nullptr || Tallies->Num() < HordeKills.Num())
	{
		HordeKillColumn->ClearChildren();
		HordeKills.Reset();
	}
	for (int32 Index = 0; Tallies != nullptr && Index < Tallies->Num(); ++Index)
	{
		const FBDKillTally& Tally = (*Tallies)[Index];
		if (!HordeKills.IsValidIndex(Index))
		{
			const UBDEnemyData* Data = Tally.Data.Get();
			UTexture2D* Icon = Data != nullptr ? Data->KillIcon.LoadSynchronous() : nullptr;
			HordeKills.Add(MakeKillEntry(HordeKillColumn, Tally.Type, Icon, /*bIconFirst*/ true));
		}
		FBDKillEntry& Entry = HordeKills[Index];
		// With no icon the kind is named, so the count still says what it counts.
		const bool bNamed = Entry.IconBox->GetVisibility() == ESlateVisibility::Collapsed;
		Refresh(Entry, Tally.Kills, bNamed
			? FText::FromString(FString::Printf(TEXT("%s %d"), *Tally.Type.ToString(), Tally.Kills))
			: FText::AsNumber(Tally.Kills));
	}

	// The candidates: one row for all of them for now, how many of the scheduled ones have
	// been brought down. Each will get his own face; the counter stays one until then.
	const UBDCandidateSubsystem* Candidates = GetCandidates();
	const int32 Fallen = Candidates != nullptr ? Candidates->GetFallenCount() : 0;
	if (Fallen <= 0 && CandidateKills.Num() > 0)
	{
		CandidateKillColumn->ClearChildren();
		CandidateKills.Reset();
	}
	if (Fallen > 0)
	{
		if (CandidateKills.Num() == 0)
		{
			CandidateKills.Add(MakeKillEntry(CandidateKillColumn, TEXT("Candidates"),
				UBDUISettings::Get().CandidateKillIcon.LoadSynchronous(), /*bIconFirst*/ false));
		}
		Refresh(CandidateKills[0], Fallen, FText::AsNumber(Fallen));
	}
}

void UBDHUDWidget::UpdateClock()
{
	static const TCHAR* const PhaseKeys[] = { TEXT("Day.Sunrise"), TEXT("Day.Day"), TEXT("Day.Sunset"), TEXT("Day.Dusk"), TEXT("Day.Night") };

	const ABDMatchManager* Match = GetMatch();
	const UBDDayCycleComponent* Day = Match != nullptr ? Match->GetDayCycle() : nullptr;
	const bool bShow = Day != nullptr && (Day->IsBlending() || Day->GetSecondsSinceBlendEnded() < UBDDaySettings::Get().ClockNoticeSeconds);
	if (!bShow)
	{
		ClockLine->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Phase"), Loc(PhaseKeys[FMath::Clamp(static_cast<int32>(Day->GetPhase()), 0, 4)]));
	Args.Add(TEXT("Time"), Day->GetClockText());
	ClockLine->SetText(BDLoc::Format(TEXT("HUD.Clock"), Args));
	ClockLine->SetVisibility(ESlateVisibility::HitTestInvisible);
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
	Args[TEXT("Votes")] = FFormatArgumentValue(Match != nullptr ? Match->GetVotesNull() : 0);
	NullScore->SetText(BDLoc::Format(TEXT("HUD.Score.Null"), Args));
	UpdateScoreBar();
}

void UBDHUDWidget::UpdateMoneyCounters()
{
	using namespace BDHUDPrivate;

	const ABDMatchManager* Match = GetMatch();
	const int32 Bribe = Match != nullptr ? Match->GetBribeHeld() : 0;
	const int32 Money = Match != nullptr ? Match->GetPublicMoney() : 0;

	FFormatNamedArguments Args;
	Args.Add(TEXT("Amount"), Bribe);
	BribeScore->SetText(BDLoc::Format(TEXT("HUD.Money.Bribe"), Args));
	Args[TEXT("Amount")] = FFormatArgumentValue(Money);
	MintScore->SetText(BDLoc::Format(TEXT("HUD.Money.Public"), Args));

	// The thief is in the row only while he is carrying: an empty bag between the two
	// meters would read as a third balance the player could spend, and it is not one.
	BribeGroup->SetVisibility(Bribe > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	MoneyArrow->SetColorAndOpacity(FSlateColor(ColorBlue));
	BribeScore->SetColorAndOpacity(FSlateColor(ColorText));
}

void UBDHUDWidget::UpdateScoreBar()
{
	const ABDMatchManager* Match = GetMatch();
	const float Blue = Match != nullptr ? static_cast<float>(Match->GetVotesBlue()) : 0.0f;
	const float Red = Match != nullptr ? static_cast<float>(Match->GetVotesRed()) : 0.0f;
	const float Null = Match != nullptr ? static_cast<float>(Match->GetVotesNull()) : 0.0f;

	// Fill weights are the shares. Nothing counted yet: the white band stands alone.
	const bool bEmpty = Blue + Red + Null <= 0.0f;
	const auto Weight = [](UBorder* Band, const float Value)
	{
		if (UHorizontalBoxSlot* BandSlot = Cast<UHorizontalBoxSlot>(Band->Slot))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = FMath::Max(0.0f, Value);
			BandSlot->SetSize(Size);
		}
		Band->SetVisibility(Value > 0.0f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	};
	Weight(BlueBand, Blue);
	Weight(NullBand, bEmpty ? 1.0f : Null);
	Weight(RedBand, Red);
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

	// Every candidate walking, in the order they came out, one row each; the overflow
	// line counts whoever did not get a row.
	TArray<ABDCandidate*> Walking;
	if (Candidates != nullptr)
	{
		Candidates->GetLivingCandidates(Walking);
	}
	int32 Row = 0;
	for (const ABDCandidate* Walker : Walking)
	{
		if (Walker == nullptr || Walker->GetMaxHealth() <= 0.0f || Row >= MaxCandidateRows)
		{
			continue;
		}
		FFormatNamedArguments Args;
		Args.Add(TEXT("Name"), Walker->DisplayName);
		Args.Add(TEXT("Health"), FMath::CeilToInt(Walker->GetCurrentHealth()));
		Args.Add(TEXT("MaxHealth"), FMath::CeilToInt(Walker->GetMaxHealth()));
		CandidateLines[Row]->SetText(BDLoc::Format(TEXT("HUD.Candidate.Health"), Args));
		CandidateLines[Row]->SetColorAndOpacity(FSlateColor(Walker->GetDebugTint()));
		CandidateBars[Row]->SetFillColorAndOpacity(Walker->GetDebugTint());
		CandidateBars[Row]->SetPercent(FMath::Clamp(Walker->GetCurrentHealth() / Walker->GetMaxHealth(), 0.0f, 1.0f));
		CandidateLines[Row]->SetVisibility(ESlateVisibility::HitTestInvisible);
		// The bar waits for the first hit, like the ones over the creeps; the line with the
		// name is there from the moment he walks out, so he is never missed.
		CandidateBarBoxes[Row]->SetVisibility(Walker->GetCurrentHealth() < Walker->GetMaxHealth()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		++Row;
	}
	for (int32 Index = Row; Index < MaxCandidateRows; ++Index)
	{
		CandidateLines[Index]->SetVisibility(ESlateVisibility::Collapsed);
		CandidateBarBoxes[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
	const int32 Overflow = Walking.Num() - Row;
	if (Overflow > 0)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Count"), Overflow);
		CandidateOverflow->SetText(BDLoc::Format(TEXT("HUD.Candidate.More"), Args));
		CandidateOverflow->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		CandidateOverflow->SetVisibility(ESlateVisibility::Collapsed);
	}
	CandidateBox->SetVisibility(Row > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (Candidates != nullptr && Candidates->IsReturnActive())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Count"), Candidates->GetReturnRemaining());
		FrozenLine->SetText(BDLoc::Format(TEXT("HUD.Candidate.Return"), Args));
		FrozenLine->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		FrozenLine->SetVisibility(ESlateVisibility::Collapsed);
	}
}

//~ Panels ---------------------------------------------------------------------------

FText UBDHUDWidget::ResultText(const int32 Delta, bool& bOutShort) const
{
	const ABDMatchManager* Match = GetMatch();
	bOutShort = false;
	if (Match == nullptr)
	{
		return FText::GetEmpty();
	}

	// The purse, not the count: nothing the player buys or sells moves the votes, so the
	// line that used to warn about the count turning now says what is left to spend.
	const int32 Money = Match->GetPublicMoney();
	bOutShort = Money + Delta < 0;

	FFormatNamedArguments Args;
	Args.Add(TEXT("Money"), Money);
	Args.Add(TEXT("After"), Money + Delta);
	Args.Add(TEXT("Cost"), -Delta);
	Args.Add(TEXT("Missing"), -(Money + Delta));
	return BDLoc::Format(bOutShort ? TEXT("HUD.Result.MoneyShort") : TEXT("HUD.Result.Money"), Args);
}

void UBDHUDWidget::SetResultLine(UTextBlock* Line, const int32 Delta)
{
	bool bShort = false;
	Line->SetText(ResultText(Delta, bShort));
	Line->SetColorAndOpacity(FSlateColor(bShort ? ColorRed : ColorMuted));
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

	// Evolution: the cost in public money, and the balance it leaves. Votes are not
	// touched by an upgrade any more, so the count is not quoted here.
	const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();
	const int32 MoneyHeld = Match->GetPublicMoney();
	FString Reason;
	const bool bCanUpgrade = Tower->CanUpgrade(Reason);
	Args.Add(TEXT("MaxLevel"), UBDTowerData::MaxLevels);
	if (Tower->IsMaxLevel())
	{
		UpgradeLabel->SetText(BDLoc::Format(TEXT("HUD.Defender.MaxLevel"), Args));
		UpgradeResult->SetText(FText::GetEmpty());
	}
	else
	{
		const int32 Cost = Tower->GetUpgradeCost();
		Args.Add(TEXT("Cost"), Cost);
		Args.Add(TEXT("NextLevel"), Tower->GetTowerLevel() + 1);
		Args.Add(TEXT("NextDamage"), FMath::RoundToInt(Tower->GetDamageAtNextLevel()));
		UpgradeLabel->SetText(BDLoc::Format(TEXT("HUD.Defender.Upgrade"), Args));

		// A defender on a platform climbs with the others, so the button goes dead for
		// reasons that have nothing to do with money. Said plainly, or it reads as a bug.
		const UBDPlatformComponent* Stand = Tower->GetPlatform();
		const bool bShortOfSlots = Stand != nullptr && !Stand->IsFullyManned();
		const bool bOutOfStep = Stand != nullptr && !bShortOfSlots && Tower->GetTowerLevel() > Stand->GetBlockLevel();
		if (bShortOfSlots || bOutOfStep)
		{
			FFormatNamedArguments BlockArgs;
			BlockArgs.Add(TEXT("Slots"), Stand->GetFreeSlotCount());
			BlockArgs.Add(TEXT("Block"), Stand->GetBlockLevel());
			UpgradeResult->SetText(BDLoc::Format(bShortOfSlots ? TEXT("HUD.Defender.BlockedSlots") : TEXT("HUD.Defender.BlockedStep"), BlockArgs));
			UpgradeResult->SetColorAndOpacity(FSlateColor(ColorRed));
		}
		else
		{
			FFormatNamedArguments MoneyArgs;
			MoneyArgs.Add(TEXT("Money"), MoneyHeld);
			MoneyArgs.Add(TEXT("After"), MoneyHeld - Cost);
			MoneyArgs.Add(TEXT("Cost"), Cost);
			MoneyArgs.Add(TEXT("Missing"), Cost - MoneyHeld);
			// Short of money, the line says so and by how much: a balance going negative in
			// red is a sum to work out, and the button beside it is already dead.
			const bool bShort = Cost > MoneyHeld;
			UpgradeResult->SetText(BDLoc::Format(bShort ? TEXT("HUD.Result.MoneyShort") : TEXT("HUD.Result.Money"), MoneyArgs));
			UpgradeResult->SetColorAndOpacity(FSlateColor(bShort ? ColorRed : ColorMuted));
		}
	}
	UpgradeButton->SetIsEnabled(bCanUpgrade);

	// Sell: a share of what it was bought for, plus the share of what its levels cost,
	// all of it public money, and the balance it leaves.
	const UBDPlaceableData* Placeable = Placement->FindPlaceableOfActor(Tower);
	const int32 Refund = Placeable != nullptr ? Match->GetSellRefund(Placeable->GetPieceKind(), Placement->FindPaidCostOfActor(Tower)) : 0;
	const int32 MoneyBack = Balance.GetEvolutionRefund(Tower->GetEvolutionSpent());
	Args.Add(TEXT("Refund"), Refund);
	Args.Add(TEXT("Money"), MoneyBack);
	SellLabel->SetText(BDLoc::Format(MoneyBack > 0 ? TEXT("HUD.Defender.SellEvolved") : TEXT("HUD.Defender.Sell"), Args));
	SetResultLine(SellResult, Refund + MoneyBack);
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
	Args.Add(TEXT("Cost"), Match != nullptr ? Match->GetBuildPrice(Selection) : Selection->GetBuildCost());
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

	// What it costs and the public money it would leave, before the click. A move was
	// already priced here; a build was not, and a build is the one the player makes over
	// and over.
	if (Match != nullptr)
	{
		const bool bMoving = Placement->IsMoving();
		const int32 Cost = bMoving ? Placement->GetMoveCost() : Match->GetBuildPrice(Selection);
		bool bShort = false;
		FFormatNamedArguments CostArgs;
		CostArgs.Add(TEXT("Cost"), Cost);
		CostArgs.Add(TEXT("Result"), ResultText(-Cost, bShort));
		CostArgs.Add(TEXT("Left"), Match->GetDividersRemaining());
		// A divider comes out of its own hand, not the purse: say that instead of a price of 0.
		const bool bFromHand = !bMoving && Selection->GetPieceKind() == EBDPieceKind::Divider;
		CostResult->SetText(BDLoc::Format(bFromHand ? TEXT("HUD.Placement.FromHand")
			: bMoving ? TEXT("HUD.Placement.MoveCost") : TEXT("HUD.Placement.BuildCost"), CostArgs));
		CostResult->SetColorAndOpacity(FSlateColor(bShort ? ColorRed : ColorMuted));
		CostResult->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		CostResult->SetVisibility(ESlateVisibility::Collapsed);
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

FText UBDHUDWidget::BuildCountText(const EBDPieceKind Kind) const
{
	const ABDMatchManager* Match = GetMatch();
	if (Match == nullptr)
	{
		return FText::GetEmpty();
	}

	// What the number means depends on the kind: the maze is a hand and counts down, a
	// character reads the slots still free on the board, and a ground tower is not
	// counted at all - only the votes and the grid stop it, so it has no number to give.
	if (ABDMatchManager::HasBudgetCeiling(Kind))
	{
		return FText::AsNumber(Match->GetBudgetRemaining(Kind));
	}
	return Kind == EBDPieceKind::Character ? FText::AsNumber(Match->GetFreeCharacterSlots()) : FText::GetEmpty();
}

EBDPlacementRefusal UBDHUDWidget::BuildRefusal(const UBDPlaceableData* Data) const
{
	// The hand asks the same question before it takes a piece, so a grey button and a
	// palette key never disagree about what can be picked up.
	const UBDPlacementComponent* Placement = GetPlacement();
	return Data == nullptr || Placement == nullptr || GetMatch() == nullptr
		? EBDPlacementRefusal::MatchRefused
		: Placement->GetHandRefusal(Data);
}

FText UBDHUDWidget::BuildInfoText(const UBDPlaceableData* Data, const EBDPlacementRefusal Refusal) const
{
	if (Data == nullptr)
	{
		return FText::GetEmpty();
	}

	// The reason wins the line whenever there is one. A greyed button with a number under
	// it still says nothing about why it is grey, and that silence is the whole complaint.
	// A locked piece says when it comes, and short of money says the price, so the player
	// knows what they are waiting for.
	const ABDMatchManager* Match = GetMatch();
	FFormatNamedArguments Args;
	Args.Add(TEXT("Cost"), Match != nullptr ? Match->GetBuildPrice(Data) : Data->GetBuildCost());
	if (Refusal == EBDPlacementRefusal::NotUnlocked)
	{
		Args.Add(TEXT("Wave"), ABDMatchManager::GetUnlockWave(Data));
		return BDLoc::Format(TEXT("HUD.Build.Locked"), Args);
	}
	if (Refusal == EBDPlacementRefusal::NoFunds)
	{
		return BDLoc::Format(TEXT("HUD.Build.NoFunds"), Args);
	}
	if (Refusal != EBDPlacementRefusal::None)
	{
		return RefusalText(Refusal);
	}

	const FText Left = BuildCountText(Data->GetPieceKind());
	if (Left.IsEmpty())
	{
		return BDLoc::Format(TEXT("HUD.Build.Cost"), Args);
	}
	Args.Add(TEXT("Left"), Left);
	// The dividers are a hand of their own and cost no money: the count is the whole story.
	if (Data->GetPieceKind() == EBDPieceKind::Divider)
	{
		return BDLoc::Format(TEXT("HUD.Build.Hand"), Args);
	}
	return BDLoc::Format(TEXT("HUD.Build.Info"), Args);
}

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
		FFormatNamedArguments UrnArgs;
		UrnArgs.Add(TEXT("Key"), FText::FromString(TEXT("U")));
		UrnArgs.Add(TEXT("Name"), Urn != nullptr ? BDLoc::PieceName(Urn) : Loc(TEXT("HUD.Build.Urn")));
		UrnLabel->SetText(BDLoc::Format(TEXT("HUD.Build.Entry"), UrnArgs));
		// The item in hand is the lit one, like the current speed.
		const bool bUrnHeld = Held != nullptr && Held == Urn;
		UrnButton->SetBackgroundColor(bUrnHeld ? ColorButton : ColorButtonIdle);
		UrnLabel->SetColorAndOpacity(FSlateColor(bUrnHeld ? ColorPanelDark : ColorText));
		const EBDPlacementRefusal UrnRefusal = bOver ? EBDPlacementRefusal::MatchRefused : BuildRefusal(Urn);
		const bool bUrnAvailable = UrnRefusal == EBDPlacementRefusal::None;
		UrnCount->SetColorAndOpacity(FSlateColor(
			bUrnHeld ? ColorPanelDark : (bUrnAvailable ? ColorMuted : ColorRed)));
		UrnCount->SetText(BuildInfoText(Urn, UrnRefusal));
		UrnButton->SetIsEnabled(bUrnAvailable);
	}

	for (int32 Index = 0; Index < PaletteData.Num(); ++Index)
	{
		const UBDPlaceableData* Data = PaletteData[Index];
		const EBDPieceKind Kind = Data->GetPieceKind();
		FFormatNamedArguments Args;
		Args.Add(TEXT("Key"), FText::AsNumber(Index + 1));
		Args.Add(TEXT("Name"), BDLoc::PieceName(Data));
		BuildLabels[Index]->SetText(BDLoc::Format(TEXT("HUD.Build.Entry"), Args));
		const bool bHeld = Held == Data;
		BuildButtons[Index]->SetBackgroundColor(bHeld ? ColorButton : ColorButtonIdle);
		BuildLabels[Index]->SetColorAndOpacity(FSlateColor(bHeld ? ColorPanelDark : ColorText));
		// What is left and what it costs, or - when the piece cannot be taken - the reason,
		// in red. A button that only greys out is the thing the player complained about:
		// whatever stops the piece is now written under it.
		const EBDPlacementRefusal Refusal = bOver ? EBDPlacementRefusal::MatchRefused : BuildRefusal(Data);
		const bool bAvailable = Refusal == EBDPlacementRefusal::None;
		BuildCounts[Index]->SetColorAndOpacity(FSlateColor(
			bHeld ? ColorPanelDark : (bAvailable ? ColorMuted : ColorRed)));
		BuildCounts[Index]->SetText(BuildInfoText(Data, Refusal));
		BuildButtons[Index]->SetIsEnabled(bAvailable);
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
	Placement->TakeIntoHand(PaletteData[Index]);
}

void UBDHUDWidget::HandleBuildUrn()
{
	UBDPlacementComponent* Placement = GetPlacement();
	UBDPlaceableData* Urn = UBDObjectiveSettings::Get().ObjectivePlaceable.LoadSynchronous();
	if (Placement != nullptr && Urn != nullptr)
	{
		Placement->TakeIntoHand(Urn);
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

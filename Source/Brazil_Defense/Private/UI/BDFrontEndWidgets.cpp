// Brazil Defense. The screens before the game: splash, loading and the main menu.

#include "UI/BDFrontEndWidgets.h"

#include "BDBuildInfo.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CircularThrobber.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Match/BDDifficultyData.h"
#include "Match/BDGameBalanceSettings.h"
#include "Match/BDMatchManager.h"
#include "Save/BDProgressSave.h"
#include "Misc/App.h"
#include "UI/BDUISettings.h"
#include "UI/BDUISubsystem.h"

namespace BDFrontEndPrivate
{
	static constexpr int32 BrandFontSize = 48;
	static constexpr int32 TitleFontSize = 40;
	static constexpr int32 HintFontSize = 14;
	static constexpr int32 BuildFontSize = 11;
	static constexpr int32 ButtonFontSize = 22;
	static constexpr float ButtonGap = 8.0f;
	static constexpr float MenuColumnWidth = 320.0f;

	/** Flat stand-in for the menu cinematic: the navy of the logo, a shade lighter than the splash. */
	static const FLinearColor MenuBackground = FLinearColor::FromSRGBColor(FColor(0x16, 0x20, 0x3E));

	/** Something centered on the screen inside a full-screen border of one color. */
	static UBorder* Centered(UWidgetTree& Tree, UWidget* Content, const FLinearColor& Background)
	{
		UBorder* Screen = Tree.ConstructWidget<UBorder>(UBorder::StaticClass());
		Screen->SetBrushColor(Background);
		Screen->SetHorizontalAlignment(HAlign_Center);
		Screen->SetVerticalAlignment(VAlign_Center);
		Screen->AddChild(Content);
		return Screen;
	}
}

//~ Splash --------------------------------------------------------------------------

void UBDSplashWidget::BuildTree()
{
	using namespace BDFrontEndPrivate;

	UVerticalBox* Column = MakeColumn();

	// The logo, when the settings name one; the mount point for the video later.
	const UBDUISettings& Settings = UBDUISettings::Get();
	if (UTexture2D* LogoTexture = Settings.SplashLogo.LoadSynchronous())
	{
		Logo = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		const float Aspect = LogoTexture->GetSizeY() > 0 ? static_cast<float>(LogoTexture->GetSizeX()) / LogoTexture->GetSizeY() : 1.0f;
		FSlateBrush Brush;
		Brush.SetResourceObject(LogoTexture);
		Brush.ImageSize = FVector2D(Settings.SplashLogoHeight * Aspect, Settings.SplashLogoHeight);
		Logo->SetBrush(Brush);
		Logo->SetVisibility(ESlateVisibility::HitTestInvisible);
		UVerticalBoxSlot* LogoSlot = Column->AddChildToVerticalBox(Logo);
		LogoSlot->SetHorizontalAlignment(HAlign_Center);
		LogoSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));
	}

	Brand = MakeText(BrandFontSize);
	Brand->SetJustification(ETextJustify::Center);
	Column->AddChildToVerticalBox(Brand);

	SkipHint = MakeText(HintFontSize, ColorMuted);
	SkipHint->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* HintSlot = Column->AddChildToVerticalBox(SkipHint);
	HintSlot->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));

	WidgetTree->RootWidget = Centered(*WidgetTree, Column, ColorPanelDark);

	// Any key skips: the widget has to hold the keyboard to hear one.
	SetIsFocusable(true);
}

void UBDSplashWidget::RefreshTexts()
{
	Brand->SetText(Loc(TEXT("Splash.Brand")));
	SkipHint->SetText(Loc(TEXT("Splash.SkipHint")));
}

void UBDSplashWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bFinished = false;
	SetKeyboardFocus();
}

FReply UBDSplashWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	Finish();
	return FReply::Handled();
}

FReply UBDSplashWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Finish();
	return FReply::Handled();
}

void UBDSplashWidget::Finish()
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	if (UBDUISubsystem* UI = GetUI())
	{
		UI->FinishSplash();
	}
}

//~ Loading -------------------------------------------------------------------------

void UBDLoadingWidget::BuildTree()
{
	using namespace BDFrontEndPrivate;

	UVerticalBox* Column = MakeColumn();

	UCircularThrobber* Spinner = WidgetTree->ConstructWidget<UCircularThrobber>(UCircularThrobber::StaticClass());
	UVerticalBoxSlot* SpinnerSlot = Column->AddChildToVerticalBox(Spinner);
	SpinnerSlot->SetHorizontalAlignment(HAlign_Center);

	Label = MakeText(ButtonFontSize);
	Label->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* LabelSlot = Column->AddChildToVerticalBox(Label);
	LabelSlot->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));

	WidgetTree->RootWidget = Centered(*WidgetTree, Column, ColorPanelDark);
}

void UBDLoadingWidget::RefreshTexts()
{
	Label->SetText(Loc(TEXT("Loading.Label")));
}

//~ Main menu -----------------------------------------------------------------------

void UBDMainMenuWidget::BuildTree()
{
	using namespace BDFrontEndPrivate;

	UVerticalBox* Column = MakeColumn();

	Title = MakeText(TitleFontSize);
	Title->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 32.0f));

	const auto AddButton = [this, Column](TObjectPtr<UTextBlock>& Label, UButton*& OutButton)
	{
		OutButton = MakeButton(Label, ButtonFontSize);
		Label->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(OutButton);
		ButtonSlot->SetPadding(FMargin(0.0f, ButtonGap));
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	};

	UButton* PlayButton = nullptr;
	UButton* ContinueMenuButton = nullptr;
	UButton* OptionsButton = nullptr;
	UButton* QuitButton = nullptr;
	AddButton(PlayLabel, PlayButton);
	AddButton(ContinueLabel, ContinueMenuButton);
	AddButton(OptionsLabel, OptionsButton);
	AddButton(QuitLabel, QuitButton);

	PlayButton->OnClicked.AddDynamic(this, &UBDMainMenuWidget::HandlePlay);
	ContinueMenuButton->OnClicked.AddDynamic(this, &UBDMainMenuWidget::HandleContinue);
	ContinueButton = ContinueMenuButton;
	// Only when there is a match to go back to.
	ContinueButton->SetVisibility(ABDMatchManager::HasSavedMatch() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	OptionsButton->OnClicked.AddDynamic(this, &UBDMainMenuWidget::HandleOptions);
	QuitButton->OnClicked.AddDynamic(this, &UBDMainMenuWidget::HandleQuit);

	// The column has a fixed width so the buttons line up whatever their words measure.
	USizeBox* ColumnBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ColumnBox->SetMinDesiredWidth(MenuColumnWidth);
	ColumnBox->AddChild(Column);

	// The build in the footer, small, so a tester always knows which one they are on.
	UOverlay* Screen = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	UOverlaySlot* MenuSlot = Screen->AddChildToOverlay(Centered(*WidgetTree, ColumnBox, MenuBackground));
	MenuSlot->SetHorizontalAlignment(HAlign_Fill);
	MenuSlot->SetVerticalAlignment(VAlign_Fill);
	UTextBlock* Build = MakeText(BuildFontSize, ColorMuted);
	Build->SetText(FText::FromString(BDBuildInfo::GetLabel()));
	UOverlaySlot* BuildSlot = Screen->AddChildToOverlay(Build);
	BuildSlot->SetHorizontalAlignment(HAlign_Right);
	BuildSlot->SetVerticalAlignment(VAlign_Bottom);
	BuildSlot->SetPadding(FMargin(12.0f));
	WidgetTree->RootWidget = Screen;
}

void UBDMainMenuWidget::RefreshTexts()
{
	Title->SetText(Loc(TEXT("Game.Title")));
	PlayLabel->SetText(Loc(TEXT("Menu.Play")));
	ContinueLabel->SetText(Loc(TEXT("Menu.Continue")));
	OptionsLabel->SetText(Loc(TEXT("Menu.Options")));
	QuitLabel->SetText(Loc(TEXT("Menu.Quit")));
}

void UBDMainMenuWidget::HandlePlay()
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->OpenDifficultySelect();
	}
}

void UBDMainMenuWidget::HandleContinue()
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->ContinueGame();
	}
}

void UBDMainMenuWidget::HandleOptions()
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->OpenOptions();
	}
}

void UBDMainMenuWidget::HandleQuit()
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->QuitGame();
	}
}

//~ Difficulty ----------------------------------------------------------------------

void UBDDifficultySelectWidget::BuildTree()
{
	using namespace BDFrontEndPrivate;

	UVerticalBox* Column = MakeColumn();

	Title = MakeText(TitleFontSize);
	Title->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 32.0f));

	// One button and one line of words per difficulty, in enum order.
	UButton* Buttons[DifficultyCount] = {};
	for (int32 Index = 0; Index < DifficultyCount; ++Index)
	{
		Buttons[Index] = MakeButton(Labels[Index], ButtonFontSize);
		Labels[Index]->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(Buttons[Index]);
		ButtonSlot->SetPadding(FMargin(0.0f, ButtonGap, 0.0f, 0.0f));
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);

		Summaries[Index] = MakeText(HintFontSize, ColorMuted);
		Summaries[Index]->SetJustification(ETextJustify::Center);
		Summaries[Index]->SetAutoWrapText(true);
		UVerticalBoxSlot* SummarySlot = Column->AddChildToVerticalBox(Summaries[Index]);
		SummarySlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, ButtonGap));
	}
	Buttons[static_cast<int32>(EBDDifficulty::Easy)]->OnClicked.AddDynamic(this, &UBDDifficultySelectWidget::HandleEasy);
	Buttons[static_cast<int32>(EBDDifficulty::Normal)]->OnClicked.AddDynamic(this, &UBDDifficultySelectWidget::HandleNormal);
	Buttons[static_cast<int32>(EBDDifficulty::Hard)]->OnClicked.AddDynamic(this, &UBDDifficultySelectWidget::HandleHard);

	UButton* BackButton = MakeButton(BackLabel, ButtonFontSize);
	BackLabel->SetJustification(ETextJustify::Center);
	BackButton->OnClicked.AddDynamic(this, &UBDDifficultySelectWidget::HandleBack);
	UVerticalBoxSlot* BackSlot = Column->AddChildToVerticalBox(BackButton);
	BackSlot->SetPadding(FMargin(0.0f, 24.0f, 0.0f, 0.0f));
	BackSlot->SetHorizontalAlignment(HAlign_Fill);

	USizeBox* ColumnBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ColumnBox->SetMinDesiredWidth(MenuColumnWidth);
	ColumnBox->SetMaxDesiredWidth(MenuColumnWidth * 1.5f);
	ColumnBox->AddChild(Column);

	WidgetTree->RootWidget = Centered(*WidgetTree, ColumnBox, MenuBackground);
}

void UBDDifficultySelectWidget::RefreshTexts()
{
	static const TCHAR* const NameKeys[DifficultyCount] = { TEXT("Difficulty.Easy"), TEXT("Difficulty.Normal"), TEXT("Difficulty.Hard") };

	Title->SetText(Loc(TEXT("Difficulty.Title")));
	for (int32 Index = 0; Index < DifficultyCount; ++Index)
	{
		const EBDDifficulty Difficulty = static_cast<EBDDifficulty>(Index);
		const bool bWon = UBDProgressSave::HasWonDifficulty(Difficulty);
		Labels[Index]->SetText(bWon
			? FText::Format(FText::FromString(TEXT("{0} {1}")), Loc(NameKeys[Index]), Loc(TEXT("Difficulty.Won")))
			: Loc(NameKeys[Index]));
		Summaries[Index]->SetText(Summary(Difficulty));
	}
	BackLabel->SetText(Loc(TEXT("Difficulty.Back")));
}

FText UBDDifficultySelectWidget::Summary(const EBDDifficulty Difficulty) const
{
	const UBDDifficultyData* Data = UBDGameBalanceSettings::Get().FindDifficultyData(Difficulty);
	if (Data == nullptr)
	{
		Data = GetDefault<UBDDifficultyData>();
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Waves"), Data->WavesToWin);
	Args.Add(TEXT("Prisoners"), Data->PrisonersFreed);
	Args.Add(TEXT("Dividers"), Data->DividerBudget);
	Args.Add(TEXT("Platforms"), Data->PlatformBudget);
	// The opening capital took the place of the tower and character counts: what the
	// player walks in with is public money, and how many defenders it buys is up to them.
	Args.Add(TEXT("Funds"), Data->StartingFunds);
	Args.Add(TEXT("Saves"), Data->SaveBudget);
	FText Text = BDLoc::Format(TEXT("Difficulty.Summary"), Args);

	// The chain: what a win below adds, and whether it is in yet.
	const EBDDifficulty Below = ABDMatchManager::GetDifficultyBelow(Difficulty);
	if (Below != EBDDifficulty::Count && !Data->ChainBonus.IsEmpty())
	{
		static const TCHAR* const NameKeys[DifficultyCount] = { TEXT("Difficulty.Easy"), TEXT("Difficulty.Normal"), TEXT("Difficulty.Hard") };
		FFormatNamedArguments BonusArgs;
		BonusArgs.Add(TEXT("Below"), Loc(NameKeys[static_cast<int32>(Below)]));
		const FText Bonus = BDLoc::Format(UBDProgressSave::HasWonDifficulty(Below) ? TEXT("Difficulty.BonusIn") : TEXT("Difficulty.BonusLocked"), BonusArgs);
		Text = FText::Format(FText::FromString(TEXT("{0}\n{1}")), Text, Bonus);
	}

	return Text;
}

void UBDDifficultySelectWidget::Pick(const EBDDifficulty Difficulty)
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->PlayGame(Difficulty);
	}
}

void UBDDifficultySelectWidget::HandleEasy()
{
	Pick(EBDDifficulty::Easy);
}

void UBDDifficultySelectWidget::HandleNormal()
{
	Pick(EBDDifficulty::Normal);
}

void UBDDifficultySelectWidget::HandleHard()
{
	Pick(EBDDifficulty::Hard);
}

void UBDDifficultySelectWidget::HandleBack()
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->CloseDifficultySelect();
	}
}


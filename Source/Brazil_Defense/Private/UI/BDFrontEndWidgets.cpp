// Brazil Defense. The screens before the game: splash, loading and the main menu.

#include "UI/BDFrontEndWidgets.h"

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
#include "Misc/App.h"
#include "UI/BDUISettings.h"
#include "UI/BDUISubsystem.h"

namespace BDFrontEndPrivate
{
	static constexpr int32 BrandFontSize = 48;
	static constexpr int32 TitleFontSize = 40;
	static constexpr int32 HintFontSize = 14;
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
	UButton* OptionsButton = nullptr;
	UButton* QuitButton = nullptr;
	AddButton(PlayLabel, PlayButton);
	AddButton(OptionsLabel, OptionsButton);
	AddButton(QuitLabel, QuitButton);

	PlayButton->OnClicked.AddDynamic(this, &UBDMainMenuWidget::HandlePlay);
	OptionsButton->OnClicked.AddDynamic(this, &UBDMainMenuWidget::HandleOptions);
	QuitButton->OnClicked.AddDynamic(this, &UBDMainMenuWidget::HandleQuit);

	// The column has a fixed width so the buttons line up whatever their words measure.
	USizeBox* ColumnBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ColumnBox->SetMinDesiredWidth(MenuColumnWidth);
	ColumnBox->AddChild(Column);

	WidgetTree->RootWidget = Centered(*WidgetTree, ColumnBox, MenuBackground);
}

void UBDMainMenuWidget::RefreshTexts()
{
	Title->SetText(Loc(TEXT("Game.Title")));
	PlayLabel->SetText(Loc(TEXT("Menu.Play")));
	OptionsLabel->SetText(Loc(TEXT("Menu.Options")));
	QuitLabel->SetText(Loc(TEXT("Menu.Quit")));
}

void UBDMainMenuWidget::HandlePlay()
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->PlayGame();
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

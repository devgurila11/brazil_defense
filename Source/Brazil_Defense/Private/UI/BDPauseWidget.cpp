// Brazil Defense. The in-game menu: continue, options, back to the menu, quit.

#include "UI/BDPauseWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/BDUISubsystem.h"

namespace BDPausePrivate
{
	static constexpr int32 TitleFontSize = 32;
	static constexpr int32 ButtonFontSize = 22;
	static constexpr float ButtonGap = 8.0f;
	static constexpr float ColumnWidth = 320.0f;
}

void UBDPauseWidget::BuildTree()
{
	using namespace BDPausePrivate;

	UVerticalBox* Column = MakeColumn();

	Title = MakeText(TitleFontSize);
	Title->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title);
	TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 24.0f));

	const auto AddButton = [this, Column](TObjectPtr<UTextBlock>& Label, UButton*& OutButton)
	{
		OutButton = MakeButton(Label, ButtonFontSize);
		Label->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(OutButton);
		ButtonSlot->SetPadding(FMargin(0.0f, ButtonGap));
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	};

	UButton* ContinueButton = nullptr;
	UButton* OptionsButton = nullptr;
	UButton* MainMenuButton = nullptr;
	UButton* QuitButton = nullptr;
	AddButton(ContinueLabel, ContinueButton);
	AddButton(OptionsLabel, OptionsButton);
	AddButton(MainMenuLabel, MainMenuButton);
	AddButton(QuitLabel, QuitButton);

	ContinueButton->OnClicked.AddDynamic(this, &UBDPauseWidget::HandleContinue);
	OptionsButton->OnClicked.AddDynamic(this, &UBDPauseWidget::HandleOptions);
	MainMenuButton->OnClicked.AddDynamic(this, &UBDPauseWidget::HandleMainMenu);
	QuitButton->OnClicked.AddDynamic(this, &UBDPauseWidget::HandleQuit);

	USizeBox* ColumnBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ColumnBox->SetMinDesiredWidth(ColumnWidth);
	UBorder* Panel = MakeBox(ColorPanel, 24.0f);
	Panel->AddChild(Column);
	ColumnBox->AddChild(Panel);

	// A dimmed screen that takes every click, so nothing under it is placed by accident.
	UBorder* Screen = MakeBox(FLinearColor(ColorPanelDark.R, ColorPanelDark.G, ColorPanelDark.B, 0.7f), 0.0f);
	Screen->SetHorizontalAlignment(HAlign_Center);
	Screen->SetVerticalAlignment(VAlign_Center);
	Screen->AddChild(ColumnBox);

	WidgetTree->RootWidget = Screen;
}

void UBDPauseWidget::RefreshTexts()
{
	Title->SetText(Loc(TEXT("Pause.Title")));
	ContinueLabel->SetText(Loc(TEXT("Pause.Continue")));
	OptionsLabel->SetText(Loc(TEXT("Menu.Options")));
	MainMenuLabel->SetText(Loc(TEXT("Pause.MainMenu")));
	QuitLabel->SetText(Loc(TEXT("Menu.Quit")));
}

void UBDPauseWidget::HandleContinue()
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->ClosePauseMenu();
	}
}

void UBDPauseWidget::HandleOptions()
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->OpenOptions();
	}
}

void UBDPauseWidget::HandleMainMenu()
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->ReturnToMenu();
	}
}

void UBDPauseWidget::HandleQuit()
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->QuitGame();
	}
}

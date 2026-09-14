// Brazil Defense. What every screen of the interface is built on.

#include "UI/BDWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "UI/BDSettingsSubsystem.h"
#include "UI/BDUISubsystem.h"

// sRGB of the logo: #1A9BDB cyan, #101830 navy, #F8F8F8 white.
const FLinearColor UBDWidgetBase::ColorBlue = FLinearColor::FromSRGBColor(FColor(0x1A, 0x9B, 0xDB));
const FLinearColor UBDWidgetBase::ColorRed = FLinearColor::FromSRGBColor(FColor(0xE0, 0x4A, 0x4A));
const FLinearColor UBDWidgetBase::ColorMuted = FLinearColor::FromSRGBColor(FColor(0x9F, 0xB3, 0xC8));
const FLinearColor UBDWidgetBase::ColorPanel = FLinearColor::FromSRGBColor(FColor(0x10, 0x18, 0x30, 0xD9));
const FLinearColor UBDWidgetBase::ColorPanelDark = FLinearColor::FromSRGBColor(FColor(0x10, 0x18, 0x30));
const FLinearColor UBDWidgetBase::ColorHighlight = FLinearColor::FromSRGBColor(FColor(0x1A, 0x9B, 0xDB));
const FLinearColor UBDWidgetBase::ColorText = FLinearColor::FromSRGBColor(FColor(0xF8, 0xF8, 0xF8));
const FLinearColor UBDWidgetBase::ColorButton = FLinearColor::FromSRGBColor(FColor(0x1A, 0x9B, 0xDB));
const FLinearColor UBDWidgetBase::ColorButtonIdle = FLinearColor::FromSRGBColor(FColor(0x2A, 0x3A, 0x5C));

void UBDWidgetBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BuildTree();
	RefreshTexts();

	LanguageChangedHandle = BDLoc::OnLanguageChanged().AddUObject(this, &UBDWidgetBase::HandleLanguageChanged);
}

void UBDWidgetBase::NativeDestruct()
{
	BDLoc::OnLanguageChanged().Remove(LanguageChangedHandle);
	Super::NativeDestruct();
}

void UBDWidgetBase::HandleLanguageChanged(const EBDLanguage NewLanguage)
{
	RefreshTexts();
}

UBDUISubsystem* UBDWidgetBase::GetUI() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UBDUISubsystem>() : nullptr;
}

UBDSettingsSubsystem* UBDWidgetBase::GetSettings() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance != nullptr ? GameInstance->GetSubsystem<UBDSettingsSubsystem>() : nullptr;
}

UTextBlock* UBDWidgetBase::MakeText(const int32 FontSize, const FLinearColor& Color) const
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = Text->GetFont();
	Font.Size = FontSize;
	Text->SetFont(Font);
	// White means the off-white of the palette; any other color is meant as given.
	Text->SetColorAndOpacity(FSlateColor(Color == FLinearColor::White ? ColorText : Color));
	// Words are never clicked: whatever is under them, a button or the board, gets the mouse.
	Text->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Text;
}

UButton* UBDWidgetBase::MakeButton(TObjectPtr<UTextBlock>& OutLabel, const int32 FontSize) const
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetBackgroundColor(ColorButton);
	OutLabel = MakeText(FontSize, ColorPanelDark);
	Button->AddChild(OutLabel);
	return Button;
}

UBorder* UBDWidgetBase::MakeBox(const FLinearColor& Background, const float InPadding) const
{
	UBorder* Box = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());

	// Every panel of the game is a rounded box: the corner is the one bit of style the
	// raw layer already commits to.
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
	Brush.TintColor = FSlateColor(FLinearColor::White);
	Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	Brush.OutlineSettings.CornerRadii = FVector4(BoxCornerRadius, BoxCornerRadius, BoxCornerRadius, BoxCornerRadius);
	Brush.OutlineSettings.Width = 0.0f;
	Box->SetBrush(Brush);
	Box->SetBrushColor(Background);
	Box->SetPadding(FMargin(InPadding));
	return Box;
}

UVerticalBox* UBDWidgetBase::MakeColumn() const
{
	// A layout box takes no clicks of its own; only what it holds does.
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Column->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return Column;
}

UHorizontalBox* UBDWidgetBase::MakeRow() const
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return Row;
}

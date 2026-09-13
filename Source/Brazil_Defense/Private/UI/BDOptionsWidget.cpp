// Brazil Defense. The options panel: graphics, audio and language, applied as they change.

#include "UI/BDOptionsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/BDSettingsSubsystem.h"
#include "UI/BDUISubsystem.h"

namespace BDOptionsPrivate
{
	static constexpr int32 TitleFontSize = 32;
	static constexpr int32 HeadingFontSize = 22;
	static constexpr int32 RowFontSize = 18;
	static constexpr float PanelWidth = 640.0f;
	static constexpr float LabelWidth = 220.0f;
	static constexpr float RowGap = 6.0f;
	static constexpr float SectionGap = 24.0f;
	static constexpr int32 VolumeMax = 100;

	/** Quality entries as the list shows them: index 0 is Auto, then Low..Epic = levels 0..3. */
	static const TCHAR* const QualityKeys[] = {
		TEXT("Options.Quality.Auto"), TEXT("Options.Quality.Low"), TEXT("Options.Quality.Medium"),
		TEXT("Options.Quality.High"), TEXT("Options.Quality.Epic") };

	static const TCHAR* const WindowModeKeys[] = {
		TEXT("Options.Window.Fullscreen"), TEXT("Options.Window.Windowed"), TEXT("Options.Window.Borderless") };

	static FString ResolutionText(const FIntPoint& Resolution)
	{
		return FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y);
	}
}

void UBDOptionsWidget::AddSection(UVerticalBox* Column, TObjectPtr<UTextBlock>& OutHeading)
{
	using namespace BDOptionsPrivate;

	OutHeading = MakeText(HeadingFontSize, ColorHighlight);
	UVerticalBoxSlot* HeadingSlot = Column->AddChildToVerticalBox(OutHeading);
	HeadingSlot->SetPadding(FMargin(0.0f, SectionGap, 0.0f, RowGap));
}

void UBDOptionsWidget::AddRow(UVerticalBox* Column, TObjectPtr<UTextBlock>& OutLabel, UWidget* Control)
{
	using namespace BDOptionsPrivate;

	UHorizontalBox* Row = MakeRow();

	OutLabel = MakeText(RowFontSize);
	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(LabelWidth);
	LabelBox->AddChild(OutLabel);
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelBox);
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	UHorizontalBoxSlot* ControlSlot = Row->AddChildToHorizontalBox(Control);
	ControlSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ControlSlot->SetVerticalAlignment(VAlign_Center);

	UVerticalBoxSlot* RowSlot = Column->AddChildToVerticalBox(Row);
	RowSlot->SetPadding(FMargin(0.0f, RowGap));
}

void UBDOptionsWidget::BuildTree()
{
	using namespace BDOptionsPrivate;

	UVerticalBox* Column = MakeColumn();

	Title = MakeText(TitleFontSize);
	Column->AddChildToVerticalBox(Title);

	//~ Graphics
	AddSection(Column, GraphicsHeading);

	QualityList = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	QualityList->OnSelectionChanged.AddDynamic(this, &UBDOptionsWidget::HandleQualityChanged);
	AddRow(Column, QualityLabel, QualityList);

	ResolutionList = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	ResolutionList->OnSelectionChanged.AddDynamic(this, &UBDOptionsWidget::HandleResolutionChanged);
	AddRow(Column, ResolutionLabel, ResolutionList);

	WindowModeList = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	WindowModeList->OnSelectionChanged.AddDynamic(this, &UBDOptionsWidget::HandleWindowModeChanged);
	AddRow(Column, WindowModeLabel, WindowModeList);

	VSyncBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	VSyncBox->OnCheckStateChanged.AddDynamic(this, &UBDOptionsWidget::HandleVSyncChanged);
	AddRow(Column, VSyncLabel, VSyncBox);

	//~ Audio
	AddSection(Column, AudioHeading);

	const auto MakeVolumeRow = [this, Column](TObjectPtr<UTextBlock>& Label, TObjectPtr<USlider>& Slider, TObjectPtr<UTextBlock>& Value)
	{
		UHorizontalBox* Row = MakeRow();
		Slider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass());
		Slider->SetMinValue(0.0f);
		Slider->SetMaxValue(static_cast<float>(VolumeMax));
		Slider->SetStepSize(1.0f);
		UHorizontalBoxSlot* SliderSlot = Row->AddChildToHorizontalBox(Slider);
		SliderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		SliderSlot->SetVerticalAlignment(VAlign_Center);

		Value = MakeText(RowFontSize, ColorMuted);
		UHorizontalBoxSlot* ValueSlot = Row->AddChildToHorizontalBox(Value);
		ValueSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));
		ValueSlot->SetVerticalAlignment(VAlign_Center);

		AddRow(Column, Label, Row);
	};

	MakeVolumeRow(MusicLabel, MusicSlider, MusicValue);
	MusicSlider->OnValueChanged.AddDynamic(this, &UBDOptionsWidget::HandleMusicChanged);

	MakeVolumeRow(EffectsLabel, EffectsSlider, EffectsValue);
	EffectsSlider->OnValueChanged.AddDynamic(this, &UBDOptionsWidget::HandleEffectsChanged);

	MutedBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass());
	MutedBox->OnCheckStateChanged.AddDynamic(this, &UBDOptionsWidget::HandleMutedChanged);
	AddRow(Column, MutedLabel, MutedBox);

	//~ Language
	AddSection(Column, LanguageHeading);

	LanguageList = WidgetTree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass());
	LanguageList->OnSelectionChanged.AddDynamic(this, &UBDOptionsWidget::HandleLanguageChanged);
	AddRow(Column, LanguageLabel, LanguageList);

	//~ Back
	UButton* BackButton = MakeButton(BackLabel, RowFontSize);
	BackButton->OnClicked.AddDynamic(this, &UBDOptionsWidget::HandleBack);
	UVerticalBoxSlot* BackSlot = Column->AddChildToVerticalBox(BackButton);
	BackSlot->SetPadding(FMargin(0.0f, SectionGap, 0.0f, 0.0f));
	BackSlot->SetHorizontalAlignment(HAlign_Left);

	// A panel of fixed width in the middle of a dimmed screen. It scrolls on a short window.
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	Scroll->AddChild(Column);

	USizeBox* PanelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PanelBox->SetWidthOverride(PanelWidth);
	UBorder* Panel = MakeBox(ColorPanel, 24.0f);
	Panel->AddChild(Scroll);
	PanelBox->AddChild(Panel);

	UBorder* Screen = MakeBox(FLinearColor(ColorPanelDark.R, ColorPanelDark.G, ColorPanelDark.B, 0.7f), 0.0f);
	Screen->SetHorizontalAlignment(HAlign_Center);
	Screen->SetVerticalAlignment(VAlign_Center);
	Screen->AddChild(PanelBox);

	WidgetTree->RootWidget = Screen;

	GatherResolutions();
}

void UBDOptionsWidget::GatherResolutions()
{
	Resolutions.Reset();

	TArray<FIntPoint> Supported;
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);
	for (const FIntPoint& Resolution : Supported)
	{
		Resolutions.AddUnique(Resolution);
	}

	// A headless or odd machine lists nothing; the common sizes keep the list usable.
	if (Resolutions.Num() == 0)
	{
		Resolutions.Append({ FIntPoint(1280, 720), FIntPoint(1600, 900), FIntPoint(1920, 1080), FIntPoint(2560, 1440) });
	}
}

void UBDOptionsWidget::RefreshTexts()
{
	using namespace BDOptionsPrivate;

	Title->SetText(Loc(TEXT("Options.Title")));
	GraphicsHeading->SetText(Loc(TEXT("Options.Graphics")));
	AudioHeading->SetText(Loc(TEXT("Options.Audio")));
	LanguageHeading->SetText(Loc(TEXT("Options.Language")));
	QualityLabel->SetText(Loc(TEXT("Options.Quality")));
	ResolutionLabel->SetText(Loc(TEXT("Options.Resolution")));
	WindowModeLabel->SetText(Loc(TEXT("Options.WindowMode")));
	VSyncLabel->SetText(Loc(TEXT("Options.VSync")));
	MusicLabel->SetText(Loc(TEXT("Options.MusicVolume")));
	EffectsLabel->SetText(Loc(TEXT("Options.EffectsVolume")));
	MutedLabel->SetText(Loc(TEXT("Options.Muted")));
	LanguageLabel->SetText(Loc(TEXT("Options.LanguageSelect")));
	BackLabel->SetText(Loc(TEXT("Options.Back")));

	// The lists are strings: rebuilt in the current language, then pointed back at the
	// saved values without firing anything.
	bSyncing = true;

	QualityList->ClearOptions();
	for (const TCHAR* Key : QualityKeys)
	{
		QualityList->AddOption(Loc(Key).ToString());
	}

	ResolutionList->ClearOptions();
	ResolutionList->AddOption(Loc(TEXT("Options.Resolution.Display")).ToString());
	for (const FIntPoint& Resolution : Resolutions)
	{
		ResolutionList->AddOption(ResolutionText(Resolution));
	}

	WindowModeList->ClearOptions();
	for (const TCHAR* Key : WindowModeKeys)
	{
		WindowModeList->AddOption(Loc(Key).ToString());
	}

	LanguageList->ClearOptions();
	for (int32 Index = 0; Index < static_cast<int32>(EBDLanguage::Count); ++Index)
	{
		LanguageList->AddOption(BDLoc::GetLanguageName(static_cast<EBDLanguage>(Index)).ToString());
	}

	bSyncing = false;

	SyncFromSettings();
}

void UBDOptionsWidget::SyncFromSettings()
{
	using namespace BDOptionsPrivate;

	const UBDSettingsSubsystem* Subsystem = GetSettings();
	if (Subsystem == nullptr)
	{
		return;
	}

	const UBDSettingsSave& Saved = Subsystem->Get();
	bSyncing = true;

	QualityList->SetSelectedIndex(Saved.QualityLevel < 0 ? 0 : FMath::Clamp(Saved.QualityLevel, 0, 3) + 1);

	int32 ResolutionIndex = 0;
	for (int32 Index = 0; Index < Resolutions.Num(); ++Index)
	{
		if (Resolutions[Index] == Saved.Resolution)
		{
			ResolutionIndex = Index + 1;
			break;
		}
	}
	ResolutionList->SetSelectedIndex(ResolutionIndex);

	WindowModeList->SetSelectedIndex(FMath::Clamp(static_cast<int32>(Saved.WindowMode), 0, static_cast<int32>(EBDWindowMode::Count) - 1));
	VSyncBox->SetIsChecked(Saved.bVSync);

	MusicSlider->SetValue(static_cast<float>(Saved.MusicVolume));
	MusicValue->SetText(FText::AsNumber(Saved.MusicVolume));
	EffectsSlider->SetValue(static_cast<float>(Saved.EffectsVolume));
	EffectsValue->SetText(FText::AsNumber(Saved.EffectsVolume));
	MutedBox->SetIsChecked(Saved.bMuted);

	LanguageList->SetSelectedIndex(FMath::Clamp(static_cast<int32>(Saved.Language), 0, static_cast<int32>(EBDLanguage::Count) - 1));

	bSyncing = false;
}

//~ Changes ----------------------------------------------------------------------

void UBDOptionsWidget::HandleQualityChanged(FString Item, const ESelectInfo::Type SelectInfo)
{
	if (bSyncing || SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	if (UBDSettingsSubsystem* Subsystem = GetSettings())
	{
		// Index 0 is Auto; the rest map to the engine levels one below.
		Subsystem->SetQualityLevel(QualityList->GetSelectedIndex() - 1);
	}
}

void UBDOptionsWidget::HandleResolutionChanged(FString Item, const ESelectInfo::Type SelectInfo)
{
	if (bSyncing || SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	if (UBDSettingsSubsystem* Subsystem = GetSettings())
	{
		const int32 Index = ResolutionList->GetSelectedIndex() - 1;
		Subsystem->SetResolution(Resolutions.IsValidIndex(Index) ? Resolutions[Index] : FIntPoint::ZeroValue);
	}
}

void UBDOptionsWidget::HandleWindowModeChanged(FString Item, const ESelectInfo::Type SelectInfo)
{
	if (bSyncing || SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	if (UBDSettingsSubsystem* Subsystem = GetSettings())
	{
		Subsystem->SetWindowMode(static_cast<EBDWindowMode>(FMath::Clamp(WindowModeList->GetSelectedIndex(), 0, static_cast<int32>(EBDWindowMode::Count) - 1)));
	}
}

void UBDOptionsWidget::HandleVSyncChanged(const bool bChecked)
{
	if (bSyncing)
	{
		return;
	}

	if (UBDSettingsSubsystem* Subsystem = GetSettings())
	{
		Subsystem->SetVSync(bChecked);
	}
}

void UBDOptionsWidget::HandleMusicChanged(const float Value)
{
	const int32 Volume = FMath::RoundToInt(Value);
	MusicValue->SetText(FText::AsNumber(Volume));
	if (bSyncing)
	{
		return;
	}

	if (UBDSettingsSubsystem* Subsystem = GetSettings())
	{
		Subsystem->SetMusicVolume(Volume);
	}
}

void UBDOptionsWidget::HandleEffectsChanged(const float Value)
{
	const int32 Volume = FMath::RoundToInt(Value);
	EffectsValue->SetText(FText::AsNumber(Volume));
	if (bSyncing)
	{
		return;
	}

	if (UBDSettingsSubsystem* Subsystem = GetSettings())
	{
		Subsystem->SetEffectsVolume(Volume);
	}
}

void UBDOptionsWidget::HandleMutedChanged(const bool bChecked)
{
	if (bSyncing)
	{
		return;
	}

	if (UBDSettingsSubsystem* Subsystem = GetSettings())
	{
		Subsystem->SetMuted(bChecked);
	}
}

void UBDOptionsWidget::HandleLanguageChanged(FString Item, const ESelectInfo::Type SelectInfo)
{
	if (bSyncing || SelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	// Setting the language rebuilds every text on screen, this panel included, and this
	// list with it. Not from inside the list's own event: it is done on the next tick.
	const EBDLanguage Language = static_cast<EBDLanguage>(FMath::Clamp(LanguageList->GetSelectedIndex(), 0, static_cast<int32>(EBDLanguage::Count) - 1));
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UBDOptionsWidget> WeakThis(this);
		World->GetTimerManager().SetTimerForNextTick([WeakThis, Language]()
		{
			UBDSettingsSubsystem* Subsystem = WeakThis.IsValid() ? WeakThis->GetSettings() : nullptr;
			if (Subsystem != nullptr)
			{
				Subsystem->SetLanguage(Language);
			}
		});
	}
}

void UBDOptionsWidget::HandleBack()
{
	if (UBDUISubsystem* UI = GetUI())
	{
		UI->CloseOptions();
	}
}

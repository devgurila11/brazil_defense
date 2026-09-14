// Brazil Defense. The options panel: graphics, audio and language, applied as they change.

#pragma once

#include "CoreMinimal.h"
#include "UI/BDWidgetBase.h"
#include "BDOptionsWidget.generated.h"

class UCheckBox;
class UComboBoxString;
class USlider;
class UTextBlock;

/**
 * Three sections of raw rows: a label and a control each. Every control writes
 * straight into UBDSettingsSubsystem, which applies and saves; there is no Apply or
 * Cancel. The lists are rebuilt in the current language on every refresh, since combo
 * entries are strings and cannot follow a language change on their own.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDOptionsWidget : public UBDWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void RefreshTexts() override;

private:
	/** Puts the saved values into every control, without firing their change events. */
	void SyncFromSettings();

	/** Row helpers: a label on the left, a control on the right. */
	void AddSection(class UVerticalBox* Column, TObjectPtr<UTextBlock>& OutHeading);
	void AddRow(class UVerticalBox* Column, TObjectPtr<UTextBlock>& OutLabel, class UWidget* Control);

	/** Resolutions offered: the display's own first, then what the hardware lists. */
	void GatherResolutions();

	UFUNCTION()
	void HandleQualityChanged(FString Item, ESelectInfo::Type SelectInfo);

	UFUNCTION()
	void HandleResolutionChanged(FString Item, ESelectInfo::Type SelectInfo);

	UFUNCTION()
	void HandleWindowModeChanged(FString Item, ESelectInfo::Type SelectInfo);

	UFUNCTION()
	void HandleVSyncChanged(bool bChecked);

	UFUNCTION()
	void HandleMusicChanged(float Value);

	UFUNCTION()
	void HandleEffectsChanged(float Value);

	UFUNCTION()
	void HandleMutedChanged(bool bChecked);

	UFUNCTION()
	void HandleInvertSidewaysChanged(bool bChecked);

	UFUNCTION()
	void HandleLanguageChanged(FString Item, ESelectInfo::Type SelectInfo);

	UFUNCTION()
	void HandleBack();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Title;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GraphicsHeading;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AudioHeading;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LanguageHeading;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QualityLabel;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> QualityList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ResolutionLabel;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> ResolutionList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WindowModeLabel;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> WindowModeList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> VSyncLabel;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> VSyncBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MusicLabel;

	UPROPERTY(Transient)
	TObjectPtr<USlider> MusicSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MusicValue;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EffectsLabel;

	UPROPERTY(Transient)
	TObjectPtr<USlider> EffectsSlider;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EffectsValue;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MutedLabel;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> MutedBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ControlsHeading;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InvertSidewaysLabel;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> InvertSidewaysBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LanguageLabel;

	UPROPERTY(Transient)
	TObjectPtr<UComboBoxString> LanguageList;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BackLabel;

	/** Index in ResolutionList minus one; entry 0 is "display". */
	TArray<FIntPoint> Resolutions;

	/** While the controls are being filled from the settings, their change events are noise. */
	bool bSyncing = false;
};

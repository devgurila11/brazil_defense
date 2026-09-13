// Brazil Defense. The screens before the game: splash, loading and the main menu.

#pragma once

#include "CoreMinimal.h"
#include "UI/BDWidgetBase.h"
#include "BDFrontEndWidgets.generated.h"

class UButton;
class UImage;
class UTextBlock;

/**
 * The brand card. A black screen with the studio name for a couple of seconds, skipped
 * by any key or click. Placeholder for the video: the widget is the mount point, a
 * MediaPlayer goes where the text is and the timing and the skip stay as they are.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDSplashWidget : public UBDWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void RefreshTexts() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	void Finish();

	UPROPERTY(Transient)
	TObjectPtr<UImage> Logo;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Brand;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SkipHint;

	bool bFinished = false;
};

/** A word and a spinner while something loads. Placeholder. */
UCLASS()
class BRAZIL_DEFENSE_API UBDLoadingWidget : public UBDWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void RefreshTexts() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Label;
};

/**
 * Play, Options, Quit, stacked. The background is a flat color standing in for the
 * looping cinematic: that goes behind the column, in the same border.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDMainMenuWidget : public UBDWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void RefreshTexts() override;

private:
	UFUNCTION()
	void HandlePlay();

	UFUNCTION()
	void HandleOptions();

	UFUNCTION()
	void HandleQuit();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Title;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlayLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OptionsLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QuitLabel;
};

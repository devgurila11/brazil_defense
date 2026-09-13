// Brazil Defense. The in-game menu: continue, options, back to the menu, quit.

#pragma once

#include "CoreMinimal.h"
#include "UI/BDWidgetBase.h"
#include "BDPauseWidget.generated.h"

class UTextBlock;

/**
 * Opened over the HUD by its Menu button or by Escape with nothing in hand. The match
 * is paused while it is up. Raw column of buttons like the main menu.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDPauseWidget : public UBDWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void RefreshTexts() override;

private:
	UFUNCTION()
	void HandleContinue();

	UFUNCTION()
	void HandleOptions();

	UFUNCTION()
	void HandleMainMenu();

	UFUNCTION()
	void HandleQuit();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Title;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ContinueLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> OptionsLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MainMenuLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QuitLabel;
};

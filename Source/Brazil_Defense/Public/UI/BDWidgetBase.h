// Brazil Defense. What every screen of the interface is built on.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/BDLocalization.h"
#include "BDWidgetBase.generated.h"

class UBDSettingsSubsystem;
class UBDUISubsystem;
class UBorder;
class UButton;
class UHorizontalBox;
class UTextBlock;
class UVerticalBox;
class UWidget;

/**
 * A screen built in code, with no Blueprint and no style: the widget tree is assembled
 * in BuildTree from plain UMG boxes, text and buttons, and every text on it is filled
 * in RefreshTexts, which runs once after the build and again whenever the language
 * changes. That split is the whole discipline of the raw interface: layout in one
 * place, words in another, and nothing typed that is not a key in the text tables.
 *
 * The visual pass comes later, on top: a Blueprint child can restyle what is here or
 * replace the tree, and the flow in UBDUISubsystem does not care which.
 */
UCLASS(Abstract)
class BRAZIL_DEFENSE_API UBDWidgetBase : public UUserWidget
{
	GENERATED_BODY()

protected:
	//~ Begin UUserWidget interface
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	//~ End UUserWidget interface

	/** Builds the widget tree and sets WidgetTree->RootWidget. Runs once. */
	virtual void BuildTree() {}

	/** Puts every text in place, in the current language. Runs after BuildTree and on every language change. */
	virtual void RefreshTexts() {}

	UBDUISubsystem* GetUI() const;
	UBDSettingsSubsystem* GetSettings() const;

	//~ Raw building blocks. Sizes in Slate units, colors for telling things apart, nothing more. --

	UTextBlock* MakeText(int32 FontSize = 18, const FLinearColor& Color = FLinearColor::White) const;

	/** A button with one text inside. The label comes back so RefreshTexts can fill it. */
	UButton* MakeButton(TObjectPtr<UTextBlock>& OutLabel, int32 FontSize = 18) const;

	/** A padded box with a solid color behind whatever is put in it. */
	UBorder* MakeBox(const FLinearColor& Background, float InPadding = 12.0f) const;

	UVerticalBox* MakeColumn() const;
	UHorizontalBox* MakeRow() const;

	/** The text of a key in the current language. */
	static FText Loc(const TCHAR* Key) { return BDLoc::Text(Key); }

	/**
	 * The palette of the interface, taken from the studio logo: navy ground, cyan accent,
	 * off-white text. Red is kept for what is red by meaning: the other side's votes and
	 * a warning. Everything on every screen draws from these six.
	 */
	static const FLinearColor ColorBlue;      // cyan of the logo: the player's votes, the accent
	static const FLinearColor ColorRed;       // the other side, and warnings
	static const FLinearColor ColorMuted;     // secondary text, blue-grey
	static const FLinearColor ColorPanel;     // navy panel over the board, translucent
	static const FLinearColor ColorPanelDark; // navy of the logo: screens, fade
	static const FLinearColor ColorHighlight; // titles and the current choice: the cyan again
	static const FLinearColor ColorText;      // off-white of the logo
	static const FLinearColor ColorButton;    // buttons: cyan, with navy words on them
	static const FLinearColor ColorButtonIdle;// a button that is not the current choice

private:
	void HandleLanguageChanged(EBDLanguage NewLanguage);

	FDelegateHandle LanguageChangedHandle;
};

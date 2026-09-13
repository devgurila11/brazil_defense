// Brazil Defense. The black over everything that screens fade through.

#pragma once

#include "CoreMinimal.h"
#include "UI/BDWidgetBase.h"
#include "BDFadeWidget.generated.h"

class UBorder;

/**
 * A full screen black layer whose opacity is driven in real time, so a fade runs at
 * the same pace whatever the game speed or a frozen board says. Never takes input:
 * whatever is under it stays clickable, which is fine for a half second.
 *
 * Driven by the core ticker rather than the widget tick: a fade has to finish, and
 * call back, even on a frame nothing is painted (a headless run, a hidden window).
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDFadeWidget : public UBDWidgetBase
{
	GENERATED_BODY()

public:
	/** Runs the opacity from where it is to Target over Seconds, then calls OnDone (may be empty). */
	void FadeTo(float Target, float Seconds, TFunction<void()> OnDone);

	/** Sets the opacity outright. */
	void SetBlack(float Opacity);

	bool IsFading() const { return bFading; }

protected:
	virtual void BuildTree() override;
	virtual void NativeDestruct() override;

private:
	/** One step of the running fade. @return whether it keeps running. */
	bool Step(float DeltaSeconds);

	void StopTicker();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> Black;

	FTSTicker::FDelegateHandle TickerHandle;

	float Opacity = 0.0f;
	float StartOpacity = 0.0f;
	float TargetOpacity = 0.0f;
	float Duration = 0.0f;
	float Elapsed = 0.0f;
	bool bFading = false;
	TFunction<void()> Done;
};

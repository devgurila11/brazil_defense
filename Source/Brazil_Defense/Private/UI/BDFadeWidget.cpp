// Brazil Defense. The black over everything that screens fade through.

#include "UI/BDFadeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Containers/Ticker.h"

void UBDFadeWidget::BuildTree()
{
	Black = MakeBox(ColorPanelDark, 0.0f);
	WidgetTree->RootWidget = Black;

	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetBlack(0.0f);
}

void UBDFadeWidget::NativeDestruct()
{
	StopTicker();
	Super::NativeDestruct();
}

void UBDFadeWidget::StopTicker()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}
}

void UBDFadeWidget::SetBlack(const float InOpacity)
{
	StopTicker();
	Opacity = FMath::Clamp(InOpacity, 0.0f, 1.0f);
	bFading = false;
	Done = nullptr;
	SetRenderOpacity(Opacity);
}

void UBDFadeWidget::FadeTo(const float Target, const float Seconds, TFunction<void()> OnDone)
{
	StartOpacity = Opacity;
	TargetOpacity = FMath::Clamp(Target, 0.0f, 1.0f);
	Duration = FMath::Max(0.0f, Seconds);
	Elapsed = 0.0f;
	Done = MoveTemp(OnDone);
	bFading = true;

	// Even a zero length fade finishes on a later tick, never inside the call, so a
	// caller that chains fades never re-enters itself.
	StopTicker();
	TWeakObjectPtr<UBDFadeWidget> WeakThis(this);
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](const float DeltaSeconds)
	{
		return WeakThis.IsValid() && WeakThis->Step(DeltaSeconds);
	}));
}

bool UBDFadeWidget::Step(const float DeltaSeconds)
{
	if (!bFading)
	{
		TickerHandle.Reset();
		return false;
	}

	// Real seconds: a fade over a frozen board or a 4x match runs the same.
	Elapsed += DeltaSeconds;
	const float Alpha = Duration > 0.0f ? FMath::Clamp(Elapsed / Duration, 0.0f, 1.0f) : 1.0f;
	Opacity = FMath::Lerp(StartOpacity, TargetOpacity, Alpha);
	SetRenderOpacity(Opacity);

	if (Alpha < 1.0f)
	{
		return true;
	}

	bFading = false;
	TickerHandle.Reset();
	TFunction<void()> Finished = MoveTemp(Done);
	Done = nullptr;
	if (Finished)
	{
		Finished();
	}
	return false;
}

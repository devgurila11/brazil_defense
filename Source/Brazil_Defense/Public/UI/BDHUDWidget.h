// Brazil Defense. The numbers of the match over the board, and what a selection offers.

#pragma once

#include "CoreMinimal.h"
#include "Match/BDMatchTypes.h"
#include "Placement/BDPlacementComponent.h"
#include "UI/BDWidgetBase.h"
#include "BDHUDWidget.generated.h"

class ABDCandidate;
class ABDMatchManager;
class ABDTowerBase;
class UBDCandidateSubsystem;
class UBDPlacementComponent;
class UBDWaveSubsystem;
class UBorder;
class UButton;
class UImage;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UVerticalBox;

/**
 * The in-game interface, raw. Always up: the two vote counters in their colors, the
 * wave and the countdown, the speed buttons, the open mouths. On demand: a panel for
 * the defender the player selected (stats, upgrade, sell) and one for the piece in
 * hand (cost, footprint, why the spot is refused). While the candidate walks, its health
 * across the top and a notice; while the count is frozen, the seconds left.
 *
 * Before any purchase the panel shows the scoreboard as it would be afterwards, in red
 * when the spend hands the lead to the other side: the one thing that keeps a player
 * from losing without understanding why.
 *
 * It only reads. Votes and phases come through the match delegates; what changes every
 * frame (the countdown, the candidate's health, the hover) is read on tick.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDHUDWidget : public UBDWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void BuildTree() override;
	virtual void RefreshTexts() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	ABDMatchManager* GetMatch() const;
	UBDWaveSubsystem* GetWaves() const;
	UBDCandidateSubsystem* GetCandidates() const;
	UBDPlacementComponent* GetPlacement() const;

	void BindMatch();
	void HandleVotesChanged(int32 Blue, int32 Red);
	void HandlePhaseChanged(EBDMatchPhase NewPhase);
	void HandleWaveStarted(int32 Wave);

	void UpdateScoreboard();
	void UpdateWaveLine();
	void UpdateSpeedButtons();
	void UpdateMouths();
	void UpdateDefenderPanel();
	void UpdatePlacementPanel();
	void UpdateCandidate(float RealDeltaSeconds);
	/** Time and phase of the day under the top block, while the sun moves and a moment after. */
	void UpdateClock();
	/** The centre box once the match is over: the result, and what the player can do next. */
	void UpdateEndPanel();

	/** "Blue B -> B' vs Red R" for a change of Delta votes; bOutInverts when a spend hands the lead over. */
	FText ResultText(int32 Delta, bool& bOutInverts) const;

	/** Puts ResultText on a line, in red when it inverts. */
	void SetResultLine(UTextBlock* Line, int32 Delta);

	/** The refusal of the placement gesture as words. */
	FText RefusalText(EBDPlacementRefusal Refusal) const;

	UFUNCTION()
	void HandleSpeed1();

	UFUNCTION()
	void HandleSpeed2();

	UFUNCTION()
	void HandleSpeed4();

	UFUNCTION()
	void HandleUpgrade();

	UFUNCTION()
	void HandleSell();

	UFUNCTION()
	void HandleCancelSelection();

	UFUNCTION()
	void HandleMenu();

	UFUNCTION()
	void HandleEndless();

	UFUNCTION()
	void HandleReturnToMenu();

	UFUNCTION()
	void HandleSave();

	UFUNCTION()
	void HandleLoad();

	/** The save button: its count, and whether a save may be taken now. */
	void UpdateSaveButton();

	/** The item bar: one button per piece of the palette, with what is left of it, and the urn. */
	void UpdateBuildPanel();

	/** Sizes that follow the viewport: icon boxes, side panel width, candidate bar. Runs when the viewport changes. */
	void ApplyResponsiveSizes();

	/** One item of the bar: a picture in a scale box over the words. */
	UButton* MakeItem(TObjectPtr<UImage>& OutIcon, TObjectPtr<USizeBox>& OutIconBox, TObjectPtr<UTextBlock>& OutLabel, TObjectPtr<UTextBlock>& OutCount);

	UFUNCTION()
	void HandleBuildUrn();

	UFUNCTION()
	void HandleBuild0();
	UFUNCTION()
	void HandleBuild1();
	UFUNCTION()
	void HandleBuild2();
	UFUNCTION()
	void HandleBuild3();
	UFUNCTION()
	void HandleBuild4();
	UFUNCTION()
	void HandleBuild5();
	UFUNCTION()
	void HandleBuild6();
	UFUNCTION()
	void HandleBuild7();
	UFUNCTION()
	void HandleBuild8();

	void SelectBuild(int32 Index);

	//~ Always visible
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BlueScore;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RedScore;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WaveLine;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeedLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SpeedButtons[3];

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeedButtonLabels[3];

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MouthsLine;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClockLine;

	//~ Candidate
	UPROPERTY(Transient)
	TObjectPtr<UBorder> CandidateBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CandidateLine;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> CandidateBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CandidateNotice;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FrozenLine;

	//~ Defender panel
	UPROPERTY(Transient)
	TObjectPtr<UBorder> DefenderBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DefenderName;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DefenderStats;

	UPROPERTY(Transient)
	TObjectPtr<UButton> UpgradeButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UpgradeLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UpgradeResult;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SellButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SellLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SellResult;

	//~ Placement panel
	UPROPERTY(Transient)
	TObjectPtr<UBorder> PlacementBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlacementName;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlacementInfo;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PlacementRefusal;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MoveResult;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CancelLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MenuLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SaveButton;

	//~ Item bar, along the bottom
	static constexpr int32 MaxPaletteButtons = 9;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BuildBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BuildTitle;

	UPROPERTY(Transient)
	TObjectPtr<UButton> UrnButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UrnLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UrnCount;

	UPROPERTY(Transient)
	TObjectPtr<UImage> UrnIcon;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> UrnIconBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BuildButtons[MaxPaletteButtons];

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BuildLabels[MaxPaletteButtons];

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BuildCounts[MaxPaletteButtons];

	UPROPERTY(Transient)
	TObjectPtr<UImage> BuildIcons[MaxPaletteButtons];

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> BuildIconBoxes[MaxPaletteButtons];

	/** The side panels' width box and the candidate bar's, resized with the viewport. */
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> SidePanelBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> CandidateBarBox;

	/** Viewport size the responsive sizes were last applied for. */
	FVector2D LastViewportSize = FVector2D::ZeroVector;

	/** The palette entries behind the buttons, loaded once. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBDPlaceableData>> PaletteData;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SaveLabel;

	//~ End of the match
	UPROPERTY(Transient)
	TObjectPtr<UBorder> EndBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EndTitle;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EndLine;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EndlessButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EndlessLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EndMenuLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LoadButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LoadLabel;

	TWeakObjectPtr<ABDMatchManager> BoundMatch;
	FDelegateHandle VotesChangedHandle;
	FDelegateHandle PhaseChangedHandle;
	FDelegateHandle WaveStartedHandle;

	/** The candidate seen last tick, to notice the one that just came out. */
	TWeakObjectPtr<ABDCandidate> LastCandidate;
	float NoticeRemaining = 0.0f;
};

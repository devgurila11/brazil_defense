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
class UCanvasPanel;
class UHorizontalBox;
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
	void HandleMoneyChanged(int32 Bribe, int32 PublicMoney);
	void HandlePhaseChanged(EBDMatchPhase NewPhase);
	void HandleWaveStarted(int32 Wave);

	void UpdateScoreboard();
	/** The thief's bribe and the mint's public money, under the count bar. */
	void UpdateMoneyCounters();
	void UpdateWaveLine();
	void UpdateSpeedButtons();
	void UpdateMouths();
	void UpdateDefenderPanel();
	void UpdatePlacementPanel();
	void UpdateCandidate(float RealDeltaSeconds);
	/** Time and phase of the day under the top block, while the sun moves and a moment after. */
	void UpdateClock();

	//~ Vote feedback ----------------------------------------------------------
	// A vote makes its ballot shiver (scale 1 -> 1.15 -> 1 with a +-5 degree wobble) and
	// its count tick; the urn pulses softly when its beep plays. A burst does not restart
	// the pulse: one runs to the end and at most one more is kept waiting, so a dense
	// wave reads as a couple of pulses, not a continuous tremor.

	/** One running pulse over a widget's render transform. */
	struct FBDPulse
	{
		float Elapsed = 0.0f;
		bool bRunning = false;
		bool bPending = false;

		void Trigger();
		/** Advances by real seconds. @return true while it moves something. */
		bool Advance(float DeltaSeconds, float Duration);
		/** 0 -> 1 -> 0 over the duration; 0 when idle. */
		float Bump(float Duration) const;
		/** -1 -> +1 -> -1 wobble over the duration; 0 when idle. */
		float Wobble(float Duration) const;
	};

	void HandleVoteSound();
	void UpdateVotePulses(float RealDeltaSeconds);
	void ApplyPulse(UWidget* Icon, UWidget* Count, const FBDPulse& Pulse);

	FBDPulse BluePulse;
	FBDPulse RedPulse;
	FBDPulse UrnPulse;
	/** The two money counters shiver as they climb, the same way the ballots do. */
	FBDPulse BribePulse;
	FBDPulse MintPulse;
	int32 LastBlueVotes = 0;
	int32 LastRedVotes = 0;
	int32 LastBribe = 0;
	int32 LastPublicMoney = 0;
	bool bVotesSeen = false;
	FDelegateHandle VoteSoundHandle;
	/** The centre box once the match is over: the result, and what the player can do next. */
	void UpdateEndPanel();

	/** "Public money M -> M'" for a change of Delta; bOutShort, and the shortfall instead, when a spend is more than there is. */
	FText ResultText(int32 Delta, bool& bOutShort) const;

	/** Puts ResultText on a line, in red when it falls short. */
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

	/** What is left of a kind: a hand counts down, a character reads the free slots, a tower is not counted. */
	FText BuildCountText(EBDPieceKind Kind) const;

	/**
	 * Why a piece of the bar cannot be taken right now, or None. The bar asks this instead
	 * of CanPlace alone, because a button that only greys out is the complaint this panel
	 * exists to answer: whatever stops the piece has to be readable under it.
	 */
	EBDPlacementRefusal BuildRefusal(const UBDPlaceableData* Data) const;

	/** The line under a build button: what is left and what it costs, or the reason it cannot be taken. */
	FText BuildInfoText(const UBDPlaceableData* Data, EBDPlacementRefusal Refusal) const;

	/** The three bands of the count bar, weighted by the votes. */
	void UpdateScoreBar();

	/** Sizes that follow the viewport: icon boxes, side panel width, candidate bar. Runs when the viewport changes. */
	void ApplyResponsiveSizes();

	/** A fixed picture of the HUD in a scale box: the box sets its share of the screen, the scale box keeps the aspect. */
	USizeBox* MakePicture(TObjectPtr<UImage>& OutImage, UTexture2D* Texture, float Size);

	/** One item of the bar: a picture in a scale box over the name and the info line. */
	UButton* MakeItem(TObjectPtr<UImage>& OutIcon, TObjectPtr<USizeBox>& OutIconBox, TObjectPtr<UTextBlock>& OutLabel, TObjectPtr<UTextBlock>& OutInfo);

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
	TObjectPtr<USizeBox> BlueIconBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> UrnScoreIconBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RedIconBox;

	UPROPERTY(Transient)
	TObjectPtr<UImage> BlueIcon;

	UPROPERTY(Transient)
	TObjectPtr<UImage> UrnScoreIcon;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RedIcon;

	//~ The count bar: blue, null, red, in proportion
	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ScoreBarBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BlueBand;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NullBand;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> RedBand;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NullScore;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> NullIconBox;

	UPROPERTY(Transient)
	TObjectPtr<UImage> NullIcon;

	//~ The money row, always up: the public money with the mint. Ahead of it, and only
	// while something is crossing, the thief with the bribe he has just been relieved of -
	// the animation of the gain, never a balance anybody can spend.
	/** The thief's part of the row: icon, amount and arrow, shown as one or not at all. */
	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> BribeGroup;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BribeScore;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> BribeIconBox;

	UPROPERTY(Transient)
	TObjectPtr<UImage> BribeIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MintScore;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> MintIconBox;

	UPROPERTY(Transient)
	TObjectPtr<UImage> MintIcon;

	/** The arrow between them, lit only while a conversion is crossing. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MoneyArrow;

	//~ Numbers rising from the urn when a creep gets through
	struct FBDFloater
	{
		TObjectPtr<UTextBlock> Text;
		float Age = 0.0f;
		int32 Value = 0;
	};
	TArray<FBDFloater> Floaters;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	void SpawnOrGrowFloater(int32 Votes);
	void UpdateFloaters(float RealDeltaSeconds);
	void SetFloaterLook(FBDFloater& Floater) const;

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

	//~ Candidates: one row per living one, pooled
	static constexpr int32 MaxCandidateRows = 6;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CandidateBox;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> CandidateRows;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CandidateLines[MaxCandidateRows];

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> CandidateBars[MaxCandidateRows];

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> CandidateBarBoxes[MaxCandidateRows];

	/** "+N more" under the rows when there are more candidates than rows. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CandidateOverflow;

	/** Kept only to keep the old responsive-size code pointing somewhere: the first row's box. */

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

	/** What the piece in hand costs and the count it would leave: for a build and for a move. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CostResult;

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
	FDelegateHandle MoneyChangedHandle;
	FDelegateHandle PhaseChangedHandle;
	FDelegateHandle WaveStartedHandle;

	/** The candidate seen last tick, to notice the one that just came out. */
	TWeakObjectPtr<ABDCandidate> LastCandidate;
	float NoticeRemaining = 0.0f;
};

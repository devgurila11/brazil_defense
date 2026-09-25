// Brazil Defense. The clock and the ledger of a match.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/BDGridTypes.h"
#include "Match/BDMatchTypes.h"
#include "Report/BDWaveLog.h"
#include "BDMatchManager.generated.h"

class ABDTowerBase;
class UBDDayCycleComponent;
class UBDDifficultyData;
class UBDGridSubsystem;
class UBDMatchSave;
class UBDPlaceableData;
class UBDPlacementComponent;

/** Broadcast whenever the match moves to another phase. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBDOnMatchPhaseChanged, EBDMatchPhase /*NewPhase*/);

/** Broadcast when a wave goes out. Carries the number of the wave that just started. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBDOnWaveStarted, int32 /*Wave*/);

/** Broadcast whenever either vote counter moves. Carries the new totals, blue then red. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBDOnVotesChanged, int32 /*Blue*/, int32 /*Red*/);

/** Broadcast whenever either money counter moves: the thief's recovered bribe, then the mint's public money. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FBDOnMoneyChanged, int32 /*Bribe*/, int32 /*PublicMoney*/);

/** What public money was spent on, so the post-match report can tell building from evolving. */
UENUM(BlueprintType)
enum class EBDFundsUse : uint8
{
	Build,
	Move,
	Evolve
};

/**
 * Where the public money of a match came from and went, kept as it happens for the
 * post-match report (BDPostMatch). Counts from the start of the match, or from the load
 * when the match was restored from a save.
 */
struct FBDMatchLedger
{
	/** Public money in the mint when the ledger opened: the starting funds, or what a save carried. */
	int32 StartingFunds = 0;
	/** Bribe dropped by the scheduled candidates killed: the only income of a match. */
	int32 BribeEarned = 0;
	/** Paid back by sales and by the levels of what was sold. */
	int32 Refunded = 0;
	/** Paid in from the console. */
	int32 Granted = 0;
	int32 SpentBuild = 0;
	int32 SpentMove = 0;
	int32 SpentEvolve = 0;
	int32 PiecesBuilt = 0;
	int32 LevelsBought = 0;
	/** Dividers added to the hand after the start: waves cleared and candidates killed. */
	int32 DividersGranted = 0;
	/** Last wave on which a piece was built or a level bought: where the defense stopped growing. */
	int32 LastGrowthWave = 0;
	/** The counters as the match ended, read before the money is dropped. */
	int32 PublicMoneyAtEnd = 0;
	int32 BribeHeldAtEnd = 0;
	/** The wave a save was loaded at, or -1 for a match played from its start. */
	int32 LoadedAtWave = -1;
	double RealStartSeconds = 0.0;
	double GameStartSeconds = 0.0;
	/** Why the match ended, in words, for the report. */
	FString EndReason;
	/** When the ledger opened, as a date: what ties the rows of one match together in the wave log. */
	FString StartedAt;
	/** The running totals as the last wave row left them; the next row is the difference. */
	FBDWaveLogMark WaveMark;
	/** A row has been written for the end this ledger is at; set again to false when endless goes on. */
	bool bReportWritten = false;
};

/**
 * Owns where a match stands: the phase, the wave, the countdown and what the player has
 * left to build with. Nothing else is allowed to decide those.
 *
 * Spawned by ABDGameMode when the level does not already carry one, and never spatially
 * loaded when it does: a match manager that streams out is a match that forgets itself.
 *
 * Also keeps the score, which in this game is votes: every creep killed adds its votes to
 * the blue counter, every creep that reaches the urn adds its votes to the red one.
 * UBDWaveSubsystem reports both, and reports the board empty through OnWaveCleared.
 */
UCLASS(meta = (DisplayName = "BD Match Manager"))
class BRAZIL_DEFENSE_API ABDMatchManager : public AActor
{
	GENERATED_BODY()

public:
	ABDMatchManager();

	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
#endif
	//~ End AActor interface

	/** The match manager of a world, or null when there is none. */
	static ABDMatchManager* Get(const UObject* WorldContextObject);

	//~ State -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	EBDMatchPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetCurrentWave() const { return CurrentWave; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	float GetTimeUntilNextWave() const { return TimeUntilNextWave; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetDividersRemaining() const { return DividersRemaining; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetPlatformsRemaining() const { return PlatformsRemaining; }

	/**
	 * Platform slots standing empty on the board right now: how many more characters
	 * could be mounted, if there are votes for them.
	 *
	 * Counted off the platforms every time it is asked rather than kept in a field. The
	 * board is the truth about the board: a platform authored in the level, one built
	 * this second and one sold all say the same thing to this without anybody having to
	 * remember to tell it.
	 */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetFreeCharacterSlots() const;

	/** Health multiplier the creeps of the current wave spawn with. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	float GetHealthScale() const;

	/** 1 until the urn is placed, then 0. Not a difficulty knob: there is one urn in the game. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetObjectivesRemaining() const { return ObjectivesRemaining; }

	/** Running total of the early call bonus, banked until there is an economy to spend it. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetEarlyCallBonus() const { return EarlyCallBonus; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	float GetGameSpeed() const { return GameSpeed; }

	/** Votes scored by the player: the sum of VotesOnDeath of every creep killed. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetVotesBlue() const { return VotesBlue; }

	/** Votes scored against the player: the sum of VotesOnArrival of every creep that reached the urn. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetVotesRed() const { return VotesRed; }

	/** Null votes: wasted damage at the vote rate. Shown on the count, never scored. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetVotesNull() const { return VotesNull; }

	/** Wasted damage, in votes. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void AddVotesNull(int32 Votes);

	/** True once the first wave has gone out and the urn is locked in where it stands. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool IsBuildLocked() const { return CurrentWave >= 1; }

	//~ Phase control ---------------------------------------------------------

	/** Opens the building phase and starts the countdown to the next wave. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void StartBuildingPhase();

	/** Debug: jumps the wave counter, so the scaling of any wave can be tested without playing up to it. */
	void DebugSetWave(int32 Wave);

	/** Debug: puts every budget back to what the difficulty hands out, as if nothing had been placed. */
	void DebugResetBudgets();

	/** Debug: throws the generated obstacles away and lays them again from a given seed. Placed pieces keep their cells. */
	void DebugRegenerateObstacles(int32 Seed);

	/**
	 * Debug: forces the match into a phase, skipping whatever would normally get it there.
	 * Building rewinds the wave count to zero as well, so placement opens up again rather
	 * than staying locked by a wave that never happened. Console tooling until the HUD and
	 * the spawner exist; not a game rule.
	 */
	void DebugForcePhase(EBDMatchPhase NewPhase);

	/** Skips the rest of the countdown, banking a bonus for the time given up. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void CallWaveEarly();

	/** Reported by UBDWaveSubsystem once the board is empty again. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void OnWaveCleared();

	/**
	 * Ends the match as a loss: the phase goes to Defeat and the board freezes where it
	 * stands. Nothing after it moves until the match is rewound from the console.
	 * @param Reason for the log, e.g. "the candidate reached the urn".
	 */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void DeclareDefeat(const FString& Reason);

	/** Whether the match is over, won or lost. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool IsMatchOver() const { return Phase == EBDMatchPhase::Defeat || Phase == EBDMatchPhase::Victory; }

	//~ Winning ------------------------------------------------------------------
	// Clearing the difficulty's last wave wins: the phase goes to Victory and the board
	// freezes like a defeat, so the player reads the result. Not while the candidate is
	// on the board, though: he is the one thing that can still lose the match, so the
	// waves keep coming until he dies (the win is taken then, at once between waves or
	// when the running wave clears) or reaches the urn. The win is kept once it is
	// taken; the player may then carry on into endless, where the waves keep scaling and
	// only a defeat ends things. A defeat after a win does not undo the win.

	/** Waves to clear for the win on this difficulty. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetWavesToWin() const;

	/** The difficulty asset this match runs on, after the fallback to the class defaults. */
	const UBDDifficultyData* GetDifficultyData() const { return DifficultyData; }

	/** Prisoners the win on this difficulty frees. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetPrisonersFreed() const;

	/** True once the winning wave has been cleared, whatever happened after. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool HasWon() const { return bWon; }

	/** True once the player chose to keep playing past the win. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool IsEndless() const { return bEndless; }

	/** The last wave cleared, nothing settled yet, and no candidate walking: the count decides now. */
	bool IsEndDue() const;

	/** Settles the match by the count: blue ahead or level wins, red ahead loses. */
	void ResolveEnd();

	/** How many times the count has been levelled down: the one way blue votes are ever taken away. */
	int32 GetLevelDownCount() const { return LevelDownCount; }

	/** Brings blue down to red (or red down to blue) so nobody is ahead: what killing the returning candidates buys. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void EqualizeVotesDown(const FString& Why);

	/** Ends the match as a win. Called when the win is due; public so the console can force it. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void DeclareVictory();

	/** From Victory, reopens the building phase for the next wave and keeps going until a defeat. @return false outside Victory. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	bool ContinueEndless();

	//~ Saving ------------------------------------------------------------------
	// The difficulty hands out a few saves. Each one writes the whole match to the one
	// slot, between waves only, and the count goes down: choosing when to spend them is
	// the game. Loading rebuilds the match in place, at the start of the wave after the
	// one saved, with the saves left as they were after that save.

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetSavesRemaining() const { return SavesRemaining; }

	/** Between waves, match still running, saves left. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool CanSaveMatch() const;

	/** Spends a save and writes the match. @return false when it cannot, with nothing spent. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	bool SaveMatch();

	/** Whether a saved match exists to load. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	static bool HasSavedMatch();

	/** Reads the slot and rebuilds this match from it. @return false when there is nothing to load. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	bool LoadMatch();

	/** Rebuilds this match from a snapshot, whatever it was doing. */
	void RestoreMatch(const UBDMatchSave& Save);

	//~ Chained difficulties -------------------------------------------------------
	// A win is recorded in UBDProgressSave. A match on a difficulty whose lower neighbour
	// has been won opens with that difficulty's ChainBonus on top of its budgets.

	/** The difficulty one step below this match's, or Count for the lowest. */
	static EBDDifficulty GetDifficultyBelow(EBDDifficulty Difficulty);

	/** Whether the chain bonus was handed out at the start of this match. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool WasChainBonusApplied() const { return bChainBonusApplied; }

	//~ Votes, reported by the wave subsystem -----------------------------------

	/** A creep was killed: its votes go to the player. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void AddVotesBlue(int32 Votes);

	/** A creep reached the urn: its votes go against the player. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void AddVotesRed(int32 Votes);

	// Votes are the score and nothing else: no piece, level or move is paid with them, so
	// nothing here takes them away but the levelling down after the return of the fallen.

	//~ The bribe and the public money -------------------------------------------
	// The currency of the match, kept apart from the votes and never mixed with them. The
	// difficulty hands out the starting funds; after that the only source is a scheduled
	// candidate killed, who drops the bribe he stole: the thief's counter takes it, then
	// the mint turns it into public money, which is what every piece, level and move is
	// paid with. Both counters live and die with the match - they are never carried to the
	// next one - and the conversion between them is driven by UBDBribeSubsystem, which is
	// what makes the numbers on the HUD climb rather than jump.

	/** Bribe recovered from the thief and not yet converted: the left-hand counter. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetBribeHeld() const { return BribeHeld; }

	/** Public money: the spendable balance, the right-hand counter. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetPublicMoney() const { return PublicMoney; }

	/** Puts recovered bribe on the thief's counter. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void AddBribe(int32 Amount, const FString& Why);

	/** Moves bribe off the thief's counter and onto the mint's, clamped to what is held. @return what actually moved. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	int32 ConvertBribe(int32 Amount);

	/** Pays public money straight into the mint from outside the game: the console. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void AddPublicMoney(int32 Amount, const FString& Why);

	/** Pays public money back for something taken off the board: a sale, or the levels of what was sold. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void PayRefund(int32 Amount, const FString& Why);

	/** Spends public money on something. @return false, nothing spent, when there is not enough. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	bool SpendPublicMoney(int32 Amount, EBDFundsUse Use);

	/** Hands back a charge the board then refused, as if it had never been made. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void ReturnPublicMoney(int32 Amount, EBDFundsUse Use, const FString& Why);

	/** Where the money of this match came from and went. */
	const FBDMatchLedger& GetLedger() const { return Ledger; }
	FBDMatchLedger& GetLedgerMutable() { return Ledger; }

	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool CanAffordPublicMoney(int32 Amount) const { return Amount <= PublicMoney; }

	/** Drops both counters to zero: what the end of a match does with money that does not carry over. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void DropMoney(const FString& Why);

	//~ Upgrades, for the HUD and the log -----------------------------------------

	/** Public money the next level of a defender costs. 0 for null or a defender at max level. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetUpgradeCost(const ABDTowerBase* Tower) const;

	//~ Prices ------------------------------------------------------------------
	// A piece costs about what a scheduled candidate of the current wave drops, so the
	// price climbs with the bosses: see UBDGameBalanceSettings, "Price".

	/** The wave prices are read on: the current one, and wave 1 before any has gone out. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetPriceWave() const { return FMath::Max(1, CurrentWave); }

	/** Public money a scheduled candidate on a wave drops. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetCandidateFunds(int32 Wave) const;

	/** Public money a piece costs to build right now. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetBuildPrice(const UBDPlaceableData* Piece) const;

	/** The same, on any wave: for the reports. */
	int32 GetBuildPriceOnWave(const UBDPlaceableData* Piece, int32 Wave) const;

	/** The wave after which a piece can be built. 0 is from the start. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	static int32 GetUnlockWave(const UBDPlaceableData* Piece);

	/** Whether a piece has come into the hand yet. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool IsUnlocked(const UBDPlaceableData* Piece) const { return CurrentWave >= GetUnlockWave(Piece); }

	//~ Moving pieces between waves --------------------------------------------

	/** Whether a placed piece may be picked up and put elsewhere right now: only while building. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool CanMove() const { return Phase == EBDMatchPhase::Building; }

	/** Fraction of a piece's price a move costs on the current wave. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	float GetMoveTaxRate() const;

	/** Public money a move of a piece bought for this much charges on the current wave. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetMoveCost(int32 PaidCost) const;

	/**
	 * Sets how fast the match runs. Only the speeds listed in the balance settings are
	 * accepted, and the choice persists across waves: a player who asked for 4x meant it.
	 *
	 * Implemented with global time dilation, so every system follows without knowing this
	 * exists. Nothing may multiply its own deltas to match.
	 */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	bool SetGameSpeed(float Speed);

	//~ Budget, asked by the placement gesture ---------------------------------
	// Only the maze is counted out. Dividers and platforms are a hand the player walks in
	// with - the dividers one that grows with the waves and the candidates, and costs no
	// money - and the urn is placed once; defenders are held back by the public money they
	// cost and by the board itself, never by a number. The maze can grow between waves as
	// well as before the first one: what a piece costs is the brake, not a lock.

	/** Whether a piece of this kind is the player's to place at all, counted or not. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	static bool IsPlaceableKind(EBDPieceKind Kind);

	/**
	 * Whether pieces of this kind come out of a counted hand. False for defenders, whose
	 * only limits are the public money and the board: asking GetBudgetRemaining about them says
	 * nothing, and treating the 0 it returns as "none left" is the bug this guards.
	 */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	static bool HasBudgetCeiling(EBDPieceKind Kind);

	/** Whether the current phase and hand allow placing a piece of this kind. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool CanPlace(EBDPieceKind Kind) const;

	/** Pieces of this kind still in hand, whatever the phase. 0 for a kind that is not counted out. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetBudgetRemaining(EBDPieceKind Kind) const;

	/** Charges one piece of this kind to the hand, when the kind is counted. @return false when it was not allowed. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	bool ConsumeBudget(EBDPieceKind Kind);

	/** Whether a piece of this kind may be taken back right now: everything but the urn, at any point. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	bool CanRemove(EBDPieceKind Kind) const;

	/** Puts a piece of this kind back in the hand. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void RefundRemoval(EBDPieceKind Kind);

	/**
	 * Adds dividers to the hand. The dividers are a budget of their own, apart from the
	 * public money: a cleared wave and a scheduled candidate killed both add to it.
	 */
	void GrantDividers(int32 Count, const FString& Why);

	/** What a scheduled candidate killed adds to the divider hand, on the difficulty. Called by the candidate subsystem. */
	void RewardCandidateKill(int32 Ordinal);

	/** Debug and measurement only: moves the divider ceiling, to ask what a different DividerBudget would buy. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	void AdjustDividerBudget(int32 Delta, const FString& Why);

	//~ Selling ------------------------------------------------------------------
	// Nothing on the board is permanent: any piece can be sold, and the price of having
	// been wrong is the part of what it was bought for that does not come back. All of it
	// comes back before the first wave, half once the waves run, and a divider keeps the
	// full refund through the grace window: see UBDGameBalanceSettings::GetSellRefundRatio.
	// Always a share of the price paid, never of today's price, and always in public money.

	/** Fraction of the price paid selling a piece of this kind pays back on the current wave. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	float GetSellRefundRatio(EBDPieceKind Kind) const;

	/** Public money selling a piece of this kind, bought for this much, pays back right now. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	int32 GetSellRefund(EBDPieceKind Kind, int32 PaidCost) const;

	/** Pays the sale of a piece into the mint. @return the public money paid. */
	UFUNCTION(BlueprintCallable, Category = "Brazil Defense|Match")
	int32 RefundSale(EBDPieceKind Kind, int32 PaidCost);

	/** The day cycle driven by this match. */
	UFUNCTION(BlueprintPure, Category = "Brazil Defense|Match")
	UBDDayCycleComponent* GetDayCycle() const { return DayCycle; }

	//~ Notifications ---------------------------------------------------------

	FBDOnMatchPhaseChanged OnPhaseChanged;
	FBDOnWaveStarted OnWaveStarted;
	FBDOnVotesChanged OnVotesChanged;
	FBDOnMoneyChanged OnMoneyChanged;

	/** Seed the board was generated from, so a match can be handed over as a number. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brazil Defense|Match")
	int32 ObstacleSeed = 0;

	/** Draw a fresh seed at BeginPlay instead of using ObstacleSeed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brazil Defense|Match")
	bool bRandomizeSeed = true;

	/** Difficulty this match runs on. Falls back to the one in the balance settings. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Brazil Defense|Match")
	EBDDifficulty Difficulty = EBDDifficulty::Normal;

private:
	/** Board setup: apply the difficulty, then generate the obstacles over the layout. */
	void SetupBoard();

	/** Moves to a phase and tells whoever is listening. */
	void SetPhase(EBDMatchPhase NewPhase);

	/** Sends the next wave out. */
	void StartWave();

	/** Counter behind a placeable state, or null when the kind is not counted out. */
	int32* FindBudget(EBDPieceKind Kind);
	const int32* FindBudget(EBDPieceKind Kind) const;

	UBDGridSubsystem* GetGrid() const;

	/** The local player's placement component, which owns the pieces on the board. */
	UBDPlacementComponent* GetPlacement() const;

	/** Resolves DifficultyData from Difficulty, falling back to the class defaults. */
	void ResolveDifficulty();

	/** The hand, the starting funds and the head start on the count, plus the chain bonus when the one below has been won. */
	void ApplyStartingBudgets();

	/** Opens a fresh ledger on the money in the mint right now. */
	void OpenLedger(int32 LoadedAtWave);

	/** Writes the abandoned row when the match is left unfinished. Once; the world tearing down and EndPlay both ask. */
	void ReportAbandoned(const FString& Why);

	void HandleWorldBeginTearDown(UWorld* World);
	FDelegateHandle TearDownHandle;

	FBDMatchLedger Ledger;

	UPROPERTY(VisibleAnywhere, Category = "Brazil Defense|Match")
	TObjectPtr<UBDDayCycleComponent> DayCycle;

	/** Resolved from Difficulty at BeginPlay. */
	UPROPERTY(Transient)
	TObjectPtr<const UBDDifficultyData> DifficultyData;

	EBDMatchPhase Phase = EBDMatchPhase::Setup;
	int32 CurrentWave = 0;
	float TimeUntilNextWave = 0.0f;
	int32 DividersRemaining = 0;
	int32 PlatformsRemaining = 0;
	int32 ObjectivesRemaining = 0;
	int32 EarlyCallBonus = 0;
	float GameSpeed = 1.0f;
	int32 VotesBlue = 0;
	int32 VotesRed = 0;
	int32 VotesNull = 0;
	int32 BribeHeld = 0;
	int32 PublicMoney = 0;
	bool bWon = false;
	bool bEndless = false;
	int32 SavesRemaining = 0;
	bool bChainBonusApplied = false;
	int32 LevelDownCount = 0;
};

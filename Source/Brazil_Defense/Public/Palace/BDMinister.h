// Brazil Defense. A minister out of the STF: he strolls the board, the Agent kicks him, he runs back.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Grid/BDGridTypes.h"
#include "BDMinister.generated.h"

class ABDEnemyBase;
class UBDGridSubsystem;
class UStaticMeshComponent;
struct FBDFreeWalk;

/** What a minister is doing. */
UENUM(BlueprintType)
enum class EBDMinisterState : uint8
{
	/** Out of the STF door and onto the board. */
	Entering,

	/** Strolling the board, free of the grid like the Agent: only dividers and blocked cells stop him. */
	Walking,
	Idle,

	/**
	 * Stopped with his book open: militants inside his radius are covered (the refuge). He
	 * reads on while they are there and for a while after the last one, then walks on.
	 */
	Reading,

	/** Kicked: up in the air, turning. */
	Flying,

	/** On the floor where he fell. */
	Down,

	/** Running back to the STF, faster, and gone through its door. */
	Fleeing,
};

/**
 * A minister (slice 3a). A black block until his model comes, already twice a man's size.
 * He comes out of the STF (UBDMinisterSubsystem finds it and its door on the board) and
 * walks the board in continuous space by the free-walk rules (FBDFreeWalk), through
 * towers, platforms and buildings, stopped only by dividers and blocked cells.
 *
 * The Agent runs at him and kicks him (ABDAgent): he flies up turning, cries out - the
 * black exclamation over him - falls, lies there a moment, then runs back to the STF and
 * is gone through its door: the kick is the only thing that sends him back.
 *
 * The refuge (slice 3b): a militant (never the candidate) within his radius (wider the
 * later his seat in the STF's order, UBDMinisterSettings::GetRefugeRadiusCells)
 * stops him, and he reads. While he reads, every militant inside the radius is covered
 * (ABDEnemyBase::IsSheltered): no defender aims at it, no hit lands, it blinks; the shield
 * dome stands on the edge of the radius while somebody is inside. With the radius empty he
 * reads on ResumeAfterSeconds and walks again. The Agent's kick breaks it: the cover drops
 * at once, the book and the dome go, and he flies.
 */
UCLASS(NotBlueprintable, meta = (DisplayName = "BD Minister"))
class BRAZIL_DEFENSE_API ABDMinister : public AActor
{
	GENERATED_BODY()

public:
	ABDMinister();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Sets him off: out of Outside (the STF door, off the board), onto DoorCell, then the
	 * stroll. Running back, he heads for the same door.
	 */
	void InitializeMinister(const FVector& Outside, const FBDCellCoord& DoorCell, const AActor* Stf, int32 InSeat);

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** His seat in the STF's order, 1 the first out; 0 for none. */
	int32 GetSeat() const { return Seat; }

	/** How far his cover reaches, in cells: wider the later his seat. */
	float GetRefugeRadiusCells() const;

	/** Puts him walking at a point of the board at once, skipping the door (for the tests). */
	void DebugPlaceOnBoard(const FVector& Where);

	/** Whether a kick would land: he is on his feet on the board, not flying, lying or running. */
	bool CanBeKicked() const;

	/** Militants he covers this frame. */
	int32 GetShelteredCount() const { return Sheltered.Num(); }

	/** Whether the shield dome is up: he reads with somebody inside his radius. */
	bool IsShieldUp() const;

	/** The Agent's foot landed: up he goes, crying out, away from the kicker. */
	void ReceiveKick(const AActor& Kicker);

	EBDMinisterState GetState() const { return State; }

	/** Half the width of his block, for how close the Agent must come. */
	float GetBodyRadius() const;

	/** The highest his feet went over the floor in his last flight, in centimetres. */
	float GetLastFlightPeak() const { return LastFlightPeak; }

	/** Kicks he has taken. */
	int32 GetKicksTaken() const { return KicksTaken; }

	/** One line on his state, for the console and the regression. */
	FString Describe() const;

private:
	FBDFreeWalk Walk() const;
	UBDGridSubsystem* GetGrid() const;

	/** Steps along Route, turning to it. @return true once it is walked. */
	bool StepAlong(float DeltaSeconds, float SpeedScale);

	void DecideNext();
	void StartWalk();

	/** Who is inside his radius, covered while he reads; starts and ends the reading. */
	void UpdateRefuge(float DeltaSeconds);
	void StartReading();
	void StopReading();

	/** Takes the cover off everyone at once and puts the book and the dome away. */
	void DropCover();
	void StartFlee();
	void Cry();

	/** Lays the block down (on its side) or stands it back up. */
	void SetLying(bool bLying);

	UPROPERTY(VisibleAnywhere, Category = "Minister")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Minister")
	TObjectPtr<UStaticMeshComponent> Block;

	/** The book in his hands while he reads. */
	UPROPERTY(VisibleAnywhere, Category = "Minister")
	TObjectPtr<UStaticMeshComponent> Book;

	/** The dome on the edge of his radius. */
	UPROPERTY(VisibleAnywhere, Category = "Minister")
	TObjectPtr<UStaticMeshComponent> Shield;

	/** The militants covered this frame. */
	TArray<TWeakObjectPtr<ABDEnemyBase>> Sheltered;

	/** Seconds read with nobody in the radius. */
	float QuietSeconds = 0.0f;

	int32 Seat = 0;

	EBDMinisterState State = EBDMinisterState::Entering;

	/** Points still to walk, the next one first. */
	TArray<FVector> Route;

	TWeakObjectPtr<const AActor> StfBuilding;
	FVector DoorOutside = FVector::ZeroVector;
	FBDCellCoord DoorCell;

	float IdleRemaining = 0.0f;
	float StateSeconds = 0.0f;

	FVector FlightFrom = FVector::ZeroVector;
	FVector FlightTo = FVector::ZeroVector;
	float LastFlightPeak = 0.0f;
	int32 KicksTaken = 0;

	/** The block's height and its rest over the root, from the mesh bounds and the scale. */
	float BlockHalfHeight = 50.0f;
	float BlockHalfWidth = 50.0f;
};

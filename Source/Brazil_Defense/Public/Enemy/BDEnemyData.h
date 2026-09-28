// Brazil Defense. Definition of one kind of enemy.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BDEnemyData.generated.h"

class ABDEnemyBase;
class UAnimSequenceBase;
class UMaterialInterface;
class USkeletalMesh;
class USoundBase;
class UStaticMesh;
class UTexture2D;

/**
 * One kind of creep: how tough it is, how fast it walks and what it is worth.
 *
 * Speeds are in cells, not centimetres. The board is the unit the designer reasons
 * in ("crosses the Esplanada in twenty seconds"), and the cell size is a project
 * setting that must be free to change without every enemy asset going stale.
 */
UCLASS(BlueprintType, meta = (DisplayName = "BD Enemy"))
class BRAZIL_DEFENSE_API UBDEnemyData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Asset type used to discover every enemy through the asset manager. */
	static const FPrimaryAssetType EnemyAssetType;

	//~ Begin UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UPrimaryDataAsset interface

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float MaxHealth = 100.0f;

	/** Walking speed, in cells per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveSpeed = 1.0f;

	/** How fast the creep reaches MoveSpeed after spawning, in cells per second squared. 0 starts it at full speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float Acceleration = 2.0f;

	/** Added to the blue counter when this creep is killed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Votes", meta = (ClampMin = "0", UIMin = "0"))
	int32 VotesOnDeath = 1;

	/** Added to the red counter when this creep reaches the urn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Votes", meta = (ClampMin = "0", UIMin = "0"))
	int32 VotesOnArrival = 1;

	/** Pawn spawned for this enemy. Loaded on demand. Falls back to ABDEnemyBase itself when unset. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TSoftClassPtr<ABDEnemyBase> EnemyClass;

	/**
	 * Mesh handed to the pawn at spawn, so a plain ABDEnemyBase can stand in for a creep
	 * without a Blueprint per enemy. Optional: a Blueprint class with its own mesh leaves
	 * this empty.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UStaticMesh> Mesh;

	/**
	 * Animated body handed to the pawn at spawn. When set it is used instead of Mesh.
	 * Variants of one creep point at the same skeletal mesh and differ by MeshMaterial.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/**
	 * Loop played on SkeletalMesh while the creep walks. Expected in place: the route moves
	 * the creep, and any root motion in it is ignored. Its rate follows the creep's speed,
	 * against the reference speed in the wave settings.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", meta = (EditCondition = "SkeletalMesh != nullptr"))
	TSoftObjectPtr<UAnimSequenceBase> MoveAnimation;

	/**
	 * Falls played on SkeletalMesh when the creep is killed, one drawn at random. Show only:
	 * the creep is out of the game the instant it dies, and a body is left behind to play
	 * the fall and sink away (ABDCreepCorpse). Empty: the creep just vanishes.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", meta = (EditCondition = "SkeletalMesh != nullptr"))
	TArray<TSoftObjectPtr<UAnimSequenceBase>> DeathAnimations;

	/** Yaw added to the body so its front faces along the route. A Mixamo import faces +Y and wants -90. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", meta = (ForceUnits = "deg"))
	float MeshYaw = 0.0f;

	/** Scale applied to Mesh or SkeletalMesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", meta = (EditCondition = "Mesh != nullptr || SkeletalMesh != nullptr"))
	FVector MeshScale = FVector::OneVector;

	/**
	 * Material put on every slot of the body, Mesh or SkeletalMesh: a placeholder shape
	 * told apart by color, or the variant of an animated creep. Optional.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual", meta = (EditCondition = "Mesh != nullptr || SkeletalMesh != nullptr"))
	TSoftObjectPtr<UMaterialInterface> MeshMaterial;

	/**
	 * What the kill board counts this creep as: its kind of gameplay, never its skin. The
	 * skins of one enemy all name the same kind and add up on one icon; an enemy with other
	 * stats names its own. Empty counts the asset as a kind of its own.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kill Board")
	FName KillType;

	/** Face of that kind on the kill board. The first creep of a kind killed brings its icon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kill Board", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> KillIcon;

	//~ Sound ------------------------------------------------------------------
	// A creep is vocal when it has SpeechSound, CallSound or both: now and then, on a timer
	// of its own, it says something or makes its noise. How many creeps may sound at once
	// over the whole board is not here but in UBDCreepSoundSettings. Another vocal NPC only
	// needs these filled: the cues, a pitch, an interval.

	/** Words. A cue meant to travel between characters: VoicePitch makes it someone else. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	TSoftObjectPtr<USoundBase> SpeechSound;

	/** The creature's own noise, kept apart from the words so the words can be reused on a creature that makes another. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	TSoftObjectPtr<USoundBase> CallSound;

	/**
	 * Share of the vocalizations that are words rather than the call. The words are the
	 * charm; the budget in UBDCreepSoundSettings is what keeps the horde from chattering.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float SpeechShare = 0.6f;

	/** Pitch the words are played at: below 1 deepens the voice, above 1 thins it. The call is left alone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.25", ClampMax = "4.0", UIMin = "0.5", UIMax = "2.0"))
	float VoicePitch = 1.0f;

	/** Seconds between two vocalizations of one creep, drawn anew each time inside this range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.5", UIMin = "0.5", ForceUnits = "s"))
	float VocalIntervalMin = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.5", UIMin = "0.5", ForceUnits = "s"))
	float VocalIntervalMax = 20.0f;

	/**
	 * A step, played by the BD Footstep notifies placed on MoveAnimation at the frames a
	 * foot lands, so it keeps time with the legs at any speed. No notify, no step.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	TSoftObjectPtr<USoundBase> FootstepSound;

	/** Cry at the moment of the kill, at the spot, under the death budget of UBDCreepSoundSettings. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	TSoftObjectPtr<USoundBase> DeathSound;

	/**
	 * The thud of the body hitting the ground, played by the BD Body Fall notifies placed on
	 * the DeathAnimations at the frame of the impact. Third layer of a kill, after the cry
	 * and the fall.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	TSoftObjectPtr<USoundBase> BodyFallSound;

	/** Whether this creep ever speaks or calls. */
	bool IsVocal() const { return !SpeechSound.IsNull() || !CallSound.IsNull(); }

	/** KillType, or the asset's own name when it names none. */
	FName GetKillType() const { return KillType.IsNone() ? GetFName() : KillType; }
};

// Brazil Defense. One minister of the STF: his body, his animations and his voice.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BDMinisterData.generated.h"

class UAnimSequenceBase;
class UMaterialInterface;
class USkeletalMesh;
class USkeleton;
class USoundBase;
class UTexture2D;

/**
 * What one of the eleven ministers looks and sounds like (DA_Ministro_NN, by his seat in
 * the STF's order). How every minister walks, reads and takes the kick stays in
 * UBDMinisterSettings: this is only who he is.
 *
 * Every slot can stay empty. ABDMinister is handed his data at spawn and, for now, reads
 * only FleeSound from it; the black block of the settings still stands in for the body
 * until the model and the animations arrive. A minister never dies: kicked, he flees back
 * to the STF, so he has a FleeSound and no death cry.
 */
UCLASS(BlueprintType, meta = (DisplayName = "BD Minister"))
class BRAZIL_DEFENSE_API UBDMinisterData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Asset type used to discover every minister through the asset manager. */
	static const FPrimaryAssetType MinisterAssetType;

	//~ Begin UPrimaryDataAsset interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UPrimaryDataAsset interface

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minister")
	FText DisplayName;

	/** His face, for the panel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minister", meta = (AllowedClasses = "/Script/Engine.Texture2D"))
	TSoftObjectPtr<UTexture2D> Icon;

	//~ Look ---------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	/** The skeleton his body and his animations share. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look")
	TSoftObjectPtr<USkeleton> Skeleton;

	/** Material put on every slot of the body. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look")
	TSoftObjectPtr<UMaterialInterface> MeshMaterial;

	/** More looks of the same man, drawn with MeshMaterial as one pool. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look")
	TArray<TSoftObjectPtr<UMaterialInterface>> SkinMaterials;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look")
	FVector MeshScale = FVector::OneVector;

	/** Yaw added to the body so its front faces where he walks. A Mixamo import faces +Y and wants -90. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Look", meta = (ForceUnits = "deg"))
	float MeshYaw = 0.0f;

	//~ Animation ----------------------------------------------------------------

	/** His stroll, in place. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequenceBase> WalkAnimation;

	/** Standing a while between two stretches. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequenceBase> IdleAnimation;

	/** Stopped with the book open, covering the militants around him. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequenceBase> ReadAnimation;

	/** The Agent's kick: up in the air and down on the floor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequenceBase> KickedAnimation;

	/** Running back to the STF. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimSequenceBase> FleeAnimation;

	//~ Sound --------------------------------------------------------------------

	/** What he says while he walks. Not played yet. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	TSoftObjectPtr<USoundBase> SpeechSound;

	/**
	 * His cry when the kick lands, as he takes off for the STF: a bank apart from the words.
	 * Empty: the black exclamation still goes up over him, for the settings' ScreamMarkSeconds.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound", meta = (AllowedClasses = "/Script/Engine.SoundBase"))
	TSoftObjectPtr<USoundBase> FleeSound;
};

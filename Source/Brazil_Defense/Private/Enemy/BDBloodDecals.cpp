// Brazil Defense. The green stain a creep leaves on the ground where it falls.

#include "Enemy/BDBloodDecals.h"

#include "BDLog.h"
#include "Components/DecalComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace BDBloodPrivate
{
	static const FName TextureParameter = TEXT("BloodTexture");
	static const FName TintParameter = TEXT("Tint");
}

UBDBloodSettings::UBDBloodSettings()
{
	CategoryName = TEXT("Game");
	DecalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/BD/Materials/M_BloodDecal.M_BloodDecal")));
}

const UBDBloodSettings& UBDBloodSettings::Get()
{
	return *GetDefault<UBDBloodSettings>();
}

UBDBloodDecalSubsystem* UBDBloodDecalSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
	return World != nullptr ? World->GetSubsystem<UBDBloodDecalSubsystem>() : nullptr;
}

void UBDBloodDecalSubsystem::Prune()
{
	Live.RemoveAll([](const TWeakObjectPtr<UDecalComponent>& Decal) { return !Decal.IsValid(); });
	for (auto It = Evicting.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

int32 UBDBloodDecalSubsystem::GetLiveCount()
{
	Prune();
	return Live.Num();
}

UDecalComponent* UBDBloodDecalSubsystem::SpawnAt(const FVector& Where)
{
	++Requested;
	const UBDBloodSettings& Settings = UBDBloodSettings::Get();
	UWorld* World = GetWorld();
	UTexture2D* Texture = TextureOverride != nullptr ? TextureOverride.Get() : Settings.BloodTexture.LoadSynchronous();
	UMaterialInterface* Material = Settings.DecalMaterial.LoadSynchronous();
	if (World == nullptr || Texture == nullptr || Material == nullptr || Settings.MaxDecals <= 0)
	{
		// The slot is empty until the art lands: the kill leaves no stain.
		return nullptr;
	}

	// Room for one more: the oldest still whole fades out fast. The ones already fading on
	// their own are left to it, so a dense wave keeps MaxDecals and no more.
	Prune();
	int32 Whole = Live.Num() - Evicting.Num();
	for (int32 Index = 0; Index < Live.Num() && Whole >= Settings.MaxDecals; ++Index)
	{
		UDecalComponent* Oldest = Live[Index].Get();
		if (Oldest != nullptr && !Evicting.Contains(Live[Index]))
		{
			// Never the owner: the decal hangs off the world settings, which must stay.
			Oldest->SetFadeOut(0.0f, Settings.OverflowFadeTime, /*DestroyOwnerAfterFade*/ false);
			Evicting.Add(Live[Index]);
			++Evicted;
			--Whole;
		}
	}

	// One painted material for every stain: the texture and the tint are the same, the
	// fade is each decal's own lifetime.
	if (Painted == nullptr || Painted->Parent != Material)
	{
		Painted = UMaterialInstanceDynamic::Create(Material, this);
	}
	Painted->SetTextureParameterValue(BDBloodPrivate::TextureParameter, Texture);
	Painted->SetVectorParameterValue(BDBloodPrivate::TintParameter, Settings.Tint);

	// Projected straight down, any turn, a little bigger or smaller each time.
	const float Scale = 1.0f + FMath::FRandRange(-Settings.SizeJitter, Settings.SizeJitter);
	const float Half = Settings.Size * 0.5f * Scale;
	const FRotator Down(-90.0f, FMath::FRandRange(0.0f, 360.0f), 0.0f);
	UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(World, Painted, FVector(Settings.Depth, Half, Half), Where, Down);
	if (Decal == nullptr)
	{
		return nullptr;
	}
	Decal->SetFadeOut(Settings.HoldTime, Settings.DecalFadeTime, /*DestroyOwnerAfterFade*/ false);
	Decal->SetFadeScreenSize(0.0f);
	Live.Add(Decal);
	++Spawned;
	return Decal;
}

// Brazil Defense. Tooling: lay out the four sound classes the options panel drives.

#include "Debug/BDSetupSoundClassesCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Audio/BDAudioSettings.h"
#include "BDLog.h"
#include "Enemy/BDCreepSoundSettings.h"
#include "Misc/PackageName.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "UI/BDUISettings.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace BDSetupSoundClassesPrivate
{
	/** Where the environment class is made when the interface settings name none. */
	static const TCHAR* const EnvironmentPath = TEXT("/Game/BD/Audio/SC_Environment");

	static bool SaveAsset(UObject* Asset)
	{
#if WITH_EDITOR
		UPackage* Package = Asset->GetPackage();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
		{
			UE_LOG(LogBDDebug, Error, TEXT("BDSetupSoundClasses: %s could not be saved to %s."), *Package->GetName(), *Filename);
			return false;
		}
		UE_LOG(LogBDDebug, Display, TEXT("BDSetupSoundClasses: saved %s."), *Filename);
		return true;
#else
		return false;
#endif
	}

	/** Hangs Child under Parent, out of any other parent's list. True when anything moved. */
	static bool Reparent(USoundClass* Child, USoundClass* Parent, TSet<UObject*>& Dirty)
	{
		bool bChanged = false;
		USoundClass* Old = Child->ParentClass;
		if (Old != Parent)
		{
			if (Old != nullptr && Old->ChildClasses.Remove(Child) > 0)
			{
				Dirty.Add(Old);
			}
			Child->ParentClass = Parent;
			Dirty.Add(Child);
			bChanged = true;
			UE_LOG(LogBDDebug, Display, TEXT("BDSetupSoundClasses: %s moved from under %s to under %s."), *Child->GetName(), *GetNameSafe(Old), *Parent->GetName());
		}
		if (!Parent->ChildClasses.Contains(Child))
		{
			Parent->ChildClasses.Add(Child);
			Dirty.Add(Parent);
			bChanged = true;
		}
		return bChanged;
	}
}

UBDSetupSoundClassesCommandlet::UBDSetupSoundClassesCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UBDSetupSoundClassesCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace BDSetupSoundClassesPrivate;

	const bool bDryRun = FParse::Param(*Params, TEXT("DryRun"));
	const UBDUISettings& UI = UBDUISettings::Get();
	const UBDCreepSoundSettings& Voice = UBDCreepSoundSettings::Get();

	USoundClass* Master = UI.MasterSoundClass.LoadSynchronous();
	USoundClass* Music = UI.MusicSoundClass.LoadSynchronous();
	USoundClass* Effects = UI.EffectsSoundClass.LoadSynchronous();
	USoundClass* VoiceClass = UI.VoiceSoundClass.LoadSynchronous();
	USoundClass* Environment = UI.EnvironmentSoundClass.LoadSynchronous();
	USoundMix* Duck = Voice.SpeechDuckMix.LoadSynchronous();
	if (Master == nullptr || Music == nullptr || Effects == nullptr || VoiceClass == nullptr || Duck == nullptr)
	{
		UE_LOG(LogBDDebug, Error, TEXT("BDSetupSoundClasses: the interface settings must name the master, music, effects and voice classes, and the creep sound settings the duck mix (master %s, music %s, effects %s, voice %s, duck %s)."),
			*GetNameSafe(Master), *GetNameSafe(Music), *GetNameSafe(Effects), *GetNameSafe(VoiceClass), *GetNameSafe(Duck));
		return 1;
	}

	TSet<UObject*> Dirty;
	if (Environment == nullptr && !bDryRun)
	{
		const FString PackageName = UI.EnvironmentSoundClass.IsNull() ? FString(EnvironmentPath) : UI.EnvironmentSoundClass.ToSoftObjectPath().GetLongPackageName();
		UPackage* Package = CreatePackage(*PackageName);
		Environment = NewObject<USoundClass>(Package, *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Environment);
		Dirty.Add(Environment);
		UE_LOG(LogBDDebug, Display, TEXT("BDSetupSoundClasses: created %s."), *Environment->GetPathName());
	}

	if (!bDryRun)
	{
		for (USoundClass* Class : { Music, Effects, VoiceClass, Environment })
		{
			Reparent(Class, Master, Dirty);
		}

		// The words step the effects and the environment back, each on its own, and only them.
		TArray<FSoundClassAdjuster> Wanted;
		for (const TPair<USoundClass*, float>& Each : { TPair<USoundClass*, float>(Effects, Voice.DuckEffectsVolume), TPair<USoundClass*, float>(Environment, Voice.DuckEnvironmentVolume) })
		{
			FSoundClassAdjuster& Adjuster = Wanted.AddDefaulted_GetRef();
			Adjuster.SoundClassObject = Each.Key;
			Adjuster.VolumeAdjuster = Each.Value;
			Adjuster.bApplyToChildren = false;
		}
		bool bSame = Duck->SoundClassEffects.Num() == Wanted.Num();
		for (int32 Index = 0; bSame && Index < Wanted.Num(); ++Index)
		{
			bSame = Duck->SoundClassEffects[Index].SoundClassObject == Wanted[Index].SoundClassObject
				&& FMath::IsNearlyEqual(Duck->SoundClassEffects[Index].VolumeAdjuster, Wanted[Index].VolumeAdjuster)
				&& !Duck->SoundClassEffects[Index].bApplyToChildren;
		}
		if (!bSame)
		{
			Duck->SoundClassEffects = Wanted;
			Duck->FadeInTime = Voice.DuckFadeSeconds;
			Duck->FadeOutTime = Voice.DuckFadeSeconds;
			Dirty.Add(Duck);
			UE_LOG(LogBDDebug, Display, TEXT("BDSetupSoundClasses: %s now ducks %s to %.2f and %s to %.2f."), *Duck->GetName(),
				*Effects->GetName(), Voice.DuckEffectsVolume, *Environment->GetName(), Voice.DuckEnvironmentVolume);
		}

		// The ambience slots on the environment class, the waves themselves and not only the
		// soundscape's override, so a clip heard anywhere else follows the same slider.
		const UBDAudioSettings& Audio = UBDAudioSettings::Get();
		for (const TArray<TSoftObjectPtr<USoundBase>>* Slot : { &Audio.AmbienceCityDay, &Audio.AmbienceBirdsDay, &Audio.AmbienceNight })
		{
			for (const TSoftObjectPtr<USoundBase>& Clip : *Slot)
			{
				USoundBase* Sound = Clip.LoadSynchronous();
				if (Sound != nullptr && Sound->SoundClassObject != Environment)
				{
					Sound->SoundClassObject = Environment;
					Dirty.Add(Sound);
				}
			}
		}

		// The voice class pushes the duck by itself whenever a line sounds.
		if (!VoiceClass->PassiveSoundMixModifiers.ContainsByPredicate([Duck](const FPassiveSoundMixModifier& Modifier) { return Modifier.SoundMix == Duck; }))
		{
			FPassiveSoundMixModifier& Modifier = VoiceClass->PassiveSoundMixModifiers.AddDefaulted_GetRef();
			Modifier.SoundMix = Duck;
			Dirty.Add(VoiceClass);
			UE_LOG(LogBDDebug, Display, TEXT("BDSetupSoundClasses: %s pushes %s while it sounds."), *VoiceClass->GetName(), *Duck->GetName());
		}

		int32 Failed = 0;
		for (UObject* Asset : Dirty)
		{
			Asset->MarkPackageDirty();
			Failed += SaveAsset(Asset) ? 0 : 1;
		}
		if (Failed > 0)
		{
			return 1;
		}
		UE_LOG(LogBDDebug, Display, TEXT("BDSetupSoundClasses: %d asset(s) changed."), Dirty.Num());
	}

	// The hierarchy as it stands, and every sound with its class.
	for (const USoundClass* Class : { Master, Music, Effects, VoiceClass, Environment })
	{
		if (Class == nullptr)
		{
			continue;
		}
		FString Children;
		for (const USoundClass* Child : Class->ChildClasses)
		{
			Children += (Children.IsEmpty() ? TEXT("") : TEXT(", ")) + GetNameSafe(Child);
		}
		UE_LOG(LogBDDebug, Display, TEXT("BDSetupSoundClasses: class %s, under %s, children [%s], %d passive mix(es)."),
			*Class->GetName(), *GetNameSafe(Class->ParentClass), *Children, Class->PassiveSoundMixModifiers.Num());
	}

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.SearchAllAssets(/*bSynchronousSearch*/ true);
	TArray<FAssetData> Sounds;
	Registry.GetAssetsByClass(USoundBase::StaticClass()->GetClassPathName(), Sounds, /*bSearchSubClasses*/ true);
	int32 Listed = 0;
	for (const FAssetData& Data : Sounds)
	{
		if (!Data.PackageName.ToString().StartsWith(TEXT("/Game/")))
		{
			continue;
		}
		const USoundBase* Sound = Cast<USoundBase>(Data.GetAsset());
		if (Sound == nullptr)
		{
			continue;
		}
		const USoundClass* Class = Sound->GetSoundClass();
		// The bare waves under a cue are many and play only through it: only the routed ones are listed.
		if (Class == nullptr && Sound->IsA(USoundWave::StaticClass()))
		{
			continue;
		}
		++Listed;
		UE_LOG(LogBDDebug, Display, TEXT("BDSetupSoundClasses: sound %s -> %s"), *Data.PackageName.ToString(), *GetNameSafe(Class));
	}
	UE_LOG(LogBDDebug, Display, TEXT("BDSetupSoundClasses: %d routed sound(s) listed."), Listed);
	return 0;
#else
	UE_LOG(LogBDDebug, Error, TEXT("BDSetupSoundClasses runs only in an editor build."));
	return 1;
#endif
}

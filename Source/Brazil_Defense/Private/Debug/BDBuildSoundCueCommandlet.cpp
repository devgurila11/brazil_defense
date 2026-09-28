// Brazil Defense. Tooling: build a random, pitch varied Sound Cue out of a folder of waves.

#include "Debug/BDBuildSoundCueCommandlet.h"

#include "BDLog.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "EdGraph/EdGraphNode.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeModulator.h"
#include "Sound/SoundNodeRandom.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"
#include "UI/BDUISettings.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

UBDBuildSoundCueCommandlet::UBDBuildSoundCueCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UBDBuildSoundCueCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString CuePath;
	FString Folder;
	FString Prefix;
	if (!FParse::Value(*Params, TEXT("Cue="), CuePath) || !FParse::Value(*Params, TEXT("Folder="), Folder))
	{
		UE_LOG(LogBDDebug, Error, TEXT("Usage: -run=BDBuildSoundCue -Cue=/Game/Path/SCue_X -Folder=/Game/Path [-Prefix=Name_] [-Exclude=Text] [-Pitch=0.05] [-Volume=0.0] [-CuePitch=1.0]"));
		return 1;
	}
	FParse::Value(*Params, TEXT("Prefix="), Prefix);
	FString Exclude;
	FParse::Value(*Params, TEXT("Exclude="), Exclude);
	float PitchSpread = 0.05f;
	float VolumeSpread = 0.0f;
	float CuePitch = 1.0f;
	FParse::Value(*Params, TEXT("Pitch="), PitchSpread);
	FParse::Value(*Params, TEXT("Volume="), VolumeSpread);
	FParse::Value(*Params, TEXT("CuePitch="), CuePitch);
	Folder.RemoveFromEnd(TEXT("/"));

	// The cue graph is built through the audio editor; without it the cue has no graph
	// and the editor would show an empty one over nodes that play.
	FModuleManager::Get().LoadModule(TEXT("AudioEditor"));
	if (!USoundCue::GetSoundCueAudioEditor().IsValid())
	{
		UE_LOG(LogBDDebug, Error, TEXT("BDBuildSoundCue: the audio editor module is not available."));
		return 1;
	}

	// The waves of the folder, in name order, so the cue lists them the way the folder does.
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({ Folder }, /*bForceRescan*/ true);
	TArray<FAssetData> Found;
	Registry.GetAssetsByPath(FName(*Folder), Found, /*bRecursive*/ false);
	TArray<USoundWave*> Waves;
	Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
	for (const FAssetData& Asset : Found)
	{
		const FString Name = Asset.AssetName.ToString();
		if (!Name.StartsWith(Prefix) || (!Exclude.IsEmpty() && Name.Contains(Exclude)))
		{
			continue;
		}
		if (USoundWave* Wave = Cast<USoundWave>(Asset.GetAsset()))
		{
			Waves.Add(Wave);
		}
	}
	if (Waves.Num() == 0)
	{
		UE_LOG(LogBDDebug, Error, TEXT("BDBuildSoundCue: no sound wave named %s* in %s."), *Prefix, *Folder);
		return 1;
	}

	// A cue already there is rebuilt in place, so whatever points at it keeps pointing.
	const FString PackageName = FPackageName::ObjectPathToPackageName(CuePath);
	const FString CueName = FPackageName::GetShortName(PackageName);
	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();
	USoundCue* Cue = FindObject<USoundCue>(Package, *CueName);
	const bool bRebuilt = Cue != nullptr;
	if (Cue == nullptr)
	{
		Cue = NewObject<USoundCue>(Package, *CueName, RF_Public | RF_Standalone | RF_Transactional);
	}
	else
	{
		Cue->ResetGraph();
	}

	USoundNodeModulator* Modulator = Cue->ConstructSoundNode<USoundNodeModulator>();
	Modulator->PitchMin = 1.0f - PitchSpread;
	Modulator->PitchMax = 1.0f + PitchSpread;
	Modulator->VolumeMin = 1.0f - VolumeSpread;
	Modulator->VolumeMax = 1.0f;

	USoundNodeRandom* Random = Cue->ConstructSoundNode<USoundNodeRandom>();
	Random->bRandomizeWithoutReplacement = true;
	Random->PreselectAtLevelLoad = 0;

	const int32 Count = FMath::Min(Waves.Num(), Random->GetMaxChildNodes());
	if (Count < Waves.Num())
	{
		UE_LOG(LogBDDebug, Warning, TEXT("BDBuildSoundCue: a Random node takes %d inputs; %d waves left out."), Count, Waves.Num() - Count);
	}
	while (Random->ChildNodes.Num() < Count)
	{
		Random->InsertChildNode(Random->ChildNodes.Num());
	}
	while (Random->ChildNodes.Num() > Count)
	{
		Random->RemoveChildNode(Random->ChildNodes.Num() - 1);
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		USoundNodeWavePlayer* Player = Cue->ConstructSoundNode<USoundNodeWavePlayer>();
		Player->SetSoundWave(Waves[Index]);
		Player->GraphNode->NodePosX = -700;
		Player->GraphNode->NodePosY = (Index - Count / 2) * 100;
		Random->ChildNodes[Index] = Player;
	}

	if (Modulator->ChildNodes.Num() == 0)
	{
		Modulator->InsertChildNode(0);
	}
	Modulator->ChildNodes[0] = Random;
	Modulator->GraphNode->NodePosX = -200;
	Random->GraphNode->NodePosX = -450;
	Cue->FirstNode = Modulator;
	Cue->LinkGraphNodesFromSoundNodes();

	Cue->PitchMultiplier = CuePitch;
	if (USoundClass* Effects = UBDUISettings::Get().EffectsSoundClass.LoadSynchronous())
	{
		Cue->SoundClassObject = Effects;
	}
	Cue->PostEditChange();
	Cue->MarkPackageDirty();

	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, Cue, *Filename, SaveArgs))
	{
		UE_LOG(LogBDDebug, Error, TEXT("BDBuildSoundCue: %s could not be saved to %s."), *PackageName, *Filename);
		return 1;
	}

	UE_LOG(LogBDDebug, Display, TEXT("BDBuildSoundCue: %s %s with %d waves from %s, pitch +-%.0f%%, volume -%.0f%%, cue pitch %.2f, class %s, duration %.2f s."),
		bRebuilt ? TEXT("rebuilt") : TEXT("created"), *PackageName, Count, *Folder, PitchSpread * 100.0f, VolumeSpread * 100.0f, CuePitch,
		Cue->SoundClassObject != nullptr ? *Cue->SoundClassObject->GetName() : TEXT("none"), Cue->GetDuration());
	return 0;
#else
	UE_LOG(LogBDDebug, Error, TEXT("BDBuildSoundCue runs only in an editor build."));
	return 1;
#endif
}

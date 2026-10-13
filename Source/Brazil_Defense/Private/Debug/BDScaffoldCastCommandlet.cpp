// Brazil Defense. Tooling: lay out the cast - data assets, art folders and texts - for production.

#include "Debug/BDScaffoldCastCommandlet.h"

#include "Algo/Find.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "BDLog.h"
#include "Candidate/BDCandidateData.h"
#include "Cast/BDCast.h"
#include "Enemy/BDEnemyData.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Palace/BDMinisterData.h"
#include "Tower/BDShooterData.h"
#include "UI/BDUISettings.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

#if WITH_EDITOR
#include "Animation/Skeleton.h"
#include "AssetToolsModule.h"
#include "Engine/Engine.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/SkeletalMesh.h"
#include "IAssetTools.h"
#include "UObject/ObjectRedirector.h"
#endif

namespace BDScaffoldCastPrivate
{
	/** A character already in the game that becomes the first of its kind. */
	struct FReuse
	{
		const TCHAR* OldPackage;
		EBDCastKind Kind;
		/** What its table entries say: the name it shows today, so nothing on screen changes. */
		const TCHAR* Portuguese;
		const TCHAR* English;
	};

	static const FReuse Reused[] =
	{
		{ TEXT("/Game/BD/Data/DA_Candidate"), EBDCastKind::Candidate, TEXT("Candidato 01"), TEXT("Candidate 01") },
		{ TEXT("/Game/BD/Data/DA_Enemy_Test"), EBDCastKind::Horde, TEXT("Militantes"), TEXT("Militants") },
		{ TEXT("/Game/BD/Data/DA_Shooter_Pistol"), EBDCastKind::Shooter, TEXT("Nicole"), TEXT("Nicole") },
	};

	/** The provisional name of a character in each language: "Candidato 07" / "Candidate 07". */
	static FString ProvisionalName(const EBDCastKind Kind, const int32 Number, const bool bPortuguese)
	{
		static const TCHAR* const Portuguese[] = { TEXT("Candidato"), TEXT("Ministro"), TEXT("Personagem"), TEXT("Horda") };
		static const TCHAR* const English[] = { TEXT("Candidate"), TEXT("Minister"), TEXT("Character"), TEXT("Horde") };
		const int32 Index = static_cast<int32>(Kind);
		return FString::Printf(TEXT("%s %02d"), bPortuguese ? Portuguese[Index] : English[Index], Number);
	}

	static bool PackageExists(const FString& PackageName)
	{
		return FPackageName::DoesPackageExist(PackageName);
	}

#if WITH_EDITOR
	static bool SavePackageOf(UObject* Asset)
	{
		UPackage* Package = Asset->GetPackage();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
		{
			UE_LOG(LogBDDebug, Error, TEXT("BDScaffoldCast: %s could not be saved to %s."), *Package->GetName(), *Filename);
			return false;
		}
		UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast: saved %s."), *Package->GetName());
		return true;
	}

	/**
	 * Saves the cast's packages left dirty (an import path pointed at a moved source file).
	 * The rename saves the referencers it rewrites itself; the packages it emptied stay unsaved.
	 */
	static void SaveDirtyPackages()
	{
		TArray<UPackage*> Dirty;
		for (TObjectIterator<UPackage> It; It; ++It)
		{
			UObject* Asset = It->IsDirty() && It->GetName().StartsWith(TEXT("/Game/Elenco/")) ? It->FindAssetInPackage() : nullptr;
			if (Asset != nullptr && !Asset->IsA<UObjectRedirector>())
			{
				Dirty.Add(*It);
			}
		}
		for (UPackage* Package : Dirty)
		{
			const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(),
				Package->ContainsMap() ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			const bool bSaved = UPackage::SavePackage(Package, nullptr, *Filename, SaveArgs);
			UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast: %s %s."), bSaved ? TEXT("saved") : TEXT("COULD NOT SAVE"), *Package->GetName());
		}
	}

	/** The art subfolder an asset of this class goes to; empty for a class the cast does not sort. */
	static FString ArtFolderFor(const FString& ClassName)
	{
		if (ClassName == TEXT("SkeletalMesh") || ClassName == TEXT("StaticMesh") || ClassName == TEXT("Skeleton") || ClassName == TEXT("PhysicsAsset"))
		{
			return TEXT("Mesh");
		}
		if (ClassName.StartsWith(TEXT("Texture")))
		{
			return TEXT("Textures");
		}
		if (ClassName.StartsWith(TEXT("Material")))
		{
			return TEXT("Materials");
		}
		if (ClassName.StartsWith(TEXT("Anim")) || ClassName.StartsWith(TEXT("BlendSpace")))
		{
			return TEXT("Anim");
		}
		if (ClassName.StartsWith(TEXT("Sound")) || ClassName.StartsWith(TEXT("MetaSound")))
		{
			return TEXT("Audio");
		}
		return FString();
	}

	/**
	 * The art only this character uses: everything under /Game the data reaches, hard or soft,
	 * that nothing outside the set refers to, plus what lies unreferenced beside it in the
	 * character's own art folders (/Game/BD/Meshs/...). What anything else also uses stays.
	 */
	static TArray<FName> FindOwnArt(IAssetRegistry& Registry, const FName Root)
	{
		const UE::AssetRegistry::FDependencyQuery Query;
		const UE::AssetRegistry::EDependencyCategory Packages = UE::AssetRegistry::EDependencyCategory::Package;

		// Everything the data reaches, and what lies beside it in the character's own art
		// folders (/Game/BD/Meshs/...): a static mesh nothing points at is still his.
		TSet<FName> Reached = { Root };
		TArray<FName> Open = { Root };
		TSet<FString> Folders;
		while (Open.Num() > 0)
		{
			const FName Package = Open.Pop();
			TArray<FName> Next;
			Registry.GetDependencies(Package, Next, Packages, Query);
			const FString Folder = FPackageName::GetLongPackagePath(Package.ToString());
			if (Folder.StartsWith(TEXT("/Game/BD/Meshs/")) && !Folders.Contains(Folder))
			{
				Folders.Add(Folder);
				TArray<FAssetData> Beside;
				Registry.GetAssetsByPath(FName(*Folder), Beside, /*bRecursive*/ false);
				for (const FAssetData& Asset : Beside)
				{
					Next.Add(Asset.PackageName);
				}
			}
			for (const FName Each : Next)
			{
				if (Each.ToString().StartsWith(TEXT("/Game/")) && !Reached.Contains(Each))
				{
					Reached.Add(Each);
					Open.Add(Each);
				}
			}
		}

		// Then drop whatever something outside the set refers to, until nothing more drops: a
		// mesh, its skeleton and its physics refer to each other in a ring, and only a pass over
		// the whole set lets such a ring stay together.
		TSet<FName> Own = Reached;
		for (bool bDropped = true; bDropped; )
		{
			bDropped = false;
			for (auto It = Own.CreateIterator(); It; ++It)
			{
				if (*It == Root)
				{
					continue;
				}
				TArray<FName> Referencers;
				Registry.GetReferencers(*It, Referencers, Packages, Query);
				const FName* Outside = Referencers.FindByPredicate([&Own, &It](const FName Referencer) { return Referencer != *It && !Own.Contains(Referencer); });
				if (Outside != nullptr)
				{
					UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast:   %s stays: %s uses it too."), *It->ToString(), *Outside->ToString());
					It.RemoveCurrent();
					bDropped = true;
				}
			}
		}

		Own.Remove(Root);
		TArray<FName> Sorted = Own.Array();
		Sorted.Sort(FNameLexicalLess());
		return Sorted;
	}

	static UAssetImportData* GetImportData(UObject& Asset)
	{
		FObjectProperty* Property = FindFProperty<FObjectProperty>(Asset.GetClass(), TEXT("AssetImportData"));
		return Property != nullptr ? Cast<UAssetImportData>(Property->GetObjectPropertyValue_InContainer(&Asset)) : nullptr;
	}

	/** The files an asset was imported from that lie right beside it in the project (the .wav of a sound). */
	static TArray<FString> GetSourcesBeside(UObject& Asset)
	{
		TArray<FString> Beside;
		UAssetImportData* ImportData = GetImportData(Asset);
		if (ImportData == nullptr)
		{
			return Beside;
		}
		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::GetPath(FPackageName::LongPackageNameToFilename(Asset.GetPackage()->GetName())));
		for (const FString& Source : ImportData->ExtractFilenames())
		{
			const FString Full = FPaths::ConvertRelativePathToFull(Source);
			if (FPaths::IsSamePath(FPaths::GetPath(Full), Dir) && IFileManager::Get().FileExists(*Full))
			{
				Beside.Add(Full);
			}
		}
		return Beside;
	}

	/**
	 * Takes those files along to the asset's new folder and makes sure its import data names
	 * them there, so a reimport still finds them. One from Downloads stays where it is.
	 */
	static void MoveSources(UObject& Asset, const TArray<FString>& Sources)
	{
		UAssetImportData* ImportData = GetImportData(Asset);
		const FString NewDir = FPaths::ConvertRelativePathToFull(FPaths::GetPath(FPackageName::LongPackageNameToFilename(Asset.GetPackage()->GetName())));
		for (const FString& Source : Sources)
		{
			const FString Target = NewDir / FPaths::GetCleanFilename(Source);
			if (IFileManager::Get().FileExists(*Target) || !IFileManager::Get().Move(*Target, *Source))
			{
				UE_LOG(LogBDDebug, Warning, TEXT("BDScaffoldCast:   could not move %s beside %s."), *Source, *Asset.GetName());
				continue;
			}
			UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast:   source %s moved beside it."), *FPaths::GetCleanFilename(Source));
			const TArray<FString> Now = ImportData != nullptr ? ImportData->ExtractFilenames() : TArray<FString>();
			for (int32 Index = 0; Index < Now.Num(); ++Index)
			{
				if (FPaths::GetCleanFilename(Now[Index]) == FPaths::GetCleanFilename(Source) && !FPaths::IsSamePath(Now[Index], Target))
				{
					ImportData->UpdateFilenameOnly(Target, Index);
					Asset.MarkPackageDirty();
				}
			}
		}
	}

	/** Every art slot back to empty; the numbers stay. DeathAnimations gets its empty entries. */
	static void ClearArt(UObject& Data, const EBDCastKind Kind)
	{
		if (UBDEnemyData* Enemy = Cast<UBDEnemyData>(&Data))
		{
			Enemy->Mesh.Reset();
			Enemy->SkeletalMesh.Reset();
			Enemy->Skeleton.Reset();
			Enemy->MoveAnimation.Reset();
			Enemy->IdleAnimation.Reset();
			Enemy->DeathAnimations.Reset();
			Enemy->DeathAnimations.SetNum(Kind == EBDCastKind::Candidate ? 1 : 4);
			Enemy->MeshMaterial.Reset();
			Enemy->SkinMaterials.Reset();
			Enemy->KillType = NAME_None;
			Enemy->KillIcon.Reset();
			Enemy->CandidateFace.Reset();
			Enemy->SpeechSound.Reset();
			Enemy->CallSound.Reset();
			Enemy->FootstepSound.Reset();
			Enemy->DeathSound.Reset();
			Enemy->BodyFallSound.Reset();
			if (UBDCandidateData* Candidate = Cast<UBDCandidateData>(Enemy))
			{
				Candidate->Rally.RallySpeech.Reset();
			}
		}
		else if (UBDShooterData* Shooter = Cast<UBDShooterData>(&Data))
		{
			Shooter->BodyMesh.Reset();
			Shooter->IdleAnimation.Reset();
			Shooter->ShootAnimation.Reset();
			Shooter->CelebrationSound.Reset();
			for (FBDShooterWeapon& Weapon : Shooter->Weapons)
			{
				Weapon.Mesh.Reset();
				Weapon.Grip = FTransform::Identity;
				Weapon.MuzzleFlash.Reset();
				Weapon.FireSound.Reset();
			}
		}
	}

	static void SetDisplayName(UObject& Data, const FText& Name)
	{
		if (UBDEnemyData* Enemy = Cast<UBDEnemyData>(&Data)) { Enemy->DisplayName = Name; }
		else if (UBDMinisterData* Minister = Cast<UBDMinisterData>(&Data)) { Minister->DisplayName = Name; }
		else if (UBDShooterData* Shooter = Cast<UBDShooterData>(&Data)) { Shooter->DisplayName = Name; }
	}
#endif

	/** Appends the missing keys to one CSV string table. Returns how many it added. */
	static int32 AddTableKeys(const FString& RelativeFile, const TArray<TPair<FString, FString>>& Wanted, const bool bDryRun)
	{
		const FString Path = FPaths::ProjectContentDir() / RelativeFile;
		FString Csv;
		if (!FFileHelper::LoadFileToString(Csv, *Path))
		{
			UE_LOG(LogBDDebug, Error, TEXT("BDScaffoldCast: string table %s not found."), *Path);
			return 0;
		}
		const FString NewLine = Csv.Contains(TEXT("\r\n")) ? TEXT("\r\n") : TEXT("\n");
		FString Added;
		int32 Count = 0;
		for (const TPair<FString, FString>& Each : Wanted)
		{
			const FString Head = FString::Printf(TEXT("\"%s\","), *Each.Key);
			if (Csv.StartsWith(Head) || Csv.Contains(NewLine + Head) || Added.Contains(Head))
			{
				continue;
			}
			Added += FString::Printf(TEXT("%s\"%s\"%s"), *Head, *Each.Value.Replace(TEXT("\""), TEXT("\"\"")), *NewLine);
			++Count;
		}
		if (Count > 0 && !bDryRun)
		{
			if (!Csv.EndsWith(TEXT("\n")))
			{
				Csv += NewLine;
			}
			Csv += Added;
			if (!FFileHelper::SaveStringToFile(Csv, *Path, FFileHelper::EEncodingOptions::ForceUTF8))
			{
				UE_LOG(LogBDDebug, Error, TEXT("BDScaffoldCast: could not write %s."), *Path);
				return 0;
			}
		}
		UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast: %s %d keys to %s."), bDryRun ? TEXT("would add") : TEXT("added"), Count, *RelativeFile);
		return Count;
	}
}

UBDScaffoldCastCommandlet::UBDScaffoldCastCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UBDScaffoldCastCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace BDScaffoldCastPrivate;

	const bool bDryRun = FParse::Param(*Params, TEXT("DryRun"));
	const TCHAR* const Verb = bDryRun ? TEXT("would") : TEXT("will");

	// The counts: asked for, or the settings' when left out; never fewer than before.
	UBDCastSettings* Settings = GetMutableDefault<UBDCastSettings>();
	int32* const Counts[] = { &Settings->Candidatos, &Settings->Ministros, &Settings->Personagens, &Settings->Hordas };
	static const TCHAR* const Switches[] = { TEXT("Candidatos="), TEXT("Ministros="), TEXT("Personagens="), TEXT("Hordas=") };
	int32 Wanted[static_cast<int32>(EBDCastKind::Count)];
	for (int32 Kind = 0; Kind < static_cast<int32>(EBDCastKind::Count); ++Kind)
	{
		int32 Asked = *Counts[Kind];
		FParse::Value(*Params, Switches[Kind], Asked);
		if (Asked < *Counts[Kind])
		{
			UE_LOG(LogBDDebug, Warning, TEXT("BDScaffoldCast: %s asks for %d but the cast has %d; nothing is ever removed, %d it stays."),
				BDCast::GetFolder(static_cast<EBDCastKind>(Kind)), Asked, *Counts[Kind], *Counts[Kind]);
			Asked = *Counts[Kind];
		}
		Wanted[Kind] = FMath::Max(0, Asked);
	}
	UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast: %d candidates, %d ministers, %d characters, %d hordes%s."),
		Wanted[0], Wanted[1], Wanted[2], Wanted[3], bDryRun ? TEXT(" (dry run: nothing is written)") : TEXT(""));

	IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
	Registry.SearchAllAssets(/*bSynchronousSearch*/ true);
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

	// 1. The characters already in the game become the first of their kind.
	for (const FReuse& Reuse : Reused)
	{
		const FString NewPackage = BDCast::GetPackageName(Reuse.Kind, 1);
		if (PackageExists(NewPackage))
		{
			UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast: %s is in place."), *NewPackage);
			continue;
		}
		if (!PackageExists(Reuse.OldPackage))
		{
			UE_LOG(LogBDDebug, Warning, TEXT("BDScaffoldCast: neither %s nor %s exists; the first %s is created empty."),
				*NewPackage, Reuse.OldPackage, BDCast::GetStem(Reuse.Kind));
			continue;
		}

		const FString Folder = BDCast::GetFolderPath(Reuse.Kind, 1);
		const TArray<FName> Art = FindOwnArt(Registry, FName(Reuse.OldPackage));
		UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast: %s %s -> %s, with %d assets of its own:"), Verb, Reuse.OldPackage, *NewPackage, Art.Num());

		TArray<FAssetRenameData> Renames;
		TArray<TPair<FString, TArray<FString>>> Moves; // new package, its source files beside it
		TArray<FString> OldPackages;
		const auto Plan = [&](const FName Package, const FString& ToFolder, const FString& ToName)
		{
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPackageName(Package, Assets);
			if (Assets.Num() != 1)
			{
				UE_LOG(LogBDDebug, Warning, TEXT("BDScaffoldCast:   %s holds %d assets; left where it is."), *Package.ToString(), Assets.Num());
				return;
			}
			UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast:   %s -> %s/%s"), *Package.ToString(), *ToFolder, *ToName);
			if (!bDryRun)
			{
				if (UObject* Asset = Assets[0].GetAsset())
				{
					Renames.Emplace(Asset, ToFolder, ToName);
					Moves.Emplace(ToFolder / ToName, GetSourcesBeside(*Asset));
					OldPackages.Add(Package.ToString());
				}
			}
		};

		Plan(FName(Reuse.OldPackage), Folder, FPackageName::GetShortName(NewPackage));
		for (const FName Package : Art)
		{
			TArray<FAssetData> Assets;
			Registry.GetAssetsByPackageName(Package, Assets);
			const FString Sub = Assets.Num() == 1 ? ArtFolderFor(Assets[0].AssetClassPath.GetAssetName().ToString()) : FString();
			if (Sub.IsEmpty())
			{
				UE_LOG(LogBDDebug, Warning, TEXT("BDScaffoldCast:   %s is of no art kind the cast sorts; left where it is."), *Package.ToString());
				continue;
			}
			Plan(Package, Folder / Sub, FPackageName::GetShortName(Package));
		}

		if (bDryRun || Renames.Num() == 0)
		{
			continue;
		}
		if (!AssetTools.RenameAssets(Renames))
		{
			UE_LOG(LogBDDebug, Error, TEXT("BDScaffoldCast: the rename of %s failed; stopping."), Reuse.OldPackage);
			return 1;
		}
		// An asset still compiling when it is renamed (a mesh building its LODs) can leave its old
		// file behind, a copy nothing refers to any more. A redirector would still be in memory.
		for (const FString& Old : OldPackages)
		{
			const FString OldFile = FPackageName::LongPackageNameToFilename(Old, FPackageName::GetAssetPackageExtension());
			const FString OldObject = Old + TEXT(".") + FPackageName::GetShortName(Old);
			if (IFileManager::Get().FileExists(*OldFile) && FindObject<UObject>(nullptr, *OldObject) == nullptr)
			{
				const bool bDeleted = IFileManager::Get().Delete(*OldFile);
				UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast:   old copy of %s %s."), *Old, bDeleted ? TEXT("deleted") : TEXT("COULD NOT be deleted"));
			}
		}

		for (const TPair<FString, TArray<FString>>& Move : Moves)
		{
			const FString ObjectPath = Move.Key + TEXT(".") + FPackageName::GetShortName(Move.Key);
			UObject* Moved = Move.Value.Num() > 0 ? FindObject<UObject>(nullptr, *ObjectPath) : nullptr;
			if (Moved != nullptr)
			{
				MoveSources(*Moved, Move.Value);
			}
		}
	}

	if (!bDryRun)
	{
		// The renames leave redirectors behind: point every referencer at the new place and drop them.
		TArray<UObjectRedirector*> Redirectors;
		for (TObjectIterator<UObjectRedirector> It; It; ++It)
		{
			if (It->DestinationObject != nullptr && It->DestinationObject->GetPathName().StartsWith(TEXT("/Game/Elenco/")))
			{
				Redirectors.Add(*It);
			}
		}
		if (Redirectors.Num() > 0)
		{
			UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast: fixing up %d redirectors."), Redirectors.Num());
			AssetTools.FixupReferencers(Redirectors, /*bCheckoutDialogPrompt*/ false, ERedirectFixupMode::DeleteFixedUpRedirectors);
		}
		SaveDirtyPackages();
	}

	// 1b. The candidates already made become candidate data in place - same package, same name,
	// so nothing pointing at them notices - with the rallies of their own number.
	for (int32 Number = 1; Number <= Wanted[static_cast<int32>(EBDCastKind::Candidate)]; ++Number)
	{
		const FString PackageName = BDCast::GetPackageName(EBDCastKind::Candidate, Number);
		const FString ObjectPath = PackageName + TEXT(".") + FPackageName::GetShortName(PackageName);
		UBDEnemyData* Old = LoadObject<UBDEnemyData>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Old == nullptr || Old->IsA<UBDCandidateData>())
		{
			continue;
		}
		UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast: %s turn %s into candidate data, with the rallies of candidate %d."), Verb, *PackageName, Number);
		if (bDryRun)
		{
			continue;
		}
		UPackage* Package = Old->GetPackage();
		const FName Name = Old->GetFName();
		Old->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
		UBDCandidateData* Candidate = NewObject<UBDCandidateData>(Package, Name, RF_Public | RF_Standalone);
		UEngine::CopyPropertiesForUnrelatedObjects(Old, Candidate);
		Candidate->Rally = FBDRallyParams::DefaultsFor(Number);
		Old->ClearFlags(RF_Public | RF_Standalone);
		Old->MarkAsGarbage();
		if (!SavePackageOf(Candidate))
		{
			return 1;
		}
	}

	// 1c. A candidate whose crowd is still a shorter run of the default - hordes 1..k, weight 1
	// each, as the first scaffold wrote them all with Horda_01 alone - gets the run of his number.
	// A list anyone touched by hand (other kinds, other weights, another order) is left alone.
	for (int32 Number = 1; Number <= Wanted[static_cast<int32>(EBDCastKind::Candidate)]; ++Number)
	{
		UBDCandidateData* Candidate = Cast<UBDCandidateData>(BDCast::LoadData(EBDCastKind::Candidate, Number));
		if (Candidate == nullptr)
		{
			continue;
		}
		const TArray<FBDRallyCrowdEntry> Default = FBDRallyParams::DefaultsFor(Number).RallyComposition;
		TArray<FBDRallyCrowdEntry>& Crowd = Candidate->Rally.RallyComposition;
		bool bShorterRun = Crowd.Num() < Default.Num();
		for (int32 Index = 0; bShorterRun && Index < Crowd.Num(); ++Index)
		{
			bShorterRun = Crowd[Index].Enemy == Default[Index].Enemy && FMath::IsNearlyEqual(Crowd[Index].Weight, Default[Index].Weight);
		}
		if (!bShorterRun)
		{
			continue;
		}
		UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast: %s give %s the crowd of hordes 1..%d (had %d)."), Verb, *Candidate->GetName(), Default.Num(), Crowd.Num());
		if (bDryRun)
		{
			continue;
		}
		Crowd = Default;
		if (!SavePackageOf(Candidate))
		{
			return 1;
		}
	}

	// 2. Every data asset up to the count, created when missing, with the numbers of the first of its kind.
	int32 Created = 0;
	for (int32 KindIndex = 0; KindIndex < static_cast<int32>(EBDCastKind::Count); ++KindIndex)
	{
		const EBDCastKind Kind = static_cast<EBDCastKind>(KindIndex);
		const UObject* Template = Cast<UObject>(StaticLoadObject(BDCast::GetDataClass(Kind), nullptr, *BDCast::GetPackageName(Kind, 1), nullptr, LOAD_NoWarn | LOAD_Quiet));
		for (int32 Number = 1; Number <= Wanted[KindIndex]; ++Number)
		{
			const FString PackageName = BDCast::GetPackageName(Kind, Number);
			if (PackageExists(PackageName))
			{
				continue;
			}
			UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast: %s create %s%s."), Verb, *PackageName,
				Template != nullptr ? *FString::Printf(TEXT(" from the numbers of %s"), *Template->GetName()) : TEXT(""));
			++Created;
			if (bDryRun)
			{
				continue;
			}
			UPackage* Package = CreatePackage(*PackageName);
			const FName Name(*FPackageName::GetShortName(PackageName));
			UObject* Data = Template != nullptr
				? DuplicateObject<UObject>(Template, Package, Name)
				: NewObject<UObject>(Package, BDCast::GetDataClass(Kind), Name);
			Data->SetFlags(RF_Public | RF_Standalone);
			ClearArt(*Data, Kind);
			if (UBDCandidateData* Candidate = Cast<UBDCandidateData>(Data))
			{
				Candidate->Rally = FBDRallyParams::DefaultsFor(Number);
			}
			SetDisplayName(*Data, FText::FromString(ProvisionalName(Kind, Number, /*bPortuguese*/ true)));
			FAssetRegistryModule::AssetCreated(Data);
			if (!SavePackageOf(Data))
			{
				return 1;
			}
		}
	}

	// The creeps already made say which skeleton their animations go on.
	if (!bDryRun)
	{
		for (const EBDCastKind Kind : { EBDCastKind::Candidate, EBDCastKind::Horde })
		{
			for (int32 Number = 1; Number <= Wanted[static_cast<int32>(Kind)]; ++Number)
			{
				UBDEnemyData* Enemy = LoadObject<UBDEnemyData>(nullptr, *BDCast::GetPackageName(Kind, Number), nullptr, LOAD_NoWarn | LOAD_Quiet);
				USkeletalMesh* Body = Enemy != nullptr && Enemy->Skeleton.IsNull() ? Enemy->SkeletalMesh.LoadSynchronous() : nullptr;
				if (Body != nullptr && Body->GetSkeleton() != nullptr)
				{
					Enemy->Skeleton = Body->GetSkeleton();
					UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast: %s takes the skeleton of its body, %s."), *Enemy->GetName(), *Body->GetSkeleton()->GetName());
					SavePackageOf(Enemy);
				}
			}
		}
	}

	// 3. The art folders, held by git while they are empty.
	int32 Folders = 0;
	for (int32 KindIndex = 0; KindIndex < static_cast<int32>(EBDCastKind::Count); ++KindIndex)
	{
		for (int32 Number = 1; Number <= Wanted[KindIndex]; ++Number)
		{
			for (const FString& Sub : BDCast::GetArtFolders())
			{
				const FString Dir = FPackageName::LongPackageNameToFilename(BDCast::GetFolderPath(static_cast<EBDCastKind>(KindIndex), Number) / Sub + TEXT("/"));
				const FString Keep = Dir / TEXT(".gitkeep");
				if (IFileManager::Get().FileExists(*Keep))
				{
					continue;
				}
				++Folders;
				if (!bDryRun)
				{
					IFileManager::Get().MakeDirectory(*Dir, /*Tree*/ true);
					FFileHelper::SaveStringToFile(FString(), *Keep);
				}
			}
		}
	}
	UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast: %s add %d art folders with a .gitkeep."), Verb, Folders);

	// 4. The texts.
	TArray<TPair<FString, FString>> Portuguese;
	TArray<TPair<FString, FString>> English;
	for (int32 KindIndex = 0; KindIndex < static_cast<int32>(EBDCastKind::Count); ++KindIndex)
	{
		const EBDCastKind Kind = static_cast<EBDCastKind>(KindIndex);
		for (int32 Number = 1; Number <= Wanted[KindIndex]; ++Number)
		{
			const FReuse* Reuse = Number == 1 ? Algo::FindByPredicate(Reused, [Kind](const FReuse& Each) { return Each.Kind == Kind; }) : nullptr;
			Portuguese.Emplace(BDCast::GetTextKey(Kind, Number), Reuse != nullptr ? FString(Reuse->Portuguese) : ProvisionalName(Kind, Number, true));
			English.Emplace(BDCast::GetTextKey(Kind, Number), Reuse != nullptr ? FString(Reuse->English) : ProvisionalName(Kind, Number, false));
		}
	}
	const UBDUISettings& UI = UBDUISettings::Get();
	AddTableKeys(UI.PortugueseTableFile, Portuguese, bDryRun);
	AddTableKeys(UI.EnglishTableFile, English, bDryRun);

	// 5. The counts, for the regression to hold the cast to.
	if (!bDryRun)
	{
		for (int32 Kind = 0; Kind < static_cast<int32>(EBDCastKind::Count); ++Kind)
		{
			*Counts[Kind] = Wanted[Kind];
		}
		Settings->TryUpdateDefaultConfigFile();
	}
	UE_LOG(LogBDDebug, Display, TEXT("BDScaffoldCast: %s create %d data assets."), Verb, Created);

	BDCast::LogAudit(BDCast::RunAudit());
	return 0;
#else
	UE_LOG(LogBDDebug, Error, TEXT("BDScaffoldCast needs the editor."));
	return 1;
#endif
}

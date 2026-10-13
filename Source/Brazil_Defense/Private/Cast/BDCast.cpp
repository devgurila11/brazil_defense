// Brazil Defense. The cast: every character's data asset, where it lives and what it still lacks.

#include "Cast/BDCast.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "BDLog.h"
#include "Candidate/BDCandidateData.h"
#include "Misc/PackageName.h"
#include "Enemy/BDEnemyData.h"
#include "HAL/IConsoleManager.h"
#include "Palace/BDMinisterData.h"
#include "Tower/BDShooterData.h"

namespace BDCastPrivate
{
	static const TCHAR* const Root = TEXT("/Game/Elenco");

	static int32 Index(const EBDCastKind Kind)
	{
		return FMath::Clamp(static_cast<int32>(Kind), 0, static_cast<int32>(EBDCastKind::Count) - 1);
	}

	static void Add(TArray<BDCast::FSlot>& Slots, const TCHAR* Name, const bool bFilled)
	{
		Slots.Add({ Name, bFilled });
	}

	static bool HasAny(const TArray<TSoftObjectPtr<UMaterialInterface>>& Materials)
	{
		return Materials.ContainsByPredicate([](const TSoftObjectPtr<UMaterialInterface>& Each) { return !Each.IsNull(); });
	}
}

int32 UBDCastSettings::GetCount(const EBDCastKind Kind) const
{
	switch (Kind)
	{
	case EBDCastKind::Candidate: return Candidatos;
	case EBDCastKind::Minister: return Ministros;
	case EBDCastKind::Shooter: return Personagens;
	case EBDCastKind::Horde: return Hordas;
	default: return 0;
	}
}

const TCHAR* BDCast::GetFolder(const EBDCastKind Kind)
{
	static const TCHAR* const Folders[] = { TEXT("Candidatos"), TEXT("Ministros"), TEXT("Personagens"), TEXT("Hordas") };
	return Folders[BDCastPrivate::Index(Kind)];
}

const TCHAR* BDCast::GetStem(const EBDCastKind Kind)
{
	static const TCHAR* const Stems[] = { TEXT("Candidato"), TEXT("Ministro"), TEXT("Personagem"), TEXT("Horda") };
	return Stems[BDCastPrivate::Index(Kind)];
}

const TCHAR* BDCast::GetKeyStem(const EBDCastKind Kind)
{
	static const TCHAR* const Keys[] = { TEXT("Candidate"), TEXT("Minister"), TEXT("Shooter"), TEXT("Enemy") };
	return Keys[BDCastPrivate::Index(Kind)];
}

UClass* BDCast::GetDataClass(const EBDCastKind Kind)
{
	switch (Kind)
	{
	case EBDCastKind::Minister: return UBDMinisterData::StaticClass();
	case EBDCastKind::Shooter: return UBDShooterData::StaticClass();
	case EBDCastKind::Candidate: return UBDCandidateData::StaticClass();
	default: return UBDEnemyData::StaticClass();
	}
}

FString BDCast::GetName(const EBDCastKind Kind, const int32 Number)
{
	return FString::Printf(TEXT("%s_%02d"), GetStem(Kind), Number);
}

FString BDCast::GetFolderPath(const EBDCastKind Kind, const int32 Number)
{
	return FString::Printf(TEXT("%s/%s/%s"), BDCastPrivate::Root, GetFolder(Kind), *GetName(Kind, Number));
}

FString BDCast::GetPackageName(const EBDCastKind Kind, const int32 Number)
{
	return FString::Printf(TEXT("%s/DA_%s"), *GetFolderPath(Kind, Number), *GetName(Kind, Number));
}

FString BDCast::GetTextKey(const EBDCastKind Kind, const int32 Number)
{
	return FString::Printf(TEXT("%s.%02d"), GetKeyStem(Kind), Number);
}

const TArray<FString>& BDCast::GetArtFolders()
{
	static const TArray<FString> Folders = { TEXT("Mesh"), TEXT("Textures"), TEXT("Materials"), TEXT("Anim"), TEXT("Audio") };
	return Folders;
}

bool BDCast::ParseAssetName(const FString& AssetName, EBDCastKind& OutKind, int32& OutNumber)
{
	for (int32 Kind = 0; Kind < static_cast<int32>(EBDCastKind::Count); ++Kind)
	{
		const FString Prefix = FString::Printf(TEXT("DA_%s_"), GetStem(static_cast<EBDCastKind>(Kind)));
		if (!AssetName.StartsWith(Prefix, ESearchCase::CaseSensitive))
		{
			continue;
		}
		const FString Digits = AssetName.Mid(Prefix.Len());
		if (Digits.IsEmpty() || !Digits.IsNumeric() || Digits.Contains(TEXT(".")) || Digits.Contains(TEXT("-")))
		{
			return false;
		}
		OutKind = static_cast<EBDCastKind>(Kind);
		OutNumber = FCString::Atoi(*Digits);
		return OutNumber > 0;
	}
	return false;
}

FString BDCast::GetTextKeyOf(const UObject* Data)
{
	EBDCastKind Kind;
	int32 Number = 0;
	return Data != nullptr && ParseAssetName(Data->GetName(), Kind, Number) ? GetTextKey(Kind, Number) : FString();
}

void BDCast::GetSlots(const UObject& Data, const EBDCastKind Kind, TArray<FSlot>& OutSlots)
{
	using BDCastPrivate::Add;
	using BDCastPrivate::HasAny;
	OutSlots.Reset();

	if (const UBDEnemyData* Enemy = Cast<UBDEnemyData>(&Data))
	{
		const bool bCandidate = Kind == EBDCastKind::Candidate;
		Add(OutSlots, TEXT("Icon"), bCandidate ? !Enemy->CandidateFace.IsNull() : !Enemy->KillIcon.IsNull());
		Add(OutSlots, TEXT("SkeletalMesh"), !Enemy->SkeletalMesh.IsNull());
		Add(OutSlots, TEXT("Skeleton"), !Enemy->Skeleton.IsNull());
		Add(OutSlots, TEXT("MeshMaterial"), !Enemy->MeshMaterial.IsNull());
		Add(OutSlots, TEXT("SkinMaterials"), HasAny(Enemy->SkinMaterials));
		if (bCandidate)
		{
			Add(OutSlots, TEXT("Anim.Walk"), !Enemy->MoveAnimation.IsNull());
			Add(OutSlots, TEXT("Anim.Idle"), !Enemy->IdleAnimation.IsNull());
			Add(OutSlots, TEXT("Anim.Death"), Enemy->DeathAnimations.ContainsByPredicate([](const TSoftObjectPtr<UAnimSequenceBase>& Each) { return !Each.IsNull(); }));
			Add(OutSlots, TEXT("Sound.Speech"), !Enemy->SpeechSound.IsNull());
			Add(OutSlots, TEXT("Sound.Death"), !Enemy->DeathSound.IsNull());
			const UBDCandidateData* Candidate = Cast<UBDCandidateData>(Enemy);
			Add(OutSlots, TEXT("Sound.Rally"), Candidate != nullptr && !Candidate->Rally.RallySpeech.IsNull());
		}
		else
		{
			Add(OutSlots, TEXT("Anim.Run"), !Enemy->MoveAnimation.IsNull());
			const int32 Falls = FMath::Max(4, Enemy->DeathAnimations.Num());
			for (int32 Fall = 0; Fall < Falls; ++Fall)
			{
				const bool bFilled = Enemy->DeathAnimations.IsValidIndex(Fall) && !Enemy->DeathAnimations[Fall].IsNull();
				OutSlots.Add({ FString::Printf(TEXT("Anim.Death_%02d"), Fall + 1), bFilled });
			}
			Add(OutSlots, TEXT("Sound.Speech"), !Enemy->SpeechSound.IsNull());
			Add(OutSlots, TEXT("Sound.Call"), !Enemy->CallSound.IsNull());
			Add(OutSlots, TEXT("Sound.Footstep"), !Enemy->FootstepSound.IsNull());
			Add(OutSlots, TEXT("Sound.Death"), !Enemy->DeathSound.IsNull());
			Add(OutSlots, TEXT("Sound.BodyFall"), !Enemy->BodyFallSound.IsNull());
		}
	}
	else if (const UBDMinisterData* Minister = Cast<UBDMinisterData>(&Data))
	{
		Add(OutSlots, TEXT("Icon"), !Minister->Icon.IsNull());
		Add(OutSlots, TEXT("SkeletalMesh"), !Minister->SkeletalMesh.IsNull());
		Add(OutSlots, TEXT("Skeleton"), !Minister->Skeleton.IsNull());
		Add(OutSlots, TEXT("MeshMaterial"), !Minister->MeshMaterial.IsNull());
		Add(OutSlots, TEXT("SkinMaterials"), HasAny(Minister->SkinMaterials));
		Add(OutSlots, TEXT("Anim.Walk"), !Minister->WalkAnimation.IsNull());
		Add(OutSlots, TEXT("Anim.Idle"), !Minister->IdleAnimation.IsNull());
		Add(OutSlots, TEXT("Anim.Read"), !Minister->ReadAnimation.IsNull());
		Add(OutSlots, TEXT("Anim.Kicked"), !Minister->KickedAnimation.IsNull());
		Add(OutSlots, TEXT("Anim.Flee"), !Minister->FleeAnimation.IsNull());
		Add(OutSlots, TEXT("Sound.Speech"), !Minister->SpeechSound.IsNull());
		Add(OutSlots, TEXT("Sound.Flee"), !Minister->FleeSound.IsNull());
	}
	else if (const UBDShooterData* Shooter = Cast<UBDShooterData>(&Data))
	{
		Add(OutSlots, TEXT("SkeletalMesh"), !Shooter->BodyMesh.IsNull());
		Add(OutSlots, TEXT("Anim.Idle"), !Shooter->IdleAnimation.IsNull());
		Add(OutSlots, TEXT("Anim.Shooting"), !Shooter->ShootAnimation.IsNull());
		const int32 Weapons = FMath::Max(6, Shooter->Weapons.Num());
		for (int32 Weapon = 0; Weapon < Weapons; ++Weapon)
		{
			const bool bFilled = Shooter->Weapons.IsValidIndex(Weapon) && !Shooter->Weapons[Weapon].Mesh.IsNull();
			OutSlots.Add({ FString::Printf(TEXT("Weapon_%d"), Weapon + 1), bFilled });
		}
		Add(OutSlots, TEXT("Sound.Celebration"), !Shooter->CelebrationSound.IsNull());
	}
}

int32 BDCast::FAuditEntry::CountEmpty() const
{
	return Slots.FilterByPredicate([](const FSlot& Slot) { return !Slot.bFilled; }).Num();
}

const BDCast::FAuditEntry* BDCast::FAudit::Find(const EBDCastKind Kind, const int32 Number) const
{
	return Entries.FindByPredicate([Kind, Number](const FAuditEntry& Entry) { return Entry.Kind == Kind && Entry.Number == Number; });
}

int32 BDCast::FAudit::TotalFound() const
{
	int32 Total = 0;
	for (const int32 Each : Found) { Total += Each; }
	return Total;
}

int32 BDCast::FAudit::TotalEmpty() const
{
	int32 Total = 0;
	for (const int32 Each : Empty) { Total += Each; }
	return Total;
}

BDCast::FAudit BDCast::RunAudit()
{
	FAudit Audit;
	IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
	Registry.ScanPathsSynchronous({ BDCastPrivate::Root }, /*bForceRescan*/ false);

	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(BDCastPrivate::Root), Assets, /*bRecursive*/ true);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });

	for (const FAssetData& Asset : Assets)
	{
		EBDCastKind Kind;
		int32 Number = 0;
		if (!ParseAssetName(Asset.AssetName.ToString(), Kind, Number))
		{
			continue;
		}
		const UObject* Data = Asset.GetAsset();
		if (Data == nullptr || !Data->IsA(GetDataClass(Kind)))
		{
			UE_LOG(LogBDDebug, Warning, TEXT("BD.Cast.Audit: %s is not a %s; left out."), *Asset.PackageName.ToString(), *GetDataClass(Kind)->GetName());
			continue;
		}

		FAuditEntry& Entry = Audit.Entries.AddDefaulted_GetRef();
		Entry.Kind = Kind;
		Entry.Number = Number;
		Entry.PackageName = Asset.PackageName.ToString();
		GetSlots(*Data, Kind, Entry.Slots);
		++Audit.Found[BDCastPrivate::Index(Kind)];
		Audit.Empty[BDCastPrivate::Index(Kind)] += Entry.CountEmpty();
	}

	const UBDCastSettings& Settings = UBDCastSettings::Get();
	for (int32 Kind = 0; Kind < static_cast<int32>(EBDCastKind::Count); ++Kind)
	{
		for (int32 Number = 1; Number <= Settings.GetCount(static_cast<EBDCastKind>(Kind)); ++Number)
		{
			if (Audit.Find(static_cast<EBDCastKind>(Kind), Number) == nullptr)
			{
				Audit.Missing.Add(GetPackageName(static_cast<EBDCastKind>(Kind), Number));
			}
		}
	}
	return Audit;
}

void BDCast::LogAudit(const FAudit& Audit)
{
	UE_LOG(LogBDDebug, Display, TEXT("BD.Cast.Audit: the cast under /Game/Elenco, each data asset with its EMPTY slots."));
	for (const FAuditEntry& Entry : Audit.Entries)
	{
		FString Empty;
		for (const FSlot& Slot : Entry.Slots)
		{
			if (!Slot.bFilled)
			{
				Empty += (Empty.IsEmpty() ? TEXT("") : TEXT(", ")) + Slot.Name;
			}
		}
		UE_LOG(LogBDDebug, Display, TEXT("BD.Cast.Audit: %-34s %2d/%2d empty: %s"), *FPackageName::GetShortName(Entry.PackageName),
			Entry.CountEmpty(), Entry.Slots.Num(), Empty.IsEmpty() ? TEXT("none") : *Empty);
	}
	for (const FString& Missing : Audit.Missing)
	{
		UE_LOG(LogBDDebug, Warning, TEXT("BD.Cast.Audit: MISSING %s (the cast settings count it; run BDScaffoldCast)."), *Missing);
	}

	FString ByKind;
	for (int32 Kind = 0; Kind < static_cast<int32>(EBDCastKind::Count); ++Kind)
	{
		ByKind += FString::Printf(TEXT("%s%s %d DAs / %d empty"), ByKind.IsEmpty() ? TEXT("") : TEXT("; "),
			GetFolder(static_cast<EBDCastKind>(Kind)), Audit.Found[Kind], Audit.Empty[Kind]);
	}
	UE_LOG(LogBDDebug, Display, TEXT("BD.Cast.Audit: SUMMARY %d DAs, %d empty slots, %d missing - %s."),
		Audit.TotalFound(), Audit.TotalEmpty(), Audit.Missing.Num(), *ByKind);
}

namespace BDCastPrivate
{
	static FAutoConsoleCommand AuditCommand(
		TEXT("BD.Cast.Audit"),
		TEXT("Lists every data asset of the cast (/Game/Elenco) with the slots still empty - mesh, material, each animation, each sound, icon - and a summary by kind."),
		FConsoleCommandDelegate::CreateLambda([]() { BDCast::LogAudit(BDCast::RunAudit()); }));
}

// Brazil Defense. Resolving a data asset typed at the console.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectGlobals.h"

namespace BDDebugAssetLookup
{
	/**
	 * Resolves an asset from a full path or from its bare asset name. The name form walks
	 * the asset registry, so the console does not need the /Game/... prefix typed.
	 * Console tooling only: a registry walk per call is fine for a human at a prompt.
	 */
	template <typename TAsset>
	TAsset* FindByPathOrName(const FString& PathOrName)
	{
		if (PathOrName.Contains(TEXT("/")))
		{
			// "/Game/BD/Data/DA_Divider" and "/Game/BD/Data/DA_Divider.DA_Divider" both work.
			FString ObjectPath = PathOrName;
			if (!ObjectPath.Contains(TEXT(".")))
			{
				ObjectPath += TEXT(".") + FPackageName::GetShortName(ObjectPath);
			}
			return LoadObject<TAsset>(nullptr, *ObjectPath);
		}

		const IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByClass(TAsset::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses*/ true);

		const FName WantedName(*PathOrName);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetName == WantedName)
			{
				return Cast<TAsset>(Asset.GetAsset());
			}
		}

		return nullptr;
	}
}

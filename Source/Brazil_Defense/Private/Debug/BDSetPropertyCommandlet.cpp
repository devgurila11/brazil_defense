// Brazil Defense. Tooling: read or set one property of an asset from the command line.

#include "Debug/BDSetPropertyCommandlet.h"

#include "BDLog.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

UBDSetPropertyCommandlet::UBDSetPropertyCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UBDSetPropertyCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString AssetPath;
	FString PropertyName;
	FString Value;
	if (!FParse::Value(*Params, TEXT("Asset="), AssetPath) || !FParse::Value(*Params, TEXT("Property="), PropertyName))
	{
		UE_LOG(LogBDDebug, Error, TEXT("Usage: -run=BDSetProperty -Asset=/Game/Path/Asset -Property=Name [-Value=Text]"));
		return 1;
	}
	const bool bWrite = FParse::Value(*Params, TEXT("Value="), Value);

	// "/Game/BD/Data/DA_X" or "/Game/BD/Data/DA_X.DA_X": the object is the asset of the package.
	const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
	const FString ObjectPath = AssetPath.Contains(TEXT(".")) ? AssetPath : AssetPath + TEXT(".") + FPackageName::GetShortName(PackageName);
	UObject* Asset = LoadObject<UObject>(nullptr, *ObjectPath);
	if (Asset == nullptr)
	{
		UE_LOG(LogBDDebug, Error, TEXT("BDSetProperty: no asset at %s."), *ObjectPath);
		return 1;
	}

	FProperty* Property = Asset->GetClass()->FindPropertyByName(FName(*PropertyName));
	if (Property == nullptr)
	{
		UE_LOG(LogBDDebug, Error, TEXT("BDSetProperty: %s has no property %s."), *Asset->GetClass()->GetName(), *PropertyName);
		return 1;
	}

	void* Address = Property->ContainerPtrToValuePtr<void>(Asset);
	FString Before;
	Property->ExportTextItem_Direct(Before, Address, nullptr, Asset, PPF_None);
	if (!bWrite)
	{
		UE_LOG(LogBDDebug, Display, TEXT("BDSetProperty: %s.%s = %s"), *Asset->GetName(), *PropertyName, *Before);
		return 0;
	}

	if (Property->ImportText_Direct(*Value, Address, Asset, PPF_None) == nullptr)
	{
		UE_LOG(LogBDDebug, Error, TEXT("BDSetProperty: '%s' is not a value %s can take."), *Value, *PropertyName);
		return 1;
	}

	FString After;
	Property->ExportTextItem_Direct(After, Address, nullptr, Asset, PPF_None);
	Asset->MarkPackageDirty();

	UPackage* Package = Asset->GetPackage();
	const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, Asset, *Filename, SaveArgs))
	{
		UE_LOG(LogBDDebug, Error, TEXT("BDSetProperty: %s could not be saved to %s."), *Package->GetName(), *Filename);
		return 1;
	}

	UE_LOG(LogBDDebug, Display, TEXT("BDSetProperty: %s.%s %s -> %s, saved to %s."), *Asset->GetName(), *PropertyName, *Before, *After, *Filename);
	return 0;
#else
	// Saving assets is an editor thing; a packaged game has nothing to write them to.
	UE_LOG(LogBDDebug, Error, TEXT("BDSetProperty runs only in an editor build."));
	return 1;
#endif
}

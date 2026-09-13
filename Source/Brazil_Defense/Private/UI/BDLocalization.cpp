// Brazil Defense. The two languages of the interface and the texts they are read from.

#include "UI/BDLocalization.h"

#include "BDLog.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTableRegistry.h"
#include "Misc/Paths.h"
#include "UI/BDUISettings.h"

namespace BDLocPrivate
{
	static const FName TableIds[] = { TEXT("BD_en"), TEXT("BD_pt") };
	static const TCHAR* const CultureCodes[] = { TEXT("en"), TEXT("pt-BR") };

	static bool bInitialized = false;
	static EBDLanguage CurrentLanguage = EBDLanguage::English;
	static FBDOnLanguageChanged LanguageChanged;

	static FName CurrentTableId()
	{
		return TableIds[static_cast<int32>(CurrentLanguage)];
	}
}

void BDLoc::Initialize()
{
	using namespace BDLocPrivate;

	if (bInitialized)
	{
		return;
	}
	bInitialized = true;

	const UBDUISettings& Settings = UBDUISettings::Get();
	const FString Files[] = { Settings.EnglishTableFile, Settings.PortugueseTableFile };

	for (int32 Index = 0; Index < static_cast<int32>(EBDLanguage::Count); ++Index)
	{
		const FString FullPath = FPaths::ProjectContentDir() / Files[Index];
		if (!FPaths::FileExists(FullPath))
		{
			UE_LOG(LogBDUI, Error, TEXT("Text table %s not found at %s: its texts will show as keys."), *TableIds[Index].ToString(), *FullPath);
			continue;
		}

		FStringTableRegistry::Get().Internal_LocTableFromFile(TableIds[Index], TableIds[Index].ToString(), Files[Index], FPaths::ProjectContentDir());
		UE_LOG(LogBDUI, Log, TEXT("Text table %s loaded from %s."), *TableIds[Index].ToString(), *Files[Index]);
	}
}

FText BDLoc::Text(const TCHAR* Key)
{
	Initialize();
	return FText::FromStringTable(BDLocPrivate::CurrentTableId(), Key);
}

FText BDLoc::Format(const TCHAR* Key, const FFormatNamedArguments& Args)
{
	return FText::Format(Text(Key), Args);
}

EBDLanguage BDLoc::GetLanguage()
{
	return BDLocPrivate::CurrentLanguage;
}

const TCHAR* BDLoc::GetCultureCode(const EBDLanguage Language)
{
	return BDLocPrivate::CultureCodes[FMath::Clamp(static_cast<int32>(Language), 0, static_cast<int32>(EBDLanguage::Count) - 1)];
}

void BDLoc::SetLanguage(const EBDLanguage Language)
{
	using namespace BDLocPrivate;

	Initialize();

	const EBDLanguage Clamped = static_cast<EBDLanguage>(FMath::Clamp(static_cast<int32>(Language), 0, static_cast<int32>(EBDLanguage::Count) - 1));
	const bool bChanged = Clamped != CurrentLanguage;
	CurrentLanguage = Clamped;

	// The engine culture follows, so numbers, dates and any engine text format alike.
	// Failing to set it is not fatal: the tables are ours and switch regardless.
	if (!FInternationalization::Get().SetCurrentLanguageAndLocale(GetCultureCode(Clamped)))
	{
		UE_LOG(LogBDUI, Warning, TEXT("Engine culture %s could not be set; the interface texts switched anyway."), GetCultureCode(Clamped));
	}

	if (bChanged)
	{
		UE_LOG(LogBDUI, Log, TEXT("Language set to %s."), *StaticEnum<EBDLanguage>()->GetNameStringByValue(static_cast<int64>(Clamped)));
	}

	// Broadcast even when unchanged: a first application at startup has widgets to fill.
	LanguageChanged.Broadcast(Clamped);
}

FText BDLoc::GetLanguageName(const EBDLanguage Language)
{
	switch (Language)
	{
	case EBDLanguage::Portuguese:
		return Text(TEXT("Language.Portuguese"));
	default:
		return Text(TEXT("Language.English"));
	}
}

FBDOnLanguageChanged& BDLoc::OnLanguageChanged()
{
	return BDLocPrivate::LanguageChanged;
}

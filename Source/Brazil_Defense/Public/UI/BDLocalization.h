// Brazil Defense. The two languages of the interface and the texts they are read from.

#pragma once

#include "CoreMinimal.h"
#include "BDLocalization.generated.h"

/** Languages the interface speaks. The order is the order of the option list. */
UENUM(BlueprintType)
enum class EBDLanguage : uint8
{
	English UMETA(DisplayName = "English"),
	Portuguese UMETA(DisplayName = "Portuguese"),

	Count UMETA(Hidden)
};

/** Broadcast after the language changed. Every widget rebuilds its texts on it. */
DECLARE_MULTICAST_DELEGATE_OneParam(FBDOnLanguageChanged, EBDLanguage /*NewLanguage*/);

/**
 * Every text the player reads comes through here. There is one string table per
 * language, each loaded from its CSV (UBDUISettings), and a key resolves against the
 * table of the current language. Switching language switches the table and tells the
 * widgets, which rebuild their texts on the spot; the engine culture follows, so
 * numbers and dates format the same way.
 *
 * Nothing user facing is ever typed into code: a key that is missing from a table
 * shows up on screen as the key itself, which is the point.
 */
class UBDPlaceableData;
class UBDTowerData;
class UBDEnemyData;

namespace BDLoc
{
	/** Registers the tables. Safe to call more than once; done on first use as well. */
	BRAZIL_DEFENSE_API void Initialize();

	/** The text behind a key in the current language. */
	BRAZIL_DEFENSE_API FText Text(const TCHAR* Key);

	/** Same, with format arguments: the table entry is the format pattern. */
	BRAZIL_DEFENSE_API FText Format(const TCHAR* Key, const FFormatNamedArguments& Args);

	/** Whether the current table has a key. */
	BRAZIL_DEFENSE_API bool HasText(const TCHAR* Key);

	/**
	 * The name of a piece in the current language: the table entry "Piece.<asset name>"
	 * when there is one, else the asset's own display name, else the asset name. The
	 * display name typed into an asset is one language; the table is every language.
	 */
	BRAZIL_DEFENSE_API FText PieceName(const UBDPlaceableData* Data);

	/** The same for a defender, whose name lives on its tower data. */
	BRAZIL_DEFENSE_API FText PieceName(const UBDTowerData* Data);

	/** The name of a kind of enemy, the same way: "Enemy.<asset name>", else its display name, else the asset name. */
	BRAZIL_DEFENSE_API FText EnemyName(const UBDEnemyData* Data);

	BRAZIL_DEFENSE_API EBDLanguage GetLanguage();

	/** Switches the tables and the engine culture, and broadcasts OnLanguageChanged. */
	BRAZIL_DEFENSE_API void SetLanguage(EBDLanguage Language);

	/** Name of a language, in that language. */
	BRAZIL_DEFENSE_API FText GetLanguageName(EBDLanguage Language);

	/** Engine culture code of a language: "en" or "pt-BR". */
	BRAZIL_DEFENSE_API const TCHAR* GetCultureCode(EBDLanguage Language);

	BRAZIL_DEFENSE_API FBDOnLanguageChanged& OnLanguageChanged();
}

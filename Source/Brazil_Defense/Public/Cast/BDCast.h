// Brazil Defense. The cast: every character's data asset, where it lives and what it still lacks.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "BDCast.generated.h"

/** The four kinds of the cast. */
UENUM(BlueprintType)
enum class EBDCastKind : uint8
{
	Candidate,
	Minister,
	Shooter,
	Horde,

	Count UMETA(Hidden)
};

/**
 * How many of each kind the cast holds: what BDScaffoldCast was last asked for. The
 * commandlet writes them here when it grows the cast, and the regression holds the assets
 * on disk to them. Nothing in the game reads them.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Brazil Defense - Cast"))
class BRAZIL_DEFENSE_API UBDCastSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UBDCastSettings& Get() { return *GetDefault<UBDCastSettings>(); }

	UPROPERTY(config, EditAnywhere, Category = "Cast", meta = (ClampMin = "0"))
	int32 Candidatos = 0;

	UPROPERTY(config, EditAnywhere, Category = "Cast", meta = (ClampMin = "0"))
	int32 Ministros = 0;

	UPROPERTY(config, EditAnywhere, Category = "Cast", meta = (ClampMin = "0"))
	int32 Personagens = 0;

	UPROPERTY(config, EditAnywhere, Category = "Cast", meta = (ClampMin = "0"))
	int32 Hordas = 0;

	int32 GetCount(EBDCastKind Kind) const;
};

/**
 * The cast lives in Content/Elenco/<Kind>/<Name>/, one folder per character with its data
 * asset DA_<Name> and the subfolders Mesh, Textures, Materials, Anim and Audio for its art:
 *
 *   /Game/Elenco/Ministros/Ministro_01/DA_Ministro_01
 *
 * The asset name never changes when the real character arrives: only DisplayName and the
 * string tables do (Candidate.NN, Minister.NN, Shooter.NN, Enemy.NN).
 */
namespace BDCast
{
	/** "Candidatos", "Ministros", "Personagens", "Hordas". */
	BRAZIL_DEFENSE_API const TCHAR* GetFolder(EBDCastKind Kind);

	/** "Candidato", "Ministro", "Personagem", "Horda": the name before the number. */
	BRAZIL_DEFENSE_API const TCHAR* GetStem(EBDCastKind Kind);

	/** "Candidate", "Minister", "Shooter", "Enemy": the string table key before the number. */
	BRAZIL_DEFENSE_API const TCHAR* GetKeyStem(EBDCastKind Kind);

	/** The data class of a kind: UBDEnemyData, UBDMinisterData or UBDShooterData. */
	BRAZIL_DEFENSE_API UClass* GetDataClass(EBDCastKind Kind);

	/** "Candidato_07". */
	BRAZIL_DEFENSE_API FString GetName(EBDCastKind Kind, int32 Number);

	/** "/Game/Elenco/Candidatos/Candidato_07". */
	BRAZIL_DEFENSE_API FString GetFolderPath(EBDCastKind Kind, int32 Number);

	/** "/Game/Elenco/Candidatos/Candidato_07/DA_Candidato_07". */
	BRAZIL_DEFENSE_API FString GetPackageName(EBDCastKind Kind, int32 Number);

	/** "Candidate.07". */
	BRAZIL_DEFENSE_API FString GetTextKey(EBDCastKind Kind, int32 Number);

	/** The full object path of the Nth data asset: "/Game/.../DA_Horda_02.DA_Horda_02". */
	BRAZIL_DEFENSE_API FString GetObjectPath(EBDCastKind Kind, int32 Number);

	/**
	 * The Nth data asset of a kind, loaded; null when it does not exist or is not of the
	 * kind's class. The one answer to "which asset is the Horda N" for the waves and the
	 * rallies alike.
	 */
	BRAZIL_DEFENSE_API UObject* LoadData(EBDCastKind Kind, int32 Number);

	/** The subfolders every character gets for its art. */
	BRAZIL_DEFENSE_API const TArray<FString>& GetArtFolders();

	/** Reads "DA_Candidato_07" back into its kind and number. False for an asset outside the cast. */
	BRAZIL_DEFENSE_API bool ParseAssetName(const FString& AssetName, EBDCastKind& OutKind, int32& OutNumber);

	/** The string table key of a cast asset ("Candidate.07"); empty for any other asset. */
	BRAZIL_DEFENSE_API FString GetTextKeyOf(const UObject* Data);

	/** One slot of a character the production fills: a mesh, a material, an animation, a sound, an icon. */
	struct FSlot
	{
		FString Name;
		bool bFilled = false;
	};

	/** Every slot of a cast asset, filled or not, in the order of the checklist. */
	BRAZIL_DEFENSE_API void GetSlots(const UObject& Data, EBDCastKind Kind, TArray<FSlot>& OutSlots);

	/** One line of the audit. */
	struct FAuditEntry
	{
		EBDCastKind Kind = EBDCastKind::Count;
		int32 Number = 0;
		FString PackageName;
		TArray<FSlot> Slots;

		int32 CountEmpty() const;
	};

	/** What the audit found. */
	struct FAudit
	{
		TArray<FAuditEntry> Entries;

		/** Data assets found by kind, and their empty slots. */
		int32 Found[static_cast<int32>(EBDCastKind::Count)] = {};
		int32 Empty[static_cast<int32>(EBDCastKind::Count)] = {};

		/** Expected by UBDCastSettings and not on disk. */
		TArray<FString> Missing;

		const FAuditEntry* Find(EBDCastKind Kind, int32 Number) const;
		int32 TotalFound() const;
		int32 TotalEmpty() const;
	};

	/** Looks every cast asset up under /Game/Elenco (loading them) and lists its slots. */
	BRAZIL_DEFENSE_API FAudit RunAudit();

	/** Writes the audit to the log: one line per asset with its empty slots, then the summary. */
	BRAZIL_DEFENSE_API void LogAudit(const FAudit& Audit);
}

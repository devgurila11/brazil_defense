// Brazil Defense. One candidate of the cast: his body, like any creep's, and his rallies.

#include "Candidate/BDCandidateData.h"

#include "Cast/BDCast.h"
#include "Misc/PackageName.h"

FBDRallyParams FBDRallyParams::DefaultsFor(const int32 Ordinal)
{
	const int32 N = FMath::Clamp(Ordinal, 1, MaxOrdinal);
	const float Along = static_cast<float>(N - 1) / static_cast<float>(MaxOrdinal - 1);

	FBDRallyParams Params;
	Params.RallyEvery = FMath::Lerp(20.0f, 10.0f, Along);
	Params.RallyCount = 2 * N;

	// Kinds 1..N with the same weight, as far as the cast has hordes: no entry points at a DA
	// that does not exist yet. Which of them rise is the rally's to say (locked or bodiless
	// ones come up as donkeys).
	const int32 Kinds = FMath::Clamp(N, 1, FMath::Max(1, UBDCastSettings::Get().GetCount(EBDCastKind::Horde)));
	for (int32 Number = 1; Number <= Kinds; ++Number)
	{
		FBDRallyCrowdEntry& Entry = Params.RallyComposition.AddDefaulted_GetRef();
		Entry.Enemy = TSoftObjectPtr<UBDEnemyData>(FSoftObjectPath(BDCast::GetObjectPath(EBDCastKind::Horde, Number)));
		Entry.Weight = 1.0f;
	}
	return Params;
}

const UBDCandidateData* UBDCandidateData::LoadForOrdinal(const int32 Ordinal)
{
	const int32 Count = FMath::Max(1, UBDCastSettings::Get().GetCount(EBDCastKind::Candidate));
	const int32 Number = (FMath::Max(1, Ordinal) - 1) % Count + 1;
	return Cast<UBDCandidateData>(BDCast::LoadData(EBDCastKind::Candidate, Number));
}

FBDRallyParams UBDCandidateData::RallyForOrdinal(const int32 Ordinal)
{
	const UBDCandidateData* Data = LoadForOrdinal(Ordinal);
	return Data != nullptr && Data->Rally.IsSet() ? Data->Rally : FBDRallyParams::DefaultsFor(Ordinal);
}

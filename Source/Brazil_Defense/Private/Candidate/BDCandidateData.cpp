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

	// Kinds 1..N with the same weight, once they exist. Only the first has art today, so
	// every candidate raises the first kind; the list is there for the other nineteen.
	const FString Horde = BDCast::GetPackageName(EBDCastKind::Horde, 1);
	FBDRallyCrowdEntry& Entry = Params.RallyComposition.AddDefaulted_GetRef();
	Entry.Enemy = TSoftObjectPtr<UBDEnemyData>(FSoftObjectPath(Horde + TEXT(".") + FPackageName::GetShortName(Horde)));
	Entry.Weight = 1.0f;
	return Params;
}

const UBDCandidateData* UBDCandidateData::LoadForOrdinal(const int32 Ordinal)
{
	const int32 Count = FMath::Max(1, UBDCastSettings::Get().GetCount(EBDCastKind::Candidate));
	const int32 Number = (FMath::Max(1, Ordinal) - 1) % Count + 1;
	const FString Package = BDCast::GetPackageName(EBDCastKind::Candidate, Number);
	return LoadObject<UBDCandidateData>(nullptr, *(Package + TEXT(".") + FPackageName::GetShortName(Package)), nullptr, LOAD_NoWarn | LOAD_Quiet);
}

FBDRallyParams UBDCandidateData::RallyForOrdinal(const int32 Ordinal)
{
	const UBDCandidateData* Data = LoadForOrdinal(Ordinal);
	return Data != nullptr && Data->Rally.IsSet() ? Data->Rally : FBDRallyParams::DefaultsFor(Ordinal);
}

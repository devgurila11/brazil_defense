// Brazil Defense. Tooling: read or set one property of an asset from the command line.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "BDSetPropertyCommandlet.generated.h"

/**
 * Reads, and optionally writes and saves, one top level property of an asset, without
 * opening the editor. For the value tweaks a briefing asks of a data asset:
 *
 *   UnrealEditor-Cmd <uproject> -run=BDSetProperty -Asset=/Game/BD/Data/DA_Difficulty_Easy -Property=DividerBudget [-Value=100]
 *
 * Without -Value it only logs what the asset holds. The value is parsed the way the
 * details panel parses it (ImportText), so anything a property can be typed as works.
 */
UCLASS()
class BRAZIL_DEFENSE_API UBDSetPropertyCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UBDSetPropertyCommandlet();

	//~ Begin UCommandlet interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet interface
};

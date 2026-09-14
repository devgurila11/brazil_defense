// Brazil Defense. A match written down, to be picked up again at the start of a wave.

#include "Save/BDMatchSave.h"

#include "BDLog.h"
#include "Kismet/GameplayStatics.h"

const TCHAR* UBDMatchSave::SlotName = TEXT("BDMatch");

bool UBDMatchSave::Exists()
{
	return UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
}

UBDMatchSave* UBDMatchSave::LoadFromSlot()
{
	if (!Exists())
	{
		return nullptr;
	}

	UBDMatchSave* Save = Cast<UBDMatchSave>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	if (Save == nullptr)
	{
		UE_LOG(LogBDMatch, Error, TEXT("Saved match in slot '%s' could not be read."), SlotName);
	}
	return Save;
}

bool UBDMatchSave::WriteToSlot() const
{
	// SaveGameToSlot takes a mutable object only because of its signature; nothing is changed.
	return UGameplayStatics::SaveGameToSlot(const_cast<UBDMatchSave*>(this), SlotName, UserIndex);
}

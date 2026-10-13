// Brazil Defense. A match written down, to be picked up again at the start of a wave.

#include "Save/BDMatchSave.h"

#include "BDLog.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"

const TCHAR* UBDMatchSave::PlayerSlotName = TEXT("BDMatch");
const TCHAR* UBDMatchSave::TestSlotName = TEXT("BDMatch_Test");

namespace BDMatchSavePrivate
{
	static bool bTestSlotAsked = false;
}

bool UBDMatchSave::IsTestSlot()
{
	// A headless check runs unattended: it must never leave its board in the player's
	// Continue, which is how a test layout once became the player's saved match.
	static const bool bFromCommandLine = FApp::IsUnattended() || FParse::Param(FCommandLine::Get(), TEXT("BDTestSlot"));
	return bFromCommandLine || BDMatchSavePrivate::bTestSlotAsked;
}

void UBDMatchSave::SetTestSlot(const bool bTest)
{
	BDMatchSavePrivate::bTestSlotAsked = bTest;
}

const TCHAR* UBDMatchSave::GetSlotName()
{
	return IsTestSlot() ? TestSlotName : PlayerSlotName;
}

bool UBDMatchSave::Exists()
{
	return UGameplayStatics::DoesSaveGameExist(GetSlotName(), UserIndex);
}

bool UBDMatchSave::Delete()
{
	if (!Exists())
	{
		return true;
	}
	const bool bDeleted = UGameplayStatics::DeleteGameInSlot(GetSlotName(), UserIndex);
	UE_LOG(LogBDMatch, Log, TEXT("Saved match in slot '%s' %s."), GetSlotName(), bDeleted ? TEXT("deleted") : TEXT("could NOT be deleted"));
	return bDeleted;
}

UBDMatchSave* UBDMatchSave::LoadFromSlot()
{
	if (!Exists())
	{
		return nullptr;
	}

	UBDMatchSave* Save = Cast<UBDMatchSave>(UGameplayStatics::LoadGameFromSlot(GetSlotName(), UserIndex));
	if (Save == nullptr)
	{
		UE_LOG(LogBDMatch, Error, TEXT("Saved match in slot '%s' could not be read."), GetSlotName());
	}
	return Save;
}

bool UBDMatchSave::WriteToSlot() const
{
	// SaveGameToSlot takes a mutable object only because of its signature; nothing is changed.
	return UGameplayStatics::SaveGameToSlot(const_cast<UBDMatchSave*>(this), GetSlotName(), UserIndex);
}

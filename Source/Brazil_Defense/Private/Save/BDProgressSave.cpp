// Brazil Defense. What the player has beaten, kept across matches and sessions.

#include "Save/BDProgressSave.h"

#include "BDLog.h"
#include "Kismet/GameplayStatics.h"
#include "Save/BDMatchSave.h"

const TCHAR* UBDProgressSave::PlayerSlotName = TEXT("BDProgress");
const TCHAR* UBDProgressSave::TestSlotName = TEXT("BDProgress_Test");

const TCHAR* UBDProgressSave::GetSlotName()
{
	// A test winning a match must not hand the player a chain bonus he never earned: that
	// is how Easy and Hard once read as won without ever being played.
	return UBDMatchSave::IsTestSlot() ? TestSlotName : PlayerSlotName;
}

namespace BDProgressPrivate
{
	static uint8 BitOf(const EBDDifficulty Difficulty)
	{
		return static_cast<uint8>(1u << static_cast<uint8>(Difficulty));
	}
}

bool UBDProgressSave::HasWonDifficulty(const EBDDifficulty Difficulty)
{
	const UBDProgressSave* Progress = LoadOrCreate();
	return Progress != nullptr && Progress->HasWon(Difficulty);
}

void UBDProgressSave::RecordWin(const EBDDifficulty Difficulty)
{
	UBDProgressSave* Progress = LoadOrCreate();
	if (Progress == nullptr || Progress->HasWon(Difficulty))
	{
		return;
	}

	Progress->MarkWon(Difficulty);
	if (Progress->WriteToSlot())
	{
		UE_LOG(LogBDMatch, Log, TEXT("Progress: %s won, recorded."),
			*StaticEnum<EBDDifficulty>()->GetNameStringByValue(static_cast<int64>(Difficulty)));
	}
	else
	{
		UE_LOG(LogBDMatch, Error, TEXT("Progress: the win on %s could not be written."),
			*StaticEnum<EBDDifficulty>()->GetNameStringByValue(static_cast<int64>(Difficulty)));
	}
}

void UBDProgressSave::ResetProgress()
{
	if (UGameplayStatics::DoesSaveGameExist(GetSlotName(), UserIndex))
	{
		UGameplayStatics::DeleteGameInSlot(GetSlotName(), UserIndex);
	}
	UE_LOG(LogBDMatch, Warning, TEXT("Progress: every win forgotten."));
}

UBDProgressSave* UBDProgressSave::LoadOrCreate()
{
	if (UGameplayStatics::DoesSaveGameExist(GetSlotName(), UserIndex))
	{
		if (UBDProgressSave* Loaded = Cast<UBDProgressSave>(UGameplayStatics::LoadGameFromSlot(GetSlotName(), UserIndex)))
		{
			return Loaded;
		}
		UE_LOG(LogBDMatch, Error, TEXT("Progress slot '%s' could not be read; starting over."), GetSlotName());
	}

	return Cast<UBDProgressSave>(UGameplayStatics::CreateSaveGameObject(StaticClass()));
}

bool UBDProgressSave::HasWon(const EBDDifficulty Difficulty) const
{
	return Difficulty < EBDDifficulty::Count && (WonMask & BDProgressPrivate::BitOf(Difficulty)) != 0;
}

void UBDProgressSave::MarkWon(const EBDDifficulty Difficulty)
{
	if (Difficulty < EBDDifficulty::Count)
	{
		WonMask |= BDProgressPrivate::BitOf(Difficulty);
	}
}

bool UBDProgressSave::WriteToSlot() const
{
	return UGameplayStatics::SaveGameToSlot(const_cast<UBDProgressSave*>(this), GetSlotName(), UserIndex);
}

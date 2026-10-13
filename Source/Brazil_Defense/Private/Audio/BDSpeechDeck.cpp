// Brazil Defense. Lines out of a bank without repeats.

#include "Audio/BDSpeechDeck.h"

#include "BDLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeRandom.h"

int32 FBDShuffleBag::Draw(const int32 Count, FRandomStream* Stream)
{
	if (Count <= 1)
	{
		return 0;
	}

	if (Count != BagCount)
	{
		BagCount = Count;
		Left.Reset();
		LastDrawn = INDEX_NONE;
	}

	const auto Pick = [Stream](const int32 Max) { return Stream != nullptr ? Stream->RandRange(0, Max) : FMath::RandRange(0, Max); };

	if (Left.Num() == 0)
	{
		for (int32 Item = 0; Item < Count; ++Item)
		{
			Left.Add(Item);
		}
		// The first of the new round is anything but the last of the old one.
		if (LastDrawn != INDEX_NONE)
		{
			Left.Remove(LastDrawn);
			const int32 First = Left[Pick(Left.Num() - 1)];
			Left.Remove(First);
			Left.Add(LastDrawn);
			LastDrawn = First;
			return First;
		}
	}

	const int32 Index = Pick(Left.Num() - 1);
	LastDrawn = Left[Index];
	Left.RemoveAtSwap(Index);
	return LastDrawn;
}

UBDSpeechDeck* UBDSpeechDeck::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return World != nullptr ? World->GetSubsystem<UBDSpeechDeck>() : nullptr;
}

USoundNode* UBDSpeechDeck::FindRandomNode(const USoundCue& Cue)
{
	TArray<USoundNode*> Open;
	Open.Add(Cue.FirstNode);
	for (int32 Head = 0; Head < Open.Num(); ++Head)
	{
		USoundNode* Node = Open[Head];
		if (Node == nullptr)
		{
			continue;
		}
		if (Node->IsA<USoundNodeRandom>())
		{
			return Node;
		}
		for (USoundNode* Child : Node->ChildNodes)
		{
			Open.Add(Child);
		}
	}
	return nullptr;
}

FBDSpeechDeckEntry* UBDSpeechDeck::FindOrBuild(USoundBase* Bank)
{
	if (FBDSpeechDeckEntry* Found = Decks.Find(Bank))
	{
		return Found;
	}
	if (Plain.Contains(Bank))
	{
		return nullptr;
	}

	USoundCue* Cue = Cast<USoundCue>(Bank);
	const USoundNode* Random = Cue != nullptr ? FindRandomNode(*Cue) : nullptr;
	const int32 Count = Random != nullptr ? Random->ChildNodes.Num() : 0;
	if (Count <= 1)
	{
		Plain.Add(Bank);
		return nullptr;
	}

	// One copy per line. The copy's random node is found again by the same walk, which
	// meets the nodes in the same order, and is stepped over: whoever pointed at it points
	// at that line instead.
	FBDSpeechDeckEntry Entry;
	for (int32 Line = 0; Line < Count; ++Line)
	{
		USoundCue* Copy = DuplicateObject<USoundCue>(Cue, this, MakeUniqueObjectName(this, USoundCue::StaticClass(),
			*FString::Printf(TEXT("%s_Line%02d"), *Cue->GetName(), Line)));
		USoundNode* CopyRandom = Copy != nullptr ? FindRandomNode(*Copy) : nullptr;
		if (CopyRandom == nullptr || !CopyRandom->ChildNodes.IsValidIndex(Line))
		{
			UE_LOG(LogBDWave, Error, TEXT("Speech deck: the copy of %s lost its random node; the bank plays as it is."), *Cue->GetName());
			Plain.Add(Bank);
			return nullptr;
		}
		USoundNode* Chosen = CopyRandom->ChildNodes[Line];
		if (Copy->FirstNode == CopyRandom)
		{
			Copy->FirstNode = Chosen;
		}
		TArray<USoundNode*> Open;
		Open.Add(Copy->FirstNode);
		for (int32 Head = 0; Head < Open.Num(); ++Head)
		{
			USoundNode* Node = Open[Head];
			if (Node == nullptr || Node == Chosen)
			{
				continue;
			}
			for (TObjectPtr<USoundNode>& Child : Node->ChildNodes)
			{
				if (Child == CopyRandom)
				{
					Child = Chosen;
				}
				Open.Add(Child);
			}
		}
		// The length of this line alone, which is what a speech waits on.
		Copy->CacheAggregateValues();
		Entry.Lines.Add(Copy);
	}

	UE_LOG(LogBDWave, Log, TEXT("Speech deck: %s holds %d line(s), each said once before any again."), *Cue->GetName(), Count);
	return &Decks.Add(Bank, MoveTemp(Entry));
}

USoundBase* UBDSpeechDeck::DrawLine(USoundBase* Bank)
{
	if (Bank == nullptr)
	{
		return nullptr;
	}
	FBDSpeechDeckEntry* Entry = FindOrBuild(Bank);
	if (Entry == nullptr)
	{
		return Bank;
	}
	return Entry->Lines[Entry->Bag.Draw(Entry->Lines.Num())];
}

int32 UBDSpeechDeck::GetLineCount(USoundBase* Bank)
{
	const FBDSpeechDeckEntry* Entry = Bank != nullptr ? FindOrBuild(Bank) : nullptr;
	return Entry != nullptr ? Entry->Lines.Num() : 1;
}

int32 UBDSpeechDeck::GetLineIndex(USoundBase* Bank, const USoundBase* Line)
{
	const FBDSpeechDeckEntry* Entry = Bank != nullptr ? FindOrBuild(Bank) : nullptr;
	if (Entry == nullptr)
	{
		return Line == Bank ? 0 : INDEX_NONE;
	}
	return Entry->Lines.IndexOfByPredicate([Line](const TObjectPtr<USoundCue>& Each) { return Each == Line; });
}

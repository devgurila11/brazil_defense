// Brazil Defense. Who is talking: an exclamation over whoever is saying a sentence, coloured by side.

#include "Audio/BDSpeechMarks.h"

#include "BDLog.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemy/BDEnemyBase.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Palace/BDAgent.h"
#include "Sound/SoundBase.h"
#include "Tower/BDShooter.h"

UBDSpeechMarkSubsystem* UBDSpeechMarkSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject != nullptr ? WorldContextObject->GetWorld() : nullptr;
	return World != nullptr ? World->GetSubsystem<UBDSpeechMarkSubsystem>() : nullptr;
}

bool UBDSpeechMarkSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

EBDSpeakerSide UBDSpeechMarkSubsystem::SideOf(const AActor& Speaker)
{
	// Everything that walks at the urn is the other side, the candidates with it; the rest
	// that talks is ours. The ministers will say so themselves.
	return Speaker.IsA<ABDEnemyBase>() ? EBDSpeakerSide::Opponent : EBDSpeakerSide::Player;
}

void UBDSpeechMarkSubsystem::NoteSpeech(AActor& Speaker, UAudioComponent& Audio)
{
	NoteSpeech(Speaker, Audio, SideOf(Speaker));
}

void UBDSpeechMarkSubsystem::NoteSpeech(AActor& Speaker, UAudioComponent& Audio, const EBDSpeakerSide Side)
{
	// One mark a speaker: a new sentence takes the old mark over and pops it again.
	FMark* Mark = Marks.FindByPredicate([&Speaker](const FMark& Other) { return Other.Speaker.Get() == &Speaker; });
	if (Mark == nullptr)
	{
		Mark = &Marks.AddDefaulted_GetRef();
		Mark->Speaker = &Speaker;
	}
	Mark->Audio = &Audio;
	Mark->Side = Side;
	Mark->StartTime = FPlatformTime::Seconds();

	++MarksStarted;
	++MarksBySide[static_cast<int32>(Side)];
	UE_LOG(LogBDUI, Verbose, TEXT("Speech mark over %s (%s) for %s, %.2f s."), *Speaker.GetName(), *UEnum::GetValueAsString(Side),
		*GetNameSafe(Audio.Sound), Audio.Sound != nullptr ? Audio.Sound->GetDuration() : 0.0f);
}

int32 UBDSpeechMarkSubsystem::CountMarks(const AActor& Speaker) const
{
	return Marks.FilterByPredicate([&Speaker](const FMark& Mark) { return Mark.Speaker.Get() == &Speaker; }).Num();
}

const TArray<UBDSpeechMarkSubsystem::FMark>& UBDSpeechMarkSubsystem::GetLiveMarks()
{
	Prune();
	return Marks;
}

void UBDSpeechMarkSubsystem::Prune()
{
	// Off as soon as the sound is: stopped, finished and destroyed, or the speaker gone.
	const double Now = FPlatformTime::Seconds();
	Marks.RemoveAll([Now](const FMark& Mark)
	{
		const UAudioComponent* Audio = Mark.Audio.Get();
		const bool bLive = Mark.Speaker.IsValid() && Audio != nullptr && Audio->IsPlaying();
		if (!bLive)
		{
			UE_LOG(LogBDUI, Verbose, TEXT("Speech mark over %s gone after %.2f s."), *GetNameSafe(Mark.Speaker.Get()), Now - Mark.StartTime);
		}
		return !bLive;
	});
}

namespace BDSpeechMarkCommands
{
	static void ExecStatus(const TArray<FString>& Args, UWorld* World)
	{
		UBDSpeechMarkSubsystem* Marks = UBDSpeechMarkSubsystem::Get(World);
		if (Marks == nullptr)
		{
			UE_LOG(LogBDDebug, Warning, TEXT("BD.Speech.Status: no speech marks in this world."));
			return;
		}
		const int32 Live = Marks->GetLiveMarks().Num();
		UE_LOG(LogBDDebug, Log, TEXT("BD.Speech.Status: %d mark(s) started (%d opponent, %d player, %d minister), %d showing now."),
			Marks->GetMarksStarted(), Marks->GetMarksStarted(EBDSpeakerSide::Opponent), Marks->GetMarksStarted(EBDSpeakerSide::Player),
			Marks->GetMarksStarted(EBDSpeakerSide::Minister), Live);
	}

	/** A sentence, near silent, said by the nth actor of a class: to see a side's mark on anyone. */
	static void ExecSay(const TArray<FString>& Args, UWorld* World)
	{
		UBDSpeechMarkSubsystem* Marks = UBDSpeechMarkSubsystem::Get(World);
		if (Marks == nullptr || Args.Num() < 1)
		{
			UE_LOG(LogBDDebug, Error, TEXT("BD.Speech.Say <class text> [side -1 his own, 0 opponent, 1 player, 2 minister] [nth=0]."));
			return;
		}
		int32 Skip = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 0;
		AActor* Speaker = nullptr;
		for (TActorIterator<AActor> It(World); It && Speaker == nullptr; ++It)
		{
			if (It->GetClass()->GetName().Contains(Args[0]) && Skip-- <= 0)
			{
				Speaker = *It;
			}
		}
		USoundBase* Words = LoadObject<USoundBase>(nullptr, TEXT("/Game/BD/Audio/SCue_Militante_Falas.SCue_Militante_Falas"));
		if (Speaker == nullptr || Words == nullptr)
		{
			UE_LOG(LogBDDebug, Warning, TEXT("BD.Speech.Say: no actor of a class with '%s', or no sentence to say."), *Args[0]);
			return;
		}
		UAudioComponent* Audio = UGameplayStatics::SpawnSoundAttached(Words, Speaker->GetRootComponent(), NAME_None, FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset, /*bStopWhenAttachedToDestroyed*/ true, /*Volume*/ 0.05f);
		if (Audio == nullptr)
		{
			UE_LOG(LogBDDebug, Warning, TEXT("BD.Speech.Say: no audio device, so no sentence and no mark."));
			return;
		}
		const int32 AskedSide = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : -1;
		const EBDSpeakerSide Side = AskedSide >= 0
			? static_cast<EBDSpeakerSide>(FMath::Min(AskedSide, 2)) : UBDSpeechMarkSubsystem::SideOf(*Speaker);
		Marks->NoteSpeech(*Speaker, *Audio, Side);
		UE_LOG(LogBDDebug, Log, TEXT("BD.Speech.Say: %s says a sentence as %s."), *Speaker->GetName(), *UEnum::GetValueAsString(Side));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSay(
		TEXT("BD.Speech.Say"),
		TEXT("BD.Speech.Say <class text> [side -1 his own, 0 opponent, 1 player, 2 minister] [nth=0]: the nth such actor says a sentence, near silent, with the mark of the side (his own by default)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSay));

	/** The nth shooter or Agent celebrates now, from his own bank, the chance taken as won. */
	static void ExecCelebrate(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || Args.Num() < 1)
		{
			UE_LOG(LogBDDebug, Error, TEXT("BD.Speech.Celebrate <BDShooter|BDAgent> [nth=0]."));
			return;
		}
		int32 Skip = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->GetClass()->GetName().Contains(Args[0]) || Skip-- > 0)
			{
				continue;
			}
			bool bSaid = false;
			if (ABDShooter* Shooter = Cast<ABDShooter>(*It))
			{
				bSaid = Shooter->DebugCelebrate();
			}
			else if (ABDAgent* Agent = Cast<ABDAgent>(*It))
			{
				bSaid = Agent->DebugCelebrate();
			}
			UE_LOG(LogBDDebug, Log, TEXT("BD.Speech.Celebrate: %s %s."), *It->GetName(), bSaid ? TEXT("says his line") : TEXT("keeps quiet (BD.Sound.Stats tells why)"));
			return;
		}
		UE_LOG(LogBDDebug, Warning, TEXT("BD.Speech.Celebrate: no actor of a class with '%s'."), *Args[0]);
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdCelebrate(
		TEXT("BD.Speech.Celebrate"),
		TEXT("BD.Speech.Celebrate <BDShooter|BDAgent> [nth=0]: the nth such character says a celebration from his own bank now, as a kill whose chance came up (his interval and the board's gap still hold)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecCelebrate));

	static FAutoConsoleCommandWithWorldAndArgs CmdStatus(
		TEXT("BD.Speech.Status"),
		TEXT("BD.Speech.Status: how many speech marks have started, by side, and how many show now."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));
}

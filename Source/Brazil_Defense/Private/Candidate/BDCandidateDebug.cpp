// Brazil Defense. Console access to the candidate, for meeting it without losing first.

#include "BDLog.h"
#include "Candidate/BDCandidateSubsystem.h"
#include "Enemy/BDCandidate.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Match/BDGameBalanceSettings.h"
#include "Match/BDMatchManager.h"

namespace BDCandidateDebug
{
	static UBDCandidateSubsystem* FindCandidates(const UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogBDCandidate, Error, TEXT("This command needs a world."));
			return nullptr;
		}

		UBDCandidateSubsystem* Candidates = World->GetSubsystem<UBDCandidateSubsystem>();
		if (Candidates == nullptr)
		{
			UE_LOG(LogBDCandidate, Error, TEXT("No candidate subsystem in this world."));
		}

		return Candidates;
	}

	static void ExecSpawn(const TArray<FString>& Args, UWorld* World)
	{
		if (UBDCandidateSubsystem* Candidates = FindCandidates(World))
		{
			// Forced: the scoreboard is not consulted. The refusals (one already out, no
			// route, match over) still apply and are logged by the subsystem.
			Candidates->SpawnCandidate(TEXT("BD.Candidate.Spawn forced it"));
		}
	}

	static void ExecStatus(const TArray<FString>& Args, UWorld* World)
	{
		const UBDCandidateSubsystem* Candidates = FindCandidates(World);
		if (Candidates == nullptr)
		{
			return;
		}

		const ABDMatchManager* Match = ABDMatchManager::Get(World);
		const UBDGameBalanceSettings& Balance = UBDGameBalanceSettings::Get();

		FString State;
		if (const ABDCandidate* Candidate = Candidates->GetCandidate())
		{
			State = FString::Printf(TEXT("%s out of mouth %d, health %.0f/%.0f, cell %d of %d, alive %.1fs"),
				*Candidate->GetName(), Candidate->SpawnPointIndex, Candidate->GetCurrentHealth(), Candidate->GetMaxHealth(),
				Candidate->GetCurrentPathIndex(), Candidate->GetPath().Num(), Candidate->GetTimeAlive());
		}
		else if (Candidates->IsCountFrozen())
		{
			State = FString::Printf(TEXT("none out; count frozen for %.1fs more, waves held"), Candidates->GetPauseRemaining());
		}
		else
		{
			State = TEXT("none out");
		}

		UE_LOG(LogBDCandidate, Log, TEXT("BD.Candidate.Status: %s | scoreboard %d blue / %d red (%s) | %d sent this match | health x%.0f, %.2f cells/s, pause %.0fs."),
			*State,
			Match != nullptr ? Match->GetVotesBlue() : 0, Match != nullptr ? Match->GetVotesRed() : 0,
			Candidates->IsScoreboardInverted() ? TEXT("inverted") : TEXT("not inverted"),
			Candidates->GetCandidatesSent(),
			Balance.CandidateHealthMultiplier, Balance.CandidateSpeed, Balance.CandidateKillPauseSeconds);
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSpawn(
		TEXT("BD.Candidate.Spawn"),
		TEXT("BD.Candidate.Spawn: sends the candidate out now, whatever the scoreboard says. One at a time."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSpawn));

	static FAutoConsoleCommandWithWorldAndArgs CmdStatus(
		TEXT("BD.Candidate.Status"),
		TEXT("BD.Candidate.Status: logs the candidate on the board, the pause and whether the scoreboard calls for one."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));
}

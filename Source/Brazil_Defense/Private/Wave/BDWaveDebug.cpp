// Brazil Defense. Console access to the creeps, until waves have content and a HUD.

#include "BDDebugAssetLookup.h"
#include "BDLog.h"
#include "Enemy/BDEnemyBase.h"
#include "Enemy/BDEnemyData.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Match/BDMatchManager.h"
#include "Wave/BDWaveSettings.h"
#include "Wave/BDWaveSubsystem.h"

namespace BDWaveDebug
{
	static constexpr int32 ArgCountSpawnMin = 1;
	static constexpr int32 ArgCountSpawnMax = 2;
	static constexpr int32 ArgCountSpawnAllMax = 1;

	static UBDWaveSubsystem* FindWaves(const UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogBDWave, Error, TEXT("This command needs a world."));
			return nullptr;
		}

		UBDWaveSubsystem* Waves = World->GetSubsystem<UBDWaveSubsystem>();
		if (Waves == nullptr)
		{
			UE_LOG(LogBDWave, Error, TEXT("No wave subsystem in this world."));
		}

		return Waves;
	}

	/**
	 * The enemy to spawn: the one named on the command line, or the debug enemy of the
	 * wave settings when nothing was named.
	 */
	static const UBDEnemyData* ResolveEnemy(const TArray<FString>& Args, const int32 NameArgIndex, const TCHAR* Command)
	{
		if (Args.IsValidIndex(NameArgIndex))
		{
			const UBDEnemyData* Data = BDDebugAssetLookup::FindByPathOrName<UBDEnemyData>(Args[NameArgIndex]);
			if (Data == nullptr)
			{
				UE_LOG(LogBDWave, Error, TEXT("%s: no UBDEnemyData found for '%s'."), Command, *Args[NameArgIndex]);
			}
			return Data;
		}

		const UBDWaveSettings& Settings = UBDWaveSettings::Get();
		const UBDEnemyData* Data = Settings.DebugEnemy.LoadSynchronous();
		if (Data == nullptr)
		{
			UE_LOG(LogBDWave, Error,
				TEXT("%s: no enemy named and no Debug Enemy set in Project Settings > Brazil Defense - Waves."), Command);
		}

		return Data;
	}

	static void ExecSpawn(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < ArgCountSpawnMin || Args.Num() > ArgCountSpawnMax)
		{
			UE_LOG(LogBDWave, Error, TEXT("Usage: BD.Wave.Spawn <spawn point index> [enemy asset path or name]"));
			return;
		}

		UBDWaveSubsystem* Waves = FindWaves(World);
		const UBDEnemyData* Data = ResolveEnemy(Args, 1, TEXT("BD.Wave.Spawn"));
		if (Waves == nullptr || Data == nullptr)
		{
			return;
		}

		Waves->SpawnEnemy(Data, FCString::Atoi(*Args[0]));
	}

	static void ExecSpawnAll(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() > ArgCountSpawnAllMax)
		{
			UE_LOG(LogBDWave, Error, TEXT("Usage: BD.Wave.SpawnAll [enemy asset path or name]"));
			return;
		}

		UBDWaveSubsystem* Waves = FindWaves(World);
		const UBDEnemyData* Data = ResolveEnemy(Args, 0, TEXT("BD.Wave.SpawnAll"));
		if (Waves == nullptr || Data == nullptr)
		{
			return;
		}

		const int32 Spawned = Waves->SpawnEnemyAtEveryPoint(Data);
		UE_LOG(LogBDWave, Log, TEXT("BD.Wave.SpawnAll: %d of %d spawn point(s) sent a %s out."),
			Spawned, Waves->GetSpawnPointCount(), *Data->GetName());
	}

	static constexpr int32 ArgCountSpawnLoopMin = 1;
	static constexpr int32 ArgCountSpawnLoopMax = 2;

	static void ExecSpawnLoop(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < ArgCountSpawnLoopMin || Args.Num() > ArgCountSpawnLoopMax)
		{
			UE_LOG(LogBDWave, Error, TEXT("Usage: BD.Wave.SpawnLoop <seconds between spawns, 0 to stop> [enemy asset path or name]"));
			return;
		}

		UBDWaveSubsystem* Waves = FindWaves(World);
		if (Waves == nullptr)
		{
			return;
		}

		const float Interval = FCString::Atof(*Args[0]);
		if (Interval <= 0.0f)
		{
			if (!Waves->IsSpawnLoopRunning())
			{
				UE_LOG(LogBDWave, Log, TEXT("BD.Wave.SpawnLoop: no loop running."));
			}
			Waves->StopSpawnLoop();
			return;
		}

		const UBDEnemyData* Data = ResolveEnemy(Args, 1, TEXT("BD.Wave.SpawnLoop"));
		if (Data != nullptr)
		{
			Waves->StartSpawnLoop(Data, Interval);
		}
	}

	static void ExecKillAll(const TArray<FString>& Args, UWorld* World)
	{
		if (UBDWaveSubsystem* Waves = FindWaves(World))
		{
			UE_LOG(LogBDWave, Log, TEXT("BD.Wave.KillAll: %d creep(s) killed."), Waves->KillAll());
		}
	}

	static void ExecStatus(const TArray<FString>& Args, UWorld* World)
	{
		UBDWaveSubsystem* Waves = FindWaves(World);
		if (Waves == nullptr)
		{
			return;
		}

		const TArray<FBDSpawnPoint>& Points = Waves->GetSpawnPoints();
		FVector ObjectiveLocation;
		const FString ObjectiveText = Waves->GetObjectiveLocation(ObjectiveLocation)
			? ObjectiveLocation.ToCompactString()
			: TEXT("none");
		const ABDMatchManager* Match = World != nullptr ? ABDMatchManager::Get(World) : nullptr;
		UE_LOG(LogBDWave, Log, TEXT("BD.Wave.Status: %d spawn point(s), %d creep(s) on the board, %d of the wave still to send, health x%.2f on wave %d, objective at %s."),
			Points.Num(), Waves->GetLivingEnemyCount(), Waves->GetWaveSpawnsRemaining(),
			Match != nullptr ? Match->GetHealthScale() : 1.0f, Match != nullptr ? Match->GetCurrentWave() : 0, *ObjectiveText);

		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const FBDSpawnPoint& Point = Points[Index];
			const FString Route = Point.Route.Num() > 0
				? FString::Printf(TEXT("%d cells to %s"), Point.Route.Num(), *Point.Route.Last().ToString())
				: TEXT("NO ROUTE");

			UE_LOG(LogBDWave, Log, TEXT("  point %d: %d cell(s) from %s, exit %s, %s."),
				Index, Point.Cells.Num(), *Point.Cells[0].ToString(), *Point.ExitCell.ToString(), *Route);
		}

		TArray<ABDEnemyBase*> Enemies;
		Waves->GetLivingEnemies(Enemies);
		for (const ABDEnemyBase* Enemy : Enemies)
		{
			UE_LOG(LogBDWave, Log, TEXT("  %s from point %d: cell %d of %d, heading %s, %.0f cm/s, health %.0f/%.0f."),
				*Enemy->GetName(), Enemy->SpawnPointIndex, Enemy->GetCurrentPathIndex(), Enemy->GetPath().Num(),
				*Enemy->GetHeadingCell().ToString(), Enemy->GetCurrentSpeed(), Enemy->GetCurrentHealth(), Enemy->GetMaxHealth());
		}
	}

	static void ExecVotesStatus(const TArray<FString>& Args, UWorld* World)
	{
		const ABDMatchManager* Match = World != nullptr ? ABDMatchManager::Get(World) : nullptr;
		if (Match == nullptr)
		{
			UE_LOG(LogBDMatch, Error, TEXT("No match manager in this world. Is the game running?"));
			return;
		}

		UE_LOG(LogBDMatch, Log, TEXT("BD.Votes.Status: %d blue (kills) / %d red (arrivals)."),
			Match->GetVotesBlue(), Match->GetVotesRed());
	}

	static void ExecVotesAddBlue(const TArray<FString>& Args, UWorld* World)
	{
		ABDMatchManager* Match = World != nullptr ? ABDMatchManager::Get(World) : nullptr;
		if (Args.Num() != 1 || Match == nullptr)
		{
			UE_LOG(LogBDMatch, Error, TEXT("Usage: BD.Votes.AddBlue <votes> (needs a running match)"));
			return;
		}

		// Debug only: hands the player votes to spend, so a move tax can be tested without a wave of kills.
		Match->AddVotesBlue(FCString::Atoi(*Args[0]));
		UE_LOG(LogBDMatch, Log, TEXT("BD.Votes.AddBlue: now %d blue / %d red."), Match->GetVotesBlue(), Match->GetVotesRed());
	}

	static void ExecVotesAddRed(const TArray<FString>& Args, UWorld* World)
	{
		ABDMatchManager* Match = World != nullptr ? ABDMatchManager::Get(World) : nullptr;
		if (Args.Num() != 1 || Match == nullptr)
		{
			UE_LOG(LogBDMatch, Error, TEXT("Usage: BD.Votes.AddRed <votes> (needs a running match)"));
			return;
		}

		Match->AddVotesRed(FCString::Atoi(*Args[0]));
		UE_LOG(LogBDMatch, Log, TEXT("BD.Votes.AddRed: now %d blue / %d red."), Match->GetVotesBlue(), Match->GetVotesRed());
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdVotesAddRed(
		TEXT("BD.Votes.AddRed"),
		TEXT("BD.Votes.AddRed <votes>: debug, scores votes against the player, to test the scoreboard warnings."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecVotesAddRed));

	static FAutoConsoleCommandWithWorldAndArgs CmdVotesAddBlue(
		TEXT("BD.Votes.AddBlue"),
		TEXT("BD.Votes.AddBlue <votes>: debug, gives the player blue votes to spend."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecVotesAddBlue));

	static FAutoConsoleCommandWithWorldAndArgs CmdSpawn(
		TEXT("BD.Wave.Spawn"),
		TEXT("BD.Wave.Spawn <spawn point index> [enemy]: sends one creep out of one spawn point. Default enemy from the wave settings."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSpawn));

	static FAutoConsoleCommandWithWorldAndArgs CmdSpawnAll(
		TEXT("BD.Wave.SpawnAll"),
		TEXT("BD.Wave.SpawnAll [enemy]: sends one creep out of every spawn point."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSpawnAll));

	static FAutoConsoleCommandWithWorldAndArgs CmdSpawnLoop(
		TEXT("BD.Wave.SpawnLoop"),
		TEXT("BD.Wave.SpawnLoop <seconds> [enemy]: one creep out of every spawn point every N seconds until BD.Wave.SpawnLoop 0. Load testing."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSpawnLoop));

	static FAutoConsoleCommandWithWorldAndArgs CmdKillAll(
		TEXT("BD.Wave.KillAll"),
		TEXT("BD.Wave.KillAll: kills every creep on the board. Their votes on death are scored."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecKillAll));

	static FAutoConsoleCommandWithWorldAndArgs CmdStatus(
		TEXT("BD.Wave.Status"),
		TEXT("BD.Wave.Status: lists the spawn points, their routes and every creep out."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));

	static FAutoConsoleCommandWithWorldAndArgs CmdVotesStatus(
		TEXT("BD.Votes.Status"),
		TEXT("BD.Votes.Status: logs the blue and red vote counters."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecVotesStatus));
}

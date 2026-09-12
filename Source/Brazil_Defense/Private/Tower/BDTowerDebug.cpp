// Brazil Defense. Console view of the towers on the board.

#include "BDLog.h"
#include "Enemy/BDEnemyBase.h"
#include "Grid/BDGridSubsystem.h"
#include "Match/BDMatchManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Platform/BDPlatformComponent.h"
#include "Tower/BDTowerBase.h"
#include "Tower/BDTowerData.h"

namespace BDTowerDebug
{
	static void ExecStatus(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			UE_LOG(LogBDTower, Error, TEXT("This command needs a world."));
			return;
		}

		int32 Count = 0;
		for (TActorIterator<ABDTowerBase> It(World); It; ++It)
		{
			const ABDTowerBase* Tower = *It;
			const UBDTowerData* Data = Tower->GetData();
			const FBDTowerLevel* Level = Tower->GetCurrentLevel();
			const ABDEnemyBase* Target = Tower->GetCurrentTarget();

			const FString Where = Tower->IsOnPlatform()
				? FString::Printf(TEXT("slot %d of %s (x%.2f)"), Tower->GetPlatformSlotIndex(),
					*GetNameSafe(Tower->GetPlatform()->GetOwner()), Tower->GetPlatform()->RangeMultiplier)
				: FString::Printf(TEXT("ground cell %s"), *Tower->GetGroundCoord().ToString());

			const FString Magazine = Data != nullptr && Data->MagazineSize > 0
				? FString::Printf(TEXT("%d/%d in magazine"), Tower->GetShotsLeftInMagazine(), Data->MagazineSize)
				: TEXT("no magazine");
			const FString Reload = Tower->IsReloading()
				? FString::Printf(TEXT("reloading, %.1fs left (%.0f%%)"), Tower->GetReloadRemaining(), Tower->GetReloadProgress() * 100.0f)
				: TEXT("not reloading");
			const FString TargetText = Target != nullptr
				? FString::Printf(TEXT("%s (%s, aim error %.1f deg)"), *Target->GetName(),
					Tower->GetAcquisitionRemaining() > 0.0f ? TEXT("acquiring") : (Tower->IsAligned() ? TEXT("aligned") : TEXT("turning")),
					Tower->GetAimError())
				: TEXT("none");

			const FString UpgradeText = Tower->IsMaxLevel()
				? TEXT("max level")
				: FString::Printf(TEXT("next level %d votes for %.1f dmg"), Tower->GetUpgradeCost(), Tower->GetDamageAtNextLevel());

			UE_LOG(LogBDTower, Log,
				TEXT("  %s (%s): level %d/%d (%s) | %s | range %.2f cells = %.0f cm | %.1f dmg @ %.2f/s | target %s | %s | %s | %d shot(s), %d kill(s)."),
				*Tower->GetName(), *GetNameSafe(Data), Tower->GetTowerLevel(), UBDTowerData::MaxLevels, *UpgradeText, *Where,
				Tower->GetEffectiveRangeCells(), Tower->GetEffectiveRange(),
				Tower->GetEffectiveDamage(), Level != nullptr ? Level->FireRate : 0.0f,
				*TargetText, *Magazine, *Reload,
				Tower->GetShotsFired(), Tower->GetKills());
			++Count;
		}

		UE_LOG(LogBDTower, Log, TEXT("BD.Tower.Status: %d tower(s) on the board."), Count);
	}

	/**
	 * The defender standing on a cell: the ground tower there, or the occupant of the slot
	 * nearest the cell center on a platform there. Null when there is none.
	 */
	static ABDTowerBase* FindTowerAt(UWorld* World, const FBDCellCoord& Coord)
	{
		const UBDGridSubsystem* Grid = World != nullptr ? World->GetSubsystem<UBDGridSubsystem>() : nullptr;
		if (Grid == nullptr)
		{
			return nullptr;
		}

		ABDTowerBase* Nearest = nullptr;
		float NearestDistance = TNumericLimits<float>::Max();
		const FVector Center = Grid->CellToWorld(Coord);

		for (TActorIterator<ABDTowerBase> It(World); It; ++It)
		{
			ABDTowerBase* Tower = *It;
			if (!Tower->IsOnPlatform() && Tower->GetGroundCoord() == Coord)
			{
				return Tower;
			}

			// A slot defender counts when its platform covers the cell.
			if (Tower->IsOnPlatform())
			{
				TArray<FBDCellCoord> Cells;
				Tower->GetPlatform()->GetFootprintCells(Cells);
				if (!Cells.Contains(Coord))
				{
					continue;
				}

				const float Distance = FVector::DistSquared2D(Tower->GetActorLocation(), Center);
				if (Distance < NearestDistance)
				{
					Nearest = Tower;
					NearestDistance = Distance;
				}
			}
		}

		return Nearest;
	}

	static void ExecUpgrade(const TArray<FString>& Args, UWorld* World, const bool bConfirm)
	{
		if (Args.Num() != 2)
		{
			UE_LOG(LogBDTower, Error, TEXT("Usage: %s <x> <y>"), bConfirm ? TEXT("BD.Tower.Upgrade") : TEXT("BD.Tower.UpgradeCost"));
			return;
		}

		const FBDCellCoord Coord(FCString::Atoi(*Args[0]), FCString::Atoi(*Args[1]));
		ABDTowerBase* Tower = FindTowerAt(World, Coord);
		if (Tower == nullptr)
		{
			UE_LOG(LogBDTower, Error, TEXT("No defender on or over cell %s."), *Coord.ToString());
			return;
		}

		FString Reason;
		const bool bCan = Tower->CanUpgrade(Reason);

		// The deal, before it is taken: cost, the score it leaves, what it buys, and the warning.
		UE_LOG(LogBDTower, Log, TEXT("%s %s"), *Tower->DescribeUpgrade(),
			bCan ? (bConfirm ? TEXT("Buying.") : TEXT("BD.Tower.Upgrade to confirm.")) : TEXT(""));

		if (bConfirm && bCan)
		{
			Tower->Upgrade();
		}
	}

	static void ExecUpgradeCost(const TArray<FString>& Args, UWorld* World) { ExecUpgrade(Args, World, false); }
	static void ExecUpgradeConfirm(const TArray<FString>& Args, UWorld* World) { ExecUpgrade(Args, World, true); }

	static void ExecUpgradeAll(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			return;
		}

		// Debug: one level to every defender, for nothing. The paid path is BD.Tower.Upgrade.
		int32 Count = 0;
		for (TActorIterator<ABDTowerBase> It(World); It; ++It)
		{
			if (!It->IsMaxLevel())
			{
				It->DebugSetLevel(It->GetTowerLevel() + 1);
				++Count;
			}
		}

		UE_LOG(LogBDTower, Log, TEXT("BD.Tower.UpgradeAll: %d defender(s) raised one level, for nothing."), Count);
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdUpgradeAll(
		TEXT("BD.Tower.UpgradeAll"),
		TEXT("BD.Tower.UpgradeAll: debug, raises every defender one level for nothing."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecUpgradeAll));

	static void ExecSetAllLevels(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != 1 || World == nullptr)
		{
			UE_LOG(LogBDTower, Error, TEXT("Usage: BD.Tower.SetAllLevels <level>"));
			return;
		}

		const int32 NewLevel = FCString::Atoi(*Args[0]);
		int32 Count = 0;
		for (TActorIterator<ABDTowerBase> It(World); It; ++It)
		{
			It->DebugSetLevel(NewLevel);
			++Count;
		}

		UE_LOG(LogBDTower, Log, TEXT("BD.Tower.SetAllLevels: %d defender(s) set to level %d, for nothing."), Count, FMath::Clamp(NewLevel, 1, UBDTowerData::MaxLevels));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdUpgradeCost(
		TEXT("BD.Tower.UpgradeCost"),
		TEXT("BD.Tower.UpgradeCost <x> <y>: shows what the next level of the defender on that cell costs and leaves, without buying."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecUpgradeCost));

	static FAutoConsoleCommandWithWorldAndArgs CmdUpgrade(
		TEXT("BD.Tower.Upgrade"),
		TEXT("BD.Tower.Upgrade <x> <y>: buys the next level of the defender on that cell with blue votes."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecUpgradeConfirm));

	static FAutoConsoleCommandWithWorldAndArgs CmdSetAllLevels(
		TEXT("BD.Tower.SetAllLevels"),
		TEXT("BD.Tower.SetAllLevels <level>: debug, sets every defender to a level for nothing."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSetAllLevels));

	static FAutoConsoleCommandWithWorldAndArgs CmdStatus(
		TEXT("BD.Tower.Status"),
		TEXT("BD.Tower.Status: logs every tower, where it stands, its effective range, its target and its tally."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));
}

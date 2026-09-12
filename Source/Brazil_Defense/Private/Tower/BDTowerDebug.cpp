// Brazil Defense. Console view of the towers on the board.

#include "BDLog.h"
#include "Enemy/BDEnemyBase.h"
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
				? FString::Printf(TEXT("%s (%s)"), *Target->GetName(),
					Tower->GetAcquisitionRemaining() > 0.0f ? TEXT("acquiring") : (Tower->IsAligned() ? TEXT("aligned") : TEXT("turning")))
				: TEXT("none");

			UE_LOG(LogBDTower, Log,
				TEXT("  %s (%s): level %d | %s | range %.2f cells = %.0f cm | %.1f dmg @ %.2f/s | target %s | %s | %s | %d shot(s), %d kill(s)."),
				*Tower->GetName(), *GetNameSafe(Data), Tower->GetTowerLevel(), *Where,
				Tower->GetEffectiveRangeCells(), Tower->GetEffectiveRange(),
				Level != nullptr ? Level->Damage : 0.0f, Level != nullptr ? Level->FireRate : 0.0f,
				*TargetText, *Magazine, *Reload,
				Tower->GetShotsFired(), Tower->GetKills());
			++Count;
		}

		UE_LOG(LogBDTower, Log, TEXT("BD.Tower.Status: %d tower(s) on the board."), Count);
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdStatus(
		TEXT("BD.Tower.Status"),
		TEXT("BD.Tower.Status: logs every tower, where it stands, its effective range, its target and its tally."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecStatus));
}

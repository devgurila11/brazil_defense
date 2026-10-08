// Brazil Defense. Tooling: what a frame costs and what the scene holds, written to the log.

#include "BDLog.h"
#include "Components/LightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Day/BDStreetLamp.h"
#include "DynamicRHI.h"
#include "Enemy/BDCreepCorpse.h"
#include "Enemy/BDEnemyBase.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Platform/BDPlatformComponent.h"
#include "RenderTimer.h"
#include "SkeletalMeshComponentBudgeted.h"
#include "Tower/BDShooter.h"
#include "Tower/BDTowerBase.h"
#include "UObject/UObjectIterator.h"

namespace BDDebugPerf
{
	/**
	 * stat unit and stat gpu only draw on screen; a headless-launched windowed run needs the
	 * same numbers in the log. The groups are the questions asked of a crowded board: the
	 * shooters, the horde, the platforms, the lamps.
	 */
	static FString GroupOf(const UActorComponent& Component)
	{
		const AActor* Owner = Component.GetOwner();
		if (Owner == nullptr) { return TEXT("NoOwner"); }
		if (Owner->IsA<ABDShooter>()) { return TEXT("Shooter"); }
		if (Owner->IsA<ABDTowerBase>()) { return TEXT("Tower"); }
		if (Owner->IsA<ABDEnemyBase>()) { return TEXT("Horde"); }
		if (Owner->IsA<ABDCreepCorpse>()) { return TEXT("Corpse"); }
		if (Owner->IsA<ABDStreetLamp>()) { return TEXT("Lamp"); }
		if (Owner->FindComponentByClass<UBDPlatformComponent>() != nullptr) { return TEXT("Platform"); }
		return TEXT("Other");
	}

	static bool IsDrawn(const UPrimitiveComponent& Primitive)
	{
		const AActor* Owner = Primitive.GetOwner();
		return Primitive.IsRegistered() && Primitive.IsVisible() && !Primitive.bHiddenInGame && (Owner == nullptr || !Owner->IsHidden());
	}

	static void LogCensus(const UWorld& World)
	{
		struct FCount { int32 Drawn = 0; int32 OnScreen = 0; int32 Shadowed = 0; int32 Skinned = 0; int32 SkinnedBudgeted = 0; int32 SkinnedAlwaysTick = 0; };
		TMap<FString, FCount> Groups;
		for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
		{
			if (It->GetWorld() != &World || !IsDrawn(**It))
			{
				continue;
			}
			FCount& Count = Groups.FindOrAdd(GroupOf(**It));
			++Count.Drawn;
			Count.OnScreen += It->WasRecentlyRendered(0.5f) ? 1 : 0;
			Count.Shadowed += It->CastShadow && It->bCastDynamicShadow ? 1 : 0;
			if (const USkeletalMeshComponent* Skinned = Cast<USkeletalMeshComponent>(*It))
			{
				if (Skinned->GetSkeletalMeshAsset() != nullptr)
				{
					++Count.Skinned;
					Count.SkinnedBudgeted += Skinned->IsA<USkeletalMeshComponentBudgeted>() ? 1 : 0;
					Count.SkinnedAlwaysTick += Skinned->VisibilityBasedAnimTickOption == EVisibilityBasedAnimTickOption::AlwaysTickPose ? 1 : 0;
				}
			}
		}
		for (const TPair<FString, FCount>& Pair : Groups)
		{
			UE_LOG(LogBDDebug, Log, TEXT("  PERF census %-9s drawn %4d, on screen %4d, casting shadow %4d, skinned %3d (budgeted %3d, always tick %3d)"),
				*Pair.Key, Pair.Value.Drawn, Pair.Value.OnScreen, Pair.Value.Shadowed, Pair.Value.Skinned, Pair.Value.SkinnedBudgeted, Pair.Value.SkinnedAlwaysTick);
		}

		TMap<FString, FIntPoint> Lights;
		for (TObjectIterator<ULightComponent> It; It; ++It)
		{
			if (It->GetWorld() != &World || !It->IsRegistered() || !It->IsVisible() || It->Intensity <= 0.0f)
			{
				continue;
			}
			FIntPoint& Count = Lights.FindOrAdd(FString::Printf(TEXT("%s/%s"), *GroupOf(**It), *It->GetClass()->GetName()));
			++Count.X;
			Count.Y += It->CastShadows && It->CastDynamicShadows ? 1 : 0;
		}
		for (const TPair<FString, FIntPoint>& Pair : Lights)
		{
			UE_LOG(LogBDDebug, Log, TEXT("  PERF lights %-40s lit %3d, shadowed %3d"), *Pair.Key, Pair.Value.X, Pair.Value.Y);
		}

		const auto Var = [](const TCHAR* Name) -> FString
		{
			const IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name);
			return Variable != nullptr ? Variable->GetString() : TEXT("?");
		};
		FIntPoint Viewport = FIntPoint::ZeroValue;
		if (World.GetGameViewport() != nullptr && World.GetGameViewport()->Viewport != nullptr)
		{
			Viewport = World.GetGameViewport()->Viewport->GetSizeXY();
		}
		UE_LOG(LogBDDebug, Log, TEXT("  PERF render %dx%d, GI %s, reflections %s, VSM %s, Lumen HWRT %s, AA %s, screen %% %s, sg.Shadow %s, sg.GI %s, a.Budget %s"),
			Viewport.X, Viewport.Y, *Var(TEXT("r.DynamicGlobalIlluminationMethod")), *Var(TEXT("r.ReflectionMethod")), *Var(TEXT("r.Shadow.Virtual.Enable")),
			*Var(TEXT("r.Lumen.HardwareRayTracing")), *Var(TEXT("r.AntiAliasingMethod")), *Var(TEXT("r.ScreenPercentage")),
			*Var(TEXT("sg.ShadowQuality")), *Var(TEXT("sg.GlobalIlluminationQuality")), *Var(TEXT("a.Budget.Enabled")));
	}

	struct FSample
	{
		FString Label;
		int32 Left = 0;
		int32 Frames = 0;
		double Frame = 0.0, Game = 0.0, Draw = 0.0, Rhi = 0.0, Gpu = 0.0, GpuWorst = 0.0;
	};

	static void ExecSample(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr)
		{
			return;
		}
		TSharedRef<FSample> Sample = MakeShared<FSample>();
		Sample->Left = Args.Num() > 0 ? FMath::Max(1, FCString::Atoi(*Args[0])) : 240;
		Sample->Label = Args.Num() > 1 ? FString::Join(TArrayView<const FString>(Args).RightChop(1), TEXT(" ")) : TEXT("sample");
		const TWeakObjectPtr<UWorld> WeakWorld = World;

		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Sample, WeakWorld](float)
		{
			const double Gpu = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
			Sample->Frame += FApp::GetDeltaTime() * 1000.0;
			Sample->Game += FPlatformTime::ToMilliseconds(GGameThreadTime);
			Sample->Draw += FPlatformTime::ToMilliseconds(GRenderThreadTime);
			Sample->Rhi += FPlatformTime::ToMilliseconds(GRHIThreadTime);
			Sample->Gpu += Gpu;
			Sample->GpuWorst = FMath::Max(Sample->GpuWorst, Gpu);
			++Sample->Frames;
			if (--Sample->Left > 0)
			{
				return true;
			}

			const double N = FMath::Max(1, Sample->Frames);
			UE_LOG(LogBDDebug, Log, TEXT("PERF [%s] %d frames: frame %.2f ms (%.0f fps), game %.2f, draw %.2f, rhi %.2f, GPU %.2f ms (worst %.2f)"),
				*Sample->Label, Sample->Frames, Sample->Frame / N, 1000.0 / FMath::Max(0.001, Sample->Frame / N),
				Sample->Game / N, Sample->Draw / N, Sample->Rhi / N, Sample->Gpu / N, Sample->GpuWorst);
			if (const UWorld* Target = WeakWorld.Get())
			{
				LogCensus(*Target);
			}
			return false;
		}));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSample(
		TEXT("BD.Debug.PerfSample"),
		TEXT("BD.Debug.PerfSample [frames=240] [label]: averages stat unit's game, draw, RHI and GPU times over the frames and logs them with a census of what draws, what casts a shadow and what lights."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecSample));

	/** A/B for the shadow question: one group's primitives stop (or start) casting, nothing saved. */
	static void ExecCastShadow(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || Args.Num() < 2)
		{
			UE_LOG(LogBDDebug, Error, TEXT("Usage: BD.Debug.PerfCastShadow <Shooter|Horde|Platform|Lamp|Tower|Other|All> <0|1>"));
			return;
		}
		const bool bCast = FCString::Atoi(*Args[1]) != 0;
		int32 Changed = 0;
		for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->IsRegistered() && (Args[0] == TEXT("All") || GroupOf(**It) == Args[0]))
			{
				It->SetCastShadow(bCast);
				++Changed;
			}
		}
		UE_LOG(LogBDDebug, Log, TEXT("BD.Debug.PerfCastShadow: %d primitive(s) of %s now %s."), Changed, *Args[0], bCast ? TEXT("cast") : TEXT("do not cast"));
	}

	/** A/B for the geometry question: one group's actors leave the scene (or come back), nothing saved. */
	static void ExecHide(const TArray<FString>& Args, UWorld* World)
	{
		if (World == nullptr || Args.Num() < 2)
		{
			UE_LOG(LogBDDebug, Error, TEXT("Usage: BD.Debug.PerfHide <Shooter|Horde|Platform|Lamp|Tower|Other> <0|1>"));
			return;
		}
		const bool bHide = FCString::Atoi(*Args[1]) != 0;
		TSet<AActor*> Actors;
		for (TObjectIterator<UPrimitiveComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->GetOwner() != nullptr && GroupOf(**It) == Args[0])
			{
				Actors.Add(It->GetOwner());
			}
		}
		for (AActor* Actor : Actors)
		{
			Actor->SetActorHiddenInGame(bHide);
		}
		UE_LOG(LogBDDebug, Log, TEXT("BD.Debug.PerfHide: %d actor(s) of %s %s."), Actors.Num(), *Args[0], bHide ? TEXT("hidden") : TEXT("shown"));
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdHide(
		TEXT("BD.Debug.PerfHide"),
		TEXT("BD.Debug.PerfHide <Shooter|Horde|Platform|Lamp|Tower|Other> <0|1>: hides one group's actors for this session, to measure what drawing them costs."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecHide));

	static FAutoConsoleCommandWithWorldAndArgs CmdCastShadow(
		TEXT("BD.Debug.PerfCastShadow"),
		TEXT("BD.Debug.PerfCastShadow <Shooter|Horde|Platform|Lamp|Tower|Other|All> <0|1>: switches one group's shadow casting for this session, to measure what it costs."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ExecCastShadow));
}

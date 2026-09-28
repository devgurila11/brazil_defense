// Brazil Defense. What is left of a killed creep: a body falling, then sinking away.

#include "Enemy/BDCreepCorpse.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SceneComponent.h"
#include "Enemy/BDEnemyBase.h"
#include "Enemy/BDEnemyData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "SkeletalMeshComponentBudgeted.h"
#include "Wave/BDWaveSettings.h"

ABDCreepCorpse::ABDCreepCorpse()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	SetCanBeDamaged(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Nothing to hit, nothing to find: the game is done with this creep.
	Body = CreateDefaultSubobject<USkeletalMeshComponentBudgeted>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
	Body->SetAutoCalculateSignificance(true);
}

ABDCreepCorpse* ABDCreepCorpse::SpawnFrom(const ABDEnemyBase& Creep)
{
	const UBDEnemyData* Data = Creep.GetData();
	const USkeletalMeshComponentBudgeted* Source = Creep.GetSkeletalBody();
	UWorld* World = Creep.GetWorld();
	const int32 MaxCorpses = UBDWaveSettings::Get().MaxCorpses;
	if (Data == nullptr || Data->DeathAnimations.Num() == 0 || World == nullptr || MaxCorpses <= 0
		|| Source == nullptr || Source->GetSkeletalMeshAsset() == nullptr || Creep.GetBody() != Source)
	{
		return nullptr;
	}

	UAnimSequenceBase* Fall = Data->DeathAnimations[FMath::RandRange(0, Data->DeathAnimations.Num() - 1)].LoadSynchronous();
	if (Fall == nullptr)
	{
		return nullptr;
	}

	// Room for one more: the oldest body still lying there starts sinking now. A wave
	// cleared in one blow keeps the board to MaxCorpses bodies, the rest going under fast.
	int32 Lying = 0;
	ABDCreepCorpse* Oldest = nullptr;
	for (ABDCreepCorpse* Other : TActorRange<ABDCreepCorpse>(World))
	{
		if (!Other->IsSinking())
		{
			++Lying;
			if (Oldest == nullptr || Other->Age > Oldest->Age)
			{
				Oldest = Other;
			}
		}
	}
	if (Lying >= MaxCorpses && Oldest != nullptr)
	{
		Oldest->BeginSink();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABDCreepCorpse* Corpse = World->SpawnActor<ABDCreepCorpse>(ABDCreepCorpse::StaticClass(), Creep.GetActorTransform(), Params);
	if (Corpse != nullptr)
	{
		Corpse->Setup(Creep, Fall);
	}
	return Corpse;
}

void ABDCreepCorpse::Setup(const ABDEnemyBase& Creep, UAnimSequenceBase* Fall)
{
	// The body exactly as the creep wore it: mesh, the rest on the root, the scale, the
	// turn towards the route and the material of its variant.
	const USkeletalMeshComponentBudgeted* Source = Creep.GetSkeletalBody();
	Data = Creep.GetData();
	Body->SetSkeletalMesh(Source->GetSkeletalMeshAsset());
	Body->SetRelativeTransform(Source->GetRelativeTransform());
	for (int32 Slot = 0; Slot < Source->GetNumMaterials(); ++Slot)
	{
		Body->SetMaterial(Slot, Source->GetMaterial(Slot));
	}

	const UBDWaveSettings& Settings = UBDWaveSettings::Get();
	const float Rate = FMath::Max(0.1f, Settings.CorpsePlayRate);
	Body->PlayAnimation(Fall, /*bLooping*/ false);
	Body->SetPlayRate(Rate);

	RestLocation = Body->GetRelativeLocation();
	RestScale = Body->GetRelativeScale3D();
	SinkDepth = FMath::Max(50.0f, Source->Bounds.BoxExtent.Z * 2.0f);
	SinkSeconds = FMath::Max(0.05f, Settings.CorpseSinkSeconds);
	SinkStart = FMath::Min(Fall->GetPlayLength() / Rate + Settings.CorpseHoldSeconds, Settings.CorpseMaxSeconds);
	Age = 0.0f;
}

void ABDCreepCorpse::BeginSink()
{
	SinkStart = FMath::Min(SinkStart, Age);
}

void ABDCreepCorpse::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;
	if (Age < SinkStart)
	{
		return;
	}

	// Down through the floor, shrinking a little as it goes so the last of it does not
	// hang at the surface. Eased in, so it reads as settling rather than as a lift going down.
	const float Alpha = FMath::Clamp((Age - SinkStart) / SinkSeconds, 0.0f, 1.0f);
	if (Alpha >= 1.0f)
	{
		Destroy();
		return;
	}
	const float Eased = Alpha * Alpha;
	Body->SetRelativeLocation(RestLocation - FVector(0.0f, 0.0f, SinkDepth * Eased));
	Body->SetRelativeScale3D(RestScale * (1.0f - 0.35f * Eased));
}

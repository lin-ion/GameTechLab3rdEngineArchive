#include "Component/Gameplay/BeamAttackComponent.h"

#include "Component/Gameplay/BulletHellDamageReceiverComponent.h"
#include "Component/Particle/ParticleSystemComponent.h"
#include "Component/SceneComponent.h"
#include "Core/Logging/Log.h"
#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/Vector.h"
#include "Particle/ParticleSystemManager.h"
#include "Render/Types/MinimalViewInfo.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float RadToDeg = 57.2957795131f;
	constexpr const char* BossTagName = "Boss";

	// Beam(Distance 방식)은 로컬 +X 로 뻗으므로, 빔이 Dir(카메라 시선) 방향으로 진행하도록
	// 컴포넌트 world rotation 을 forward(+X)==Dir 로 맞춘다.
	// FRotator::GetForwardVector 규약: Forward=(cosP*cosY, cosP*sinY, -sinP) → Yaw=atan2(Y,X), Pitch=-asin(Z).
	FRotator RotatorFromDirection(const FVector& Dir)
	{
		const FVector D = (Dir.Length() > 1e-6f) ? Dir.Normalized() : FVector::ForwardVector;
		FRotator Result;
		Result.Yaw = std::atan2(D.Y, D.X) * RadToDeg;
		Result.Pitch = -std::asin(std::clamp(D.Z, -1.0f, 1.0f)) * RadToDeg;
		Result.Roll = 0.0f;
		return Result;
	}
}

UBeamAttackComponent::UBeamAttackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEnabled = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBeamAttackComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
}

bool UBeamAttackComponent::ComputeAim(FVector& OutOrigin, FVector& OutDirection) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	FVector Direction = FVector::ForwardVector;
	bool bResolved = false;

	if (bUseCameraAim)
	{
		if (UWorld* World = GetWorld())
		{
			FMinimalViewInfo POV;
			if (World->GetActivePOV(POV))
			{
				Direction = POV.Rotation.GetForwardVector();
				bResolved = true;
			}
		}
	}

	if (!bResolved)
	{
		// 카메라 미사용 또는 POV 없음 → 캐릭터 forward.
		Direction = Owner->GetActorRotation().GetForwardVector();
	}

	Direction = (Direction.Length() > 1e-6f) ? Direction.Normalized() : FVector::ForwardVector;

	const FVector OwnerLocation = Owner->GetActorLocation();
	OutDirection = Direction;
	// actor 위치에서 camera 방향으로 SpawnForwardOffset(=1 transform) 만큼 앞. 회전 정렬은 하지 않는다.
	OutOrigin = OwnerLocation + Direction * SpawnForwardOffset;
	return true;
}

bool UBeamAttackComponent::IsBossActor(const AActor* Candidate) const
{
	if (!Candidate)
	{
		return false;
	}
	if (Candidate->HasTag(FName(BossTagName)))
	{
		return true;
	}
	const FString Name = Candidate->GetName();
	return Name.find("Boss") != FString::npos || Name.find("boss") != FString::npos;
}

UParticleSystemComponent* UBeamAttackComponent::SpawnBeamComponent(const FVector& Origin, const FVector& Direction)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	UParticleSystemComponent* Beam = Owner->AddComponent<UParticleSystemComponent>();
	if (!Beam)
	{
		return nullptr;
	}

	Beam->SetFName(FName("PlayerBeamEmitter"));
	if (USceneComponent* Root = Owner->GetRootComponent())
	{
		Beam->AttachToComponent(Root);
	}
	if (UParticleSystem* Template = FParticleSystemManager::Get().Load(BeamTemplatePath))
	{
		Beam->SetTemplate(Template);
	}
	// camera 시선 방향 1 transform 앞에 배치하고, 빔이 그 방향으로 진행하도록 정렬한다.
	Beam->SetWorldLocation(Origin);
	Beam->SetWorldRotation(RotatorFromDirection(Direction));
	// BeamScale 로 빔 비주얼(굵기=max(scale.X,Y), 길이=scale.X) 키우기 — asset 무수정, per-instance.
	Beam->SetRelativeScale(FVector(BeamScale, BeamScale, BeamScale));
	Beam->Activate(true);
	return Beam;
}

void UBeamAttackComponent::DestroyBeamComponent()
{
	if (UParticleSystemComponent* Beam = BeamComponent.Get())
	{
		Beam->Deactivate();
		if (AActor* Owner = GetOwner())
		{
			Owner->RemoveComponent(Beam);
		}
	}
	BeamComponent = nullptr;
}

void UBeamAttackComponent::FireBeam()
{
	// 재시전 시 기존 빔 정리.
	if (bBeamActive)
	{
		DestroyBeamComponent();
	}

	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	if (!ComputeAim(Origin, Direction))
	{
		return;
	}

	BeamOrigin = Origin;
	BeamDirection = Direction;
	BeamComponent = SpawnBeamComponent(Origin, Direction);
	bBeamActive = true;
	BeamAge = 0.0f;
	DamageAccumulator = 0.0f;

	UE_LOG("[BeamAttack] FireBeam owner=%s origin=(%.2f,%.2f,%.2f) dir=(%.2f,%.2f,%.2f) range=%.2f dur=%.2f",
		GetOwner() ? GetOwner()->GetName().c_str() : "nil",
		Origin.X, Origin.Y, Origin.Z,
		Direction.X, Direction.Y, Direction.Z,
		BeamRange, BeamDuration);
}

void UBeamAttackComponent::EndBeam()
{
	DestroyBeamComponent();
	bBeamActive = false;
	BeamAge = 0.0f;
	DamageAccumulator = 0.0f;
}

void UBeamAttackComponent::SetBeamScale(float InScale)
{
	BeamScale = InScale > 0.0f ? InScale : 0.0f;
	// 시전 중이면 살아있는 빔 비주얼에도 즉시 반영 (충돌은 매 tick BeamScale 을 다시 읽음).
	if (UParticleSystemComponent* Beam = BeamComponent.Get())
	{
		Beam->SetRelativeScale(FVector(BeamScale, BeamScale, BeamScale));
	}
	UE_LOG("[BeamAttack] SetBeamScale owner=%s scale=%.3f",
		GetOwner() ? GetOwner()->GetName().c_str() : "nil", BeamScale);
}

void UBeamAttackComponent::SetBeamDuration(float InDuration)
{
	// UPROPERTY 범위(Min=0, Max=60)에 맞춰 clamp. BeamDuration 은 매 tick 비교에 쓰여 즉시 반영된다.
	BeamDuration = std::clamp(InDuration, 0.0f, 60.0f);
	UE_LOG("[BeamAttack] SetBeamDuration owner=%s duration=%.3f",
		GetOwner() ? GetOwner()->GetName().c_str() : "nil", BeamDuration);
}

void UBeamAttackComponent::ApplyBeamDamageAlong(const FVector& Origin, const FVector& Direction)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 충돌 길이·두께도 BeamScale 로 비주얼과 함께 키운다.
	const float EffectiveRange = BeamRange * BeamScale;
	const float EffectiveRadius = BeamRadius * BeamScale;
	const FVector End = Origin + Direction * EffectiveRange;
	FHitResult Hit;
	const uint32 Mask =
		ObjectTypeBit(ECollisionChannel::WorldStatic)
		| ObjectTypeBit(ECollisionChannel::WorldDynamic)
		| ObjectTypeBit(ECollisionChannel::Pawn)
		| ObjectTypeBit(ECollisionChannel::Trigger);
	const bool bHit = World->PhysicsSweepByObjectTypes(
		Origin,
		End,
		FQuat::Identity,
		FCollisionShape::MakeSphere(EffectiveRadius),
		Hit,
		Mask,
		GetOwner());

	if (bDrawDebug)
	{
		DrawDebugLine(World, Origin, bHit ? Hit.WorldHitLocation : End,
			bHit && IsBossActor(Hit.HitActor) ? FColor::Green() : FColor::Red(), 0.05f);
	}

	if (!bHit)
	{
		return;
	}

	AActor* Target = Hit.HitActor;
	// 충돌 판정 결과에 따라 보스에게만 대미지 — 보스가 아니면 아무것도 하지 않는다.
	if (!Target || Target == GetOwner() || !IsBossActor(Target) || DamagePerTick <= 0.0f)
	{
		return;
	}

	if (UBulletHellDamageReceiverComponent* DamageReceiver = Target->GetComponentByClass<UBulletHellDamageReceiverComponent>())
	{
		DamageReceiver->ApplyDamage(DamagePerTick);
	}
}

void UBeamAttackComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	(void)TickType;
	(void)ThisTickFunction;

	if (!bBeamActive)
	{
		return;
	}

	BeamAge += DeltaTime;
	DamageAccumulator += DeltaTime;

	// 일직선 고정(보스 추적 안 함): 시전 시작 시 캡처한 BeamOrigin/BeamDirection 으로 판정.
	if (DamageTickInterval > 0.0f)
	{
		while (DamageAccumulator >= DamageTickInterval)
		{
			DamageAccumulator -= DamageTickInterval;
			ApplyBeamDamageAlong(BeamOrigin, BeamDirection);
		}
	}
	else
	{
		ApplyBeamDamageAlong(BeamOrigin, BeamDirection);
	}

	if (BeamDuration > 0.0f && BeamAge >= BeamDuration)
	{
		EndBeam();
	}
}

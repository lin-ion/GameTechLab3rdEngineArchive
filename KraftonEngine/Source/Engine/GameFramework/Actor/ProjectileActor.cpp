#include "pch.h"
#include "ProjectileActor.h"
#include "Engine/Runtime/Engine.h"

#include "Engine/Component/Primitive/StaticMeshComponent.h"
#include "Engine/Component/Particle/ParticleSystemComponent.h"
#include "Particle/ParticleSystem.h"
#include "Particle/ParticleEmitter.h"
#include "Particle/ParticleLODLevel.h"
#include "Particle/ParticleModule.h"
#include "Particle/Modules/ParticleModuleRequired.h"
#include "Particle/Modules/ParticleModuleSpawn.h"
#include "Particle/Modules/ParticleModuleLifetime.h"
#include "Particle/Modules/ParticleModuleLocation.h"
#include "Particle/Modules/ParticleModuleVelocity.h"
#include "Particle/TypeData/ParticleModuleTypeDataRibbon.h"
#include "Particle/Distributions/DistributionFloatConstant.h"
#include "Particle/Distributions/DistributionVectorUniform.h"
#include "Core/Types/CollisionTypes.h"

// ---------------------------------------------------------------------------
// Ribbon(궤적) emitter 코드 빌드 — 자산 직렬화/에디터 완성 전까지의 우회용.
// ParticleSystemActor 의 ConfigureMeshEmitter 패턴을 Ribbon 으로 옮긴 것.
// (모듈 파라미터를 코드에서 직접 박음 → 변경 시 재빌드 필요.)
// ---------------------------------------------------------------------------
namespace
{
	UDistributionFloatConstant* SetFloatConstant(UDistributionFloat*& Distribution, UObject* Outer, float Value)
	{
		if (Distribution)
		{
			UObjectManager::Get().DestroyObject(Distribution);
			Distribution = nullptr;
		}

		auto* NewDistribution = UObjectManager::Get().CreateObject<UDistributionFloatConstant>(Outer);
		if (NewDistribution)
		{
			NewDistribution->Constant = Value;
			Distribution = NewDistribution;
		}
		return NewDistribution;
	}

	UDistributionVectorUniform* SetVectorUniform(UDistributionVector*& Distribution, UObject* Outer, const FVector& Min, const FVector& Max)
	{
		if (Distribution)
		{
			UObjectManager::Get().DestroyObject(Distribution);
			Distribution = nullptr;
		}

		auto* NewDistribution = UObjectManager::Get().CreateObject<UDistributionVectorUniform>(Outer);
		if (NewDistribution)
		{
			NewDistribution->Min = Min;
			NewDistribution->Max = Max;
			Distribution = NewDistribution;
		}
		return NewDistribution;
	}

	// 시간 순으로 sample 된 입자를 이어 quad-strip 으로 렌더하는 Ribbon emitter 구성.
	// AddEmitter() 가 LOD0 + Required/Spawn + Lifetime/Location/Velocity/Color/Size 를
	// 이미 만들어 두므로, 여기서는 (a) Ribbon TypeData 부착 + (b) 파라미터만 덮어쓴다.
	void ConfigureRibbonEmitter(UParticleEmitter* Emitter)
	{
		if (!Emitter) return;
		Emitter->EmitterName = "Projectile Trail";

		UParticleLODLevel* LOD = Emitter->GetLODLevel(0);
		if (!LOD) return;

		// (1) TypeData = Ribbon — CreateInstance() 에서 FParticleRibbonEmitterInstance 생성
		if (!LOD->TypeDataModule)
		{
			auto* TypeData = UObjectManager::Get().CreateObject<UParticleModuleTypeDataRibbon>(LOD);
			if (TypeData)
			{
				TypeData->MaxTessellation = 8;
				TypeData->TangentTension  = 0.5f;
				TypeData->TilesPerTrail   = 1.0f;
				LOD->TypeDataModule = TypeData;
			}
		}

		// (2) Required: Ribbon/Beam 셰이더(BeamTrail.hlsl) 호환 머티리얼 + 월드 공간.
		//     bUseLocalSpace=false 라야 발사체가 날아간 자취가 월드에 남는다.
		if (UParticleModuleRequired* Required = LOD->RequiredModule)
		{
			Required->MaterialSlot   = "Content/Material/Particle/Ribbon1.uasset";
			Required->bUseLocalSpace = false;
		}

		// (3) Spawn rate — ribbon 의 sample 밀도(부드러움)를 좌우.
		if (UParticleModuleSpawn* Spawn = LOD->SpawnModule)
		{
			SetFloatConstant(Spawn->RateDistribution, Spawn, 60.0f);
		}

		// (4) AddEmitter() 가 만들어 둔 일반 모듈 파라미터만 갱신.
		for (UParticleModule* M : LOD->Modules)
		{
			if (auto* Lifetime = Cast<UParticleModuleLifetime>(M))
			{
				// Lifetime = 트레일이 유지되는 시간 ≒ 꼬리 길이.
				SetFloatConstant(Lifetime->LifetimeDistribution, Lifetime, 0.5f);
			}
			else if (auto* Loc = Cast<UParticleModuleLocation>(M))
			{
				// 점 소스(spread 0) — 궤적이 발사체 중심을 따라 한 줄로 이어지도록.
				SetVectorUniform(Loc->StartLocationDistribution, Loc, FVector::ZeroVector, FVector::ZeroVector);
			}
			else if (auto* Vel = Cast<UParticleModuleVelocity>(M))
			{
				// 트레일은 emitter 이동을 따라가므로 입자 자체 속도는 0.
				SetVectorUniform(Vel->StartVelocityDistribution, Vel, FVector::ZeroVector, FVector::ZeroVector);
			}
		}
	}
}

void AProjectileActor::BeginPlay()
{
	Super::BeginPlay();
}

// 	SpringArm->AttachToComponent(CapsuleComponent);
// root 가 capsule component
void AProjectileActor::InitDefaultComponents()
{
	StaticMeshComponent = AddComponent<UStaticMeshComponent>();
	ParticleSystemComponent = AddComponent<UParticleSystemComponent>();

	SetRootComponent(StaticMeshComponent);
	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	UStaticMesh* MeshAsset = FMeshManager::LoadStaticMesh("Content/Data/BasicShape/Cube.OBJ" , Device);
	StaticMeshComponent->SetStaticMesh(MeshAsset);
	ParticleSystemComponent->AttachToComponent(StaticMeshComponent);


	UParticleSystem* PS = UObjectManager::Get().CreateObject<UParticleSystem>();
	UParticleEmitter* RibbonEmitter = PS->AddEmitter();   // 코어/일반 모듈 자동 생성
	ConfigureRibbonEmitter(RibbonEmitter);                // Ribbon TypeData + 파라미터 세팅
	PS->BuildEmitters();                                  // payload layout 캐시 (안전망)

	ParticleSystemComponent->SetTemplate(PS);
	ParticleSystemComponent->Activate(false);

}

void AProjectileActor::PostDuplicate()
{
	Super::PostDuplicate();
	StaticMeshComponent = GetComponentByClass<UStaticMeshComponent>();
	ParticleSystemComponent = GetComponentByClass<UParticleSystemComponent>();
}
// ── IPoolableProjectile ─────────────────────────────────────────────────────

void AProjectileActor::OnPoolConstruct()
{
	if (bPoolConstructed) return;     // 풀 최초 편입 시 1회만
	bPoolConstructed = true;
	InitDefaultComponents();          // 무거운 컴포넌트 구성 + 렌더 상태 생성
}

void AProjectileActor::Activate(const FVector& Location, const FVector& Velocity)
{
	SetActorLocation(Location);
	SetVisible(true);                 // 렌더 가시성 ON (액터→컴포넌트 전파)

	CachedVelocity = Velocity;
	LifeTimeRemaining = DefaultLifeTime;

	if (UStaticMeshComponent* Mesh = StaticMeshComponent.Get())
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);   // Hit/Overlap 이벤트만
		// 물리 시뮬레이션 기반 발사체라면 위 대신:
		// Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		// Mesh->SetSimulatePhysics(true);
		// Mesh->SetLinearVelocity(Velocity);
	}
	if (UParticleSystemComponent* PSC = ParticleSystemComponent.Get())
	{
		PSC->Activate(true);          // 파티클 리셋 후 재생
	}
	bNeedsTick = true;                // 이동/수명 갱신 Tick ON
}

void AProjectileActor::Deactivate()
{
	bNeedsTick = false;

	if (UStaticMeshComponent* Mesh = StaticMeshComponent.Get())
	{
		// Mesh->SetLinearVelocity(FVector::ZeroVector);   // 물리 시뮬레이션 사용 시
		// Mesh->SetSimulatePhysics(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // 물리/쿼리 등록 해제
	}
	if (UParticleSystemComponent* PSC = ParticleSystemComponent.Get())
	{
		PSC->Deactivate();
	}
	SetVisible(false);                // 렌더 가시성 OFF — 단, Destroy 는 하지 않음
}

void AProjectileActor::ResetState()
{
	CachedVelocity = FVector::ZeroVector;
	LifeTimeRemaining = 0.0f;
	// TODO(game): 소유자/관통 카운트/궤적/타이머 등 게임 측 잔여 상태 초기화
}
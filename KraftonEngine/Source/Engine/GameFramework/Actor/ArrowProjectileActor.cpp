#include "pch.h"
#include "ArrowProjectileActor.h"

#include "Core/Types/CollisionTypes.h"
#include "Engine/Component/Particle/ParticleSystemComponent.h"
#include "Engine/Component/Primitive/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/ProjectilePoolSubSystem.h"
#include "GameFramework/World.h"
#include "Math/Quat.h"
#include "Particle/ParticleSystemManager.h"

namespace
{
	FRotator MakeArrowRotationFromVelocity(const FVector& Velocity)
	{
		if (Velocity.Length() <= 1.e-6f)
		{
			return FRotator(FVector(90.0f, 0.0f, 90.0f));
		}

		FVector Direction = Velocity;
		Direction.Normalize();

		constexpr float RadToDeg = 180.0f / 3.1415926535f;
		const float HorizontalLength = std::sqrt(Direction.X * Direction.X + Direction.Y * Direction.Y);
		const float Pitch = std::atan2(-Direction.Z, HorizontalLength) * RadToDeg;
		const float Yaw = std::atan2(Direction.Y, Direction.X) * RadToDeg;

		const FRotator DirectionRotation(Pitch, Yaw, 0.0f);
		const FRotator MeshAxisOffset(FVector(90.0f, 0.0f, 90.0f));
		return (DirectionRotation.ToQuaternion() * MeshAxisOffset.ToQuaternion()).ToRotator();
	}

	constexpr const char* AimParticlePath = "Content/Particle System/Aim.uasset";
	constexpr const char* FireArrowParticlePath = "Content/Particle System/FireArrow.uasset";
}

void AArrowProjectileActor::BeginPlay()
{
	Super::BeginPlay();
}

void AArrowProjectileActor::InitArrowComponents()
{
	StaticMeshComponent = AddComponent<UStaticMeshComponent>();
	SetRootComponent(StaticMeshComponent);

	ID3D11Device* Device = GEngine->GetRenderer().GetFD3DDevice().GetDevice();
	UStaticMesh* MeshAsset = FMeshManager::LoadStaticMesh("Content/Data/Arrow/Arrow_StaticMesh.uasset", Device);
	StaticMeshComponent->SetStaticMesh(MeshAsset);
	EnsureArrowParticleComponent(AimParticleComponent, "ArrowAimParticle", AimParticlePath);
	EnsureArrowParticleComponent(FireArrowParticleComponent, "ArrowFireParticle", FireArrowParticlePath);
	SetAimParticleActive(false);
	SetFireArrowParticleActive(false);
}

UParticleSystemComponent* AArrowProjectileActor::EnsureArrowParticleComponent(
	TWeakObjectPtr<UParticleSystemComponent>& Component,
	const char* ComponentName,
	const char* TemplatePath)
{
	if (UParticleSystemComponent* Existing = Component.Get())
	{
		return Existing;
	}

	for (UActorComponent* ActorComponent : GetComponents())
	{
		UParticleSystemComponent* ParticleComponent = Cast<UParticleSystemComponent>(ActorComponent);
		if (ParticleComponent && ParticleComponent->GetFName() == FName(ComponentName))
		{
			Component = ParticleComponent;
			return ParticleComponent;
		}
	}

	UParticleSystemComponent* ParticleComponent = AddComponent<UParticleSystemComponent>();
	if (!ParticleComponent)
	{
		UE_LOG("[ArrowProjectileParticle] failed to create component=%s", ComponentName);
		return nullptr;
	}

	ParticleComponent->SetFName(FName(ComponentName));
	ParticleComponent->SetAutoActivate(false);
	ParticleComponent->SetResetOnActivate(true);
	if (UStaticMeshComponent* Mesh = StaticMeshComponent.Get())
	{
		ParticleComponent->AttachToComponent(Mesh);
	}

	UParticleSystem* ParticleSystem = FParticleSystemManager::Get().Load(TemplatePath);
	if (ParticleSystem)
	{
		ParticleComponent->SetTemplate(ParticleSystem);
		UE_LOG("[ArrowProjectileParticle] loaded component=%s path=%s", ComponentName, TemplatePath);
	}
	else
	{
		UE_LOG("[ArrowProjectileParticle] failed to load template component=%s path=%s", ComponentName, TemplatePath);
	}

	Component = ParticleComponent;
	return ParticleComponent;
}

void AArrowProjectileActor::SetAimParticleActive(bool bActive)
{
	// Aim uses a vector-field particle. In this engine it renders reliably as a
	// world-spawned ParticleSystemActor/PSC, but not as a child PSC on the pooled
	// arrow mesh. HaruController owns the world-space Aim effect while the arrow is held.
	if (bActive)
	{
		return;
	}

	UParticleSystemComponent* AimParticle = EnsureArrowParticleComponent(AimParticleComponent, "ArrowAimParticle", AimParticlePath);
	if (!AimParticle)
	{
		return;
	}

	if (bActive)
	{
		constexpr int32 AimRefreshIntervalFrames = 6;
		const bool bShouldRefresh = !bAimParticleRequested
			|| !AimParticle->IsActive()
			|| AimParticleRefreshCounter >= AimRefreshIntervalFrames;
		if (bShouldRefresh)
		{
			AimParticle->Deactivate();
			AimParticle->Activate(true);
			const FVector L = GetActorLocation();
			UE_LOG("[ArrowProjectileParticle] Aim active actor=%s loc=(%.3f,%.3f,%.3f) refresh=%d",
				GetName().c_str(), L.X, L.Y, L.Z, AimParticleRefreshCounter);
			AimParticleRefreshCounter = 0;
		}
		else
		{
			++AimParticleRefreshCounter;
		}
		bAimParticleRequested = true;
	}
	else
	{
		if (bAimParticleRequested || AimParticle->IsActive())
		{
			UE_LOG("[ArrowProjectileParticle] Aim inactive actor=%s", GetName().c_str());
		}
		AimParticleRefreshCounter = 0;
		bAimParticleRequested = false;
		AimParticle->Deactivate();
	}
}

void AArrowProjectileActor::SetFireArrowParticleActive(bool bActive)
{
	UParticleSystemComponent* FireParticle = EnsureArrowParticleComponent(FireArrowParticleComponent, "ArrowFireParticle", FireArrowParticlePath);
	if (!FireParticle)
	{
		return;
	}

	if (bActive)
	{
		if (!bFireArrowParticleRequested || !FireParticle->IsActive())
		{
			FireParticle->Activate(true);
			const FVector L = GetActorLocation();
			UE_LOG("[ArrowProjectileParticle] FireArrow active actor=%s loc=(%.3f,%.3f,%.3f)",
				GetName().c_str(), L.X, L.Y, L.Z);
		}
		bFireArrowParticleRequested = true;
	}
	else
	{
		bFireArrowParticleRequested = false;
		FireParticle->Deactivate();
	}
}

void AArrowProjectileActor::OnPoolConstruct()
{
	if (bPoolConstructed) return;
	bPoolConstructed = true;
	InitArrowComponents();

	UE_LOG("[ArrowProjectileConstruct] HasActorBegunPlay=%d", (int)HasActorBegunPlay());
	if (HasActorBegunPlay())
	{
		if (UStaticMeshComponent* Mesh = StaticMeshComponent.Get())
		{
			Mesh->BeginPlay();
		}
		if (UParticleSystemComponent* AimParticle = AimParticleComponent.Get())
		{
			AimParticle->BeginPlay();
		}
		if (UParticleSystemComponent* FireParticle = FireArrowParticleComponent.Get())
		{
			FireParticle->BeginPlay();
		}
	}
}

void AArrowProjectileActor::Activate(const FVector& Location, const FVector& Velocity)
{
	SetActorLocation(Location);
	if (Velocity.Length() <= 1.e-6f)
	{
		HoldAt(Location, FVector::ForwardVector);
		return;
	}

	Launch(Velocity);
}

void AArrowProjectileActor::HoldAt(const FVector& Location, const FVector& AimDirection)
{
	bHeld = true;
	bNeedsTick = false;
	CachedVelocity = FVector::ZeroVector;
	LifeTimeRemaining = DefaultLifeTime;
	VelocityWarmupRemaining = 0.0f;

	SetActorLocation(Location);
	SetActorRotation(MakeArrowRotationFromVelocity(AimDirection));
	SetVisible(true);
	SetFireArrowParticleActive(false);
	SetAimParticleActive(true);

	if (UStaticMeshComponent* Mesh = StaticMeshComponent.Get())
	{
		Mesh->SetLinearVelocity(FVector::ZeroVector);
		Mesh->SetAngularVelocity(FVector::ZeroVector);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetSimulatePhysics(false);
		Mesh->SetEnableGravity(false);
	}
}

void AArrowProjectileActor::Launch(const FVector& Velocity)
{
	bHeld = false;
	CachedVelocity = Velocity;
	LifeTimeRemaining = DefaultLifeTime;
	VelocityWarmupRemaining = 0.12f;
	SetActorRotation(MakeArrowRotationFromVelocity(Velocity));
	SetVisible(true);
	SetAimParticleActive(false);
	SetFireArrowParticleActive(true);

	if (UStaticMeshComponent* Mesh = StaticMeshComponent.Get())
	{
		Mesh->SetSimulatePhysics(true);
		Mesh->SetEnableGravity(true);
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetLinearVelocity(Velocity);
		Mesh->SetAngularVelocity(FVector::ZeroVector);

		const FVector ReadV = Mesh->GetLinearVelocity();
		UE_LOG("[ArrowProjectileActivate] sim=%d gravity=1 coll=%d inVel=(%.3f,%.3f,%.3f) readVel=(%.3f,%.3f,%.3f)",
			(int)Mesh->GetSimulatePhysics(), (int)Mesh->IsCollisionEnabled(),
			Velocity.X, Velocity.Y, Velocity.Z, ReadV.X, ReadV.Y, ReadV.Z);
	}
	else
	{
		UE_LOG("[ArrowProjectileActivate] StaticMeshComponent == NULL");
	}

	bNeedsTick = true;
}

void AArrowProjectileActor::Deactivate()
{
	bNeedsTick = false;
	bHeld = false;

	if (UStaticMeshComponent* Mesh = StaticMeshComponent.Get())
	{
		Mesh->SetLinearVelocity(FVector::ZeroVector);
		Mesh->SetAngularVelocity(FVector::ZeroVector);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetSimulatePhysics(false);
		Mesh->SetEnableGravity(false);
	}
	SetAimParticleActive(false);
	SetFireArrowParticleActive(false);

	SetVisible(false);
}

void AArrowProjectileActor::ResetState()
{
	bHeld = false;
	bAimParticleRequested = false;
	bFireArrowParticleRequested = false;
	AimParticleRefreshCounter = 0;
	CachedVelocity = FVector::ZeroVector;
	LifeTimeRemaining = 0.0f;
	VelocityWarmupRemaining = 0.0f;
	SetAimParticleActive(false);
	SetFireArrowParticleActive(false);
}

void AArrowProjectileActor::Tick(float DeltaTime)
{
	AActor::Tick(DeltaTime);
	if (!bNeedsTick) return;

	if (UStaticMeshComponent* Mesh = StaticMeshComponent.Get())
	{
		// PhysX body creation can lag one frame after activation. Reapply the launch
		// velocity briefly, then let gravity own the trajectory.
		if (VelocityWarmupRemaining > 0.0f)
		{
			Mesh->SetLinearVelocity(CachedVelocity);
			VelocityWarmupRemaining -= DeltaTime;
		}

		const FVector L = GetActorLocation();
		const FVector V = Mesh->GetLinearVelocity();
		SetActorRotation(MakeArrowRotationFromVelocity(V));
	//	UE_LOG("[ArrowProjectileTick] life=%.2f loc=(%.3f,%.3f,%.3f) vel=(%.3f,%.3f,%.3f)",
	//		LifeTimeRemaining, L.X, L.Y, L.Z, V.X, V.Y, V.Z);
	//
	}

	LifeTimeRemaining -= DeltaTime;
	if (LifeTimeRemaining <= 0.0f)
	{
		if (UWorld* W = GetWorld())
		{
			if (FProjectilePoolSubsystem* Pool = W->GetProjectilePool())
			{
				Pool->Release(this);
				return;
			}
		}
		Deactivate();
	}
}

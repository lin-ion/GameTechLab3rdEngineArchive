#include "pch.h"
#include "ArrowProjectileActor.h"

#include "Core/Types/CollisionTypes.h"
#include "Engine/Component/Primitive/StaticMeshComponent.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/ProjectilePoolSubSystem.h"
#include "GameFramework/World.h"
#include "Math/Quat.h"

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

	SetVisible(false);
}

void AArrowProjectileActor::ResetState()
{
	bHeld = false;
	CachedVelocity = FVector::ZeroVector;
	LifeTimeRemaining = 0.0f;
	VelocityWarmupRemaining = 0.0f;
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
		UE_LOG("[ArrowProjectileTick] life=%.2f loc=(%.3f,%.3f,%.3f) vel=(%.3f,%.3f,%.3f)",
			LifeTimeRemaining, L.X, L.Y, L.Z, V.X, V.Y, V.Z);
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

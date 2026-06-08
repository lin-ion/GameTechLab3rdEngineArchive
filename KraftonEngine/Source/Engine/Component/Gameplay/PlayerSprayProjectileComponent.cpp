#include "Component/Gameplay/PlayerSprayProjectileComponent.h"

#include "Component/Gameplay/BulletHellDamageReceiverComponent.h"
#include "Component/Gameplay/BulletTrailComponent.h"
#include "Component/Primitive/InstancedStaticMeshComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Core/Types/RayTypes.h"
#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Pawn/BossCharacter.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Math/Rotator.h"
#include "Render/Types/MinimalViewInfo.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	constexpr float Pi = 3.1415926535f;
	constexpr const char* PlayerTagName = "Player";
	constexpr const char* BossTagName = "Boss";

	float ClampFloat(float Value, float MinValue, float MaxValue)
	{
		return (std::max)(MinValue, (std::min)(MaxValue, Value));
	}

	FVector SafeDirection(const FVector& Direction, const FVector& Fallback)
	{
		return Direction.IsNearlyZero() ? Fallback : Direction.Normalized();
	}

	AActor* ResolveHitActor(const FHitResult& Hit)
	{
		if (Hit.HitActor)
		{
			return Hit.HitActor;
		}
		return Hit.HitComponent ? Hit.HitComponent->GetOwner() : nullptr;
	}

	bool HasActorTag(const AActor* Actor, const char* TagName)
	{
		return Actor && TagName && Actor->HasTag(FName(TagName));
	}

	void MakeBasis(const FVector& Forward, FVector& OutRight, FVector& OutUp)
	{
		const FVector F = SafeDirection(Forward, FVector::ForwardVector);
		const FVector Reference = std::fabs(F.Z) < 0.95f ? FVector::UpVector : FVector::RightVector;
		OutRight = SafeDirection(Reference.Cross(F), FVector::RightVector);
		OutUp = SafeDirection(F.Cross(OutRight), FVector::UpVector);
	}
}

UPlayerSprayProjectileComponent::UPlayerSprayProjectileComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEnabled = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UPlayerSprayProjectileComponent::BeginPlay()
{
	UActorComponent::BeginPlay();
	EnsureRenderComponent();
	EnsureTrailComponent();
}

void UPlayerSprayProjectileComponent::StartAttack()
{
	bAttackHeld = true;
	FireAccumulator = 0.0f;
	TryFireBurst();
	UE_LOG("[PlayerSpray] StartAttack owner=%s comp=%s fireRate=%.3f perBurst=%d aimDist=%.3f spawnOffset=(%.3f,%.3f) cone=%.3f radius=%.3f renderScale=%.3f mesh=%s material=%s",
		GetOwner() ? GetOwner()->GetName().c_str() : "nil",
		GetName().c_str(),
		FireRate,
		ProjectilesPerBurst,
		AimRayDistance,
		SpawnForwardOffset,
		SpawnUpOffset,
		ConeHalfAngleDegrees,
		ProjectileRadius,
		RenderScale,
		MeshPath.c_str(),
		MaterialPath.c_str());
}

void UPlayerSprayProjectileComponent::StopAttack()
{
	bAttackHeld = false;
	FireAccumulator = 0.0f;
	UE_LOG("[PlayerSpray] StopAttack activeProjectiles=%d", static_cast<int32>(Projectiles.size()));
}

void UPlayerSprayProjectileComponent::ClearProjectiles()
{
	Projectiles.clear();
	ClearRender();
	SyncTrail();
}

void UPlayerSprayProjectileComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	(void)TickType;
	(void)ThisTickFunction;

	TickAttack(DeltaTime);
	TickProjectiles(DeltaTime);
	SyncRender();
	SyncTrail();
}

void UPlayerSprayProjectileComponent::TickAttack(float DeltaTime)
{
	if (!bAttackHeld || DeltaTime <= 0.0f)
	{
		return;
	}

	const float Interval = FireRate > 0.0f ? 1.0f / FireRate : 0.0f;
	if (Interval <= 0.0f)
	{
		TryFireBurst();
		return;
	}

	FireAccumulator += DeltaTime;
	while (FireAccumulator >= Interval)
	{
		FireAccumulator -= Interval;
		TryFireBurst();
	}
}

void UPlayerSprayProjectileComponent::TryFireBurst()
{
	FVector CameraLocation;
	FVector CameraForward;
	AActor* BossActor = nullptr;
	if (!FindCameraBossTarget(CameraLocation, CameraForward, BossActor))
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	const FVector OwnerLocation = OwnerActor ? OwnerActor->GetActorLocation() : CameraLocation;
	const FVector SpawnOrigin = OwnerLocation
		+ SafeDirection(CameraForward, FVector::ForwardVector) * SpawnForwardOffset
		+ FVector::UpVector * SpawnUpOffset;
	const int32 Count = (std::max)(1, ProjectilesPerBurst);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		SpawnProjectile(SpawnOrigin, CameraForward, BossActor, Index, Count);
	}

	UE_LOG("[PlayerSpray] Burst target=%s count=%d active=%d",
		BossActor ? BossActor->GetName().c_str() : "nil",
		Count,
		static_cast<int32>(Projectiles.size()));
}

bool UPlayerSprayProjectileComponent::FindCameraBossTarget(
	FVector& OutCameraLocation,
	FVector& OutCameraForward,
	AActor*& OutBossActor)
{
	OutCameraLocation = FVector::ZeroVector;
	OutCameraForward = FVector::ForwardVector;
	OutBossActor = nullptr;

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FMinimalViewInfo POV;
	if (!World->GetActivePOV(POV))
	{
		return false;
	}

	OutCameraLocation = POV.Location;
	OutCameraForward = SafeDirection(POV.Rotation.GetForwardVector(), FVector::ForwardVector);
	const FVector End = OutCameraLocation + OutCameraForward * AimRayDistance;

	FHitResult Hit;
	const uint32 Mask =
		ObjectTypeBit(ECollisionChannel::WorldStatic)
		| ObjectTypeBit(ECollisionChannel::WorldDynamic)
		| ObjectTypeBit(ECollisionChannel::Pawn);
	const AActor* IgnoreActor = GetOwner();
	bool bHit = World->PhysicsRaycastByObjectTypes(
		OutCameraLocation,
		OutCameraForward,
		AimRayDistance,
		Hit,
		Mask,
		IgnoreActor);
	bool bVisualFallbackHit = false;
	if (!bHit || !IsBossActor(Hit.HitActor))
	{
		AActor* VisualBossActor = nullptr;
		FHitResult VisualHit;
		if (RaycastBossVisualFallback(
			World,
			OutCameraLocation,
			OutCameraForward,
			AimRayDistance,
			VisualHit,
			VisualBossActor))
		{
			Hit = VisualHit;
			Hit.HitActor = VisualBossActor;
			bHit = true;
			bVisualFallbackHit = true;
		}
	}

	const FVector DebugEnd = bHit ? Hit.WorldHitLocation : End;
	if (bDrawAimRay)
	{
		DrawDebugLine(World, OutCameraLocation, End, FColor(0, 180, 255), AimRayDebugDuration);
		DrawDebugLine(World, OutCameraLocation, DebugEnd, bHit && IsBossActor(Hit.HitActor) ? FColor::Green() : FColor::Red(), AimRayDebugDuration);
	}

	if (!bHit || !IsBossActor(Hit.HitActor))
	{
		UE_LOG("[PlayerSpray] Aim ray miss boss hit=%d actor=%s visualFallback=%d povLoc=(%.3f,%.3f,%.3f) povRot=(%.3f,%.3f,%.3f) forward=(%.3f,%.3f,%.3f)",
			(int)bHit,
			Hit.HitActor ? Hit.HitActor->GetName().c_str() : "nil",
			(int)bVisualFallbackHit,
			OutCameraLocation.X,
			OutCameraLocation.Y,
			OutCameraLocation.Z,
			POV.Rotation.Pitch,
			POV.Rotation.Yaw,
			POV.Rotation.Roll,
			OutCameraForward.X,
			OutCameraForward.Y,
			OutCameraForward.Z);
		return false;
	}

	OutBossActor = Hit.HitActor;
	UE_LOG("[PlayerSpray] Aim ray hit boss actor=%s hitLoc=(%.3f,%.3f,%.3f) distance=%.3f visualFallback=%d povLoc=(%.3f,%.3f,%.3f) povRot=(%.3f,%.3f,%.3f) forward=(%.3f,%.3f,%.3f)",
		OutBossActor ? OutBossActor->GetName().c_str() : "nil",
		Hit.WorldHitLocation.X,
		Hit.WorldHitLocation.Y,
		Hit.WorldHitLocation.Z,
		Hit.Distance,
		(int)bVisualFallbackHit,
		OutCameraLocation.X,
		OutCameraLocation.Y,
		OutCameraLocation.Z,
		POV.Rotation.Pitch,
		POV.Rotation.Yaw,
		POV.Rotation.Roll,
		OutCameraForward.X,
		OutCameraForward.Y,
		OutCameraForward.Z);
	return true;
}

bool UPlayerSprayProjectileComponent::IsBossActor(const AActor* Candidate) const
{
	if (!Candidate)
	{
		return false;
	}

	if (HasActorTag(Candidate, BossTagName))
	{
		return true;
	}

	if (Cast<ABossCharacter>(Candidate))
	{
		return true;
	}

	const FString Name = Candidate->GetName();
	return Name.find("Boss") != FString::npos || Name.find("boss") != FString::npos;
}

bool UPlayerSprayProjectileComponent::RaycastBossVisualFallback(
	UWorld* World,
	const FVector& Start,
	const FVector& Direction,
	float MaxDistance,
	FHitResult& OutHit,
	AActor*& OutBossActor) const
{
	OutHit = FHitResult();
	OutBossActor = nullptr;
	if (!World || MaxDistance <= 0.0f)
	{
		return false;
	}

	float BestDistance = (std::numeric_limits<float>::max)();
	FHitResult BestHit;
	AActor* BestActor = nullptr;
	FRay Ray;
	Ray.Origin = Start;
	Ray.Direction = Direction;

	for (AActor* Candidate : World->GetActors())
	{
		if (!IsValid(Candidate) || Candidate == GetOwner() || !IsBossActor(Candidate))
		{
			continue;
		}

		for (UActorComponent* Component : Candidate->GetComponents())
		{
			UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
			if (!Primitive || !Primitive->IsVisible())
			{
				continue;
			}

			FHitResult ComponentHit;
			if (Primitive->LineTraceComponent(Ray, ComponentHit))
			{
				const float Distance = (ComponentHit.WorldHitLocation - Start).Length();
				if (Distance >= 0.0f && Distance <= MaxDistance && Distance < BestDistance)
				{
					BestDistance = Distance;
					BestHit = ComponentHit;
					BestHit.HitActor = Candidate;
					BestHit.HitComponent = Primitive;
					BestHit.Distance = Distance;
					BestActor = Candidate;
				}
				continue;
			}

			float BoxDistance = 0.0f;
			if (RayIntersectsBox(Start, Direction, MaxDistance, Primitive->GetWorldBoundingBox(), BoxDistance)
				&& BoxDistance < BestDistance)
			{
				BestDistance = BoxDistance;
				BestHit = FHitResult();
				BestHit.bHit = true;
				BestHit.HitActor = Candidate;
				BestHit.HitComponent = Primitive;
				BestHit.Distance = BoxDistance;
				BestHit.WorldHitLocation = Start + Direction * BoxDistance;
				BestHit.WorldNormal = (BestHit.WorldHitLocation - Candidate->GetActorLocation()).Normalized();
				BestHit.ImpactNormal = BestHit.WorldNormal;
				BestActor = Candidate;
			}
		}
	}

	if (!BestActor)
	{
		return false;
	}

	OutHit = BestHit;
	OutBossActor = BestActor;
	return true;
}

bool UPlayerSprayProjectileComponent::RayIntersectsBox(
	const FVector& Start,
	const FVector& Direction,
	float MaxDistance,
	const FBoundingBox& Bounds,
	float& OutDistance) const
{
	OutDistance = 0.0f;
	if (!Bounds.IsValid() || MaxDistance <= 0.0f)
	{
		return false;
	}

	float TMin = 0.0f;
	float TMax = MaxDistance;

	const auto TestAxis = [&TMin, &TMax](float StartValue, float DirectionValue, float MinValue, float MaxValue) -> bool
	{
		if (std::fabs(DirectionValue) < 1.0e-6f)
		{
			return StartValue >= MinValue && StartValue <= MaxValue;
		}

		float T1 = (MinValue - StartValue) / DirectionValue;
		float T2 = (MaxValue - StartValue) / DirectionValue;
		if (T1 > T2)
		{
			std::swap(T1, T2);
		}
		TMin = (std::max)(TMin, T1);
		TMax = (std::min)(TMax, T2);
		return TMin <= TMax;
	};

	if (!TestAxis(Start.X, Direction.X, Bounds.Min.X, Bounds.Max.X)) return false;
	if (!TestAxis(Start.Y, Direction.Y, Bounds.Min.Y, Bounds.Max.Y)) return false;
	if (!TestAxis(Start.Z, Direction.Z, Bounds.Min.Z, Bounds.Max.Z)) return false;

	OutDistance = TMin >= 0.0f ? TMin : TMax;
	return OutDistance >= 0.0f && OutDistance <= MaxDistance;
}

void UPlayerSprayProjectileComponent::SpawnProjectile(
	const FVector& Origin,
	const FVector& CameraForward,
	AActor* TargetActor,
	int32 BurstIndex,
	int32 BurstCount)
{
	FPlayerSprayProjectile Projectile;
	Projectile.Position = Origin;
	Projectile.PreviousPosition = Origin;
	const FVector Direction = BuildSpreadDirection(CameraForward, BurstIndex, BurstCount);
	Projectile.Velocity = Direction * InitialSpeed;
	Projectile.HomingTarget = TargetActor;
	Projectile.Lifetime = (std::max)(0.01f, ProjectileLifetime);
	Projectile.Radius = (std::max)(0.001f, ProjectileRadius);
	Projectile.Damage = (std::max)(0.0f, ProjectileDamage);
	Projectile.TrailSamples.push_back(Origin);
	Projectiles.push_back(Projectile);
}

void UPlayerSprayProjectileComponent::TickProjectiles(float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	for (int32 Index = 0; Index < static_cast<int32>(Projectiles.size());)
	{
		FPlayerSprayProjectile& Projectile = Projectiles[Index];
		Projectile.Age += DeltaTime;
		Projectile.ScatterAge += DeltaTime;

		if (!Projectile.bHoming && Projectile.ScatterAge >= ScatterDuration)
		{
			Projectile.bHoming = true;
			if (HomingSpeed > 0.0f)
			{
				Projectile.Velocity = SafeDirection(Projectile.Velocity, FVector::ForwardVector) * HomingSpeed;
			}
		}

		UpdateHoming(Projectile, DeltaTime);
		Projectile.PreviousPosition = Projectile.Position;
		Projectile.Position += Projectile.Velocity * DeltaTime;

		if (CheckProjectileCollision(Projectile) || Projectile.Age >= Projectile.Lifetime)
		{
			RemoveProjectileAtIndex(Index);
			continue;
		}

		Projectile.TrailSampleAccumulator += DeltaTime;
		const float SampleInterval = TrailMaxSamples > 1 ? TrailLifetime / static_cast<float>(TrailMaxSamples - 1) : TrailLifetime;
		if (Projectile.TrailSamples.empty() || Projectile.TrailSampleAccumulator >= (std::max)(0.005f, SampleInterval))
		{
			Projectile.TrailSampleAccumulator = 0.0f;
			Projectile.TrailSamples.push_back(Projectile.Position);
			while (static_cast<int32>(Projectile.TrailSamples.size()) > (std::max)(2, TrailMaxSamples))
			{
				Projectile.TrailSamples.erase(Projectile.TrailSamples.begin());
			}
		}

		++Index;
	}
}

void UPlayerSprayProjectileComponent::UpdateHoming(FPlayerSprayProjectile& Projectile, float DeltaTime)
{
	if (!Projectile.bHoming || DeltaTime <= 0.0f)
	{
		return;
	}

	const AActor* TargetActor = Projectile.HomingTarget.Get();
	if (!TargetActor)
	{
		return;
	}

	const FVector DesiredDirection = SafeDirection(TargetActor->GetActorLocation() - Projectile.Position, Projectile.Velocity);
	const float CurrentSpeed = Projectile.Velocity.Length();
	if (CurrentSpeed <= 0.0f)
	{
		return;
	}

	const FVector CurrentDirection = SafeDirection(Projectile.Velocity, DesiredDirection);
	const float Dot = ClampFloat(CurrentDirection.Dot(DesiredDirection), -1.0f, 1.0f);
	const float AngleRadians = std::acos(Dot);
	if (AngleRadians <= 0.0001f)
	{
		Projectile.Velocity = DesiredDirection * CurrentSpeed;
		return;
	}

	const float MaxTurnRadians = (std::max)(0.0f, HomingMaxTurnRateDegrees) * (Pi / 180.0f) * DeltaTime;
	const float TurnAlpha = MaxTurnRadians > 0.0f ? ClampFloat(MaxTurnRadians / AngleRadians, 0.0f, 1.0f) : 1.0f;
	const float StrengthAlpha = ClampFloat((std::max)(0.0f, HomingStrength) * DeltaTime, 0.0f, 1.0f);
	const float Alpha = (std::min)(TurnAlpha, StrengthAlpha);
	const FVector NewDirection = SafeDirection(CurrentDirection * (1.0f - Alpha) + DesiredDirection * Alpha, DesiredDirection);
	Projectile.Velocity = NewDirection * (HomingSpeed > 0.0f ? HomingSpeed : CurrentSpeed);
}

bool UPlayerSprayProjectileComponent::CheckProjectileCollision(const FPlayerSprayProjectile& Projectile)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FHitResult Hit;
	const uint32 Mask =
		ObjectTypeBit(ECollisionChannel::WorldStatic)
		| ObjectTypeBit(ECollisionChannel::WorldDynamic)
		| ObjectTypeBit(ECollisionChannel::Pawn)
		| ObjectTypeBit(ECollisionChannel::Trigger);
	const bool bHit = World->PhysicsSweepByObjectTypes(
		Projectile.PreviousPosition,
		Projectile.Position,
		FQuat::Identity,
		FCollisionShape::MakeSphere(Projectile.Radius),
		Hit,
		Mask,
		GetOwner());
	if (bHit)
	{
		AActor* TargetActor = ResolveHitActor(Hit);
		if (TargetActor == GetOwner() || HasActorTag(TargetActor, PlayerTagName))
		{
			return false;
		}
		ApplyDamageToHitTarget(Projectile, Hit);
	}
	return bHit;
}

void UPlayerSprayProjectileComponent::ApplyDamageToHitTarget(
	const FPlayerSprayProjectile& Projectile,
	const FHitResult& Hit) const
{
	if (Projectile.Damage <= 0.0f)
	{
		return;
	}

	AActor* TargetActor = ResolveHitActor(Hit);
	if (!TargetActor || TargetActor == GetOwner() || HasActorTag(TargetActor, PlayerTagName))
	{
		return;
	}

	if (UBulletHellDamageReceiverComponent* DamageReceiver = TargetActor->GetComponentByClass<UBulletHellDamageReceiverComponent>())
	{
		DamageReceiver->ApplyDamage(Projectile.Damage);
	}
}

void UPlayerSprayProjectileComponent::RemoveProjectileAtIndex(int32 ProjectileIndex)
{
	if (ProjectileIndex < 0 || ProjectileIndex >= static_cast<int32>(Projectiles.size()))
	{
		return;
	}

	const int32 LastIndex = static_cast<int32>(Projectiles.size()) - 1;
	if (ProjectileIndex != LastIndex)
	{
		Projectiles[ProjectileIndex] = Projectiles[LastIndex];
	}
	Projectiles.pop_back();
}

void UPlayerSprayProjectileComponent::SyncRender()
{
	UInstancedStaticMeshComponent* Renderer = EnsureRenderComponent();
	if (!Renderer)
	{
		return;
	}

	TArray<FTransform> Transforms;
	Transforms.reserve(Projectiles.size());
	const FMatrix RendererWorldInverse = Renderer->GetWorldInverseMatrix();
	for (const FPlayerSprayProjectile& Projectile : Projectiles)
	{
		const FTransform WorldTransform = MakeProjectileTransform(Projectile);
		Transforms.push_back(FTransform(WorldTransform.ToMatrix() * RendererWorldInverse));
	}
	Renderer->SetInstances(std::move(Transforms));
}

void UPlayerSprayProjectileComponent::SyncTrail()
{
	UBulletTrailComponent* TrailRenderer = EnsureTrailComponent();
	if (!TrailRenderer)
	{
		return;
	}

	TArray<FBulletTrailChain> Chains;
	Chains.reserve(Projectiles.size());
	for (const FPlayerSprayProjectile& Projectile : Projectiles)
	{
		if (Projectile.TrailSamples.size() < 2)
		{
			continue;
		}

		FBulletTrailChain Chain;
		Chain.MaterialPath = TrailMaterialPath;
		for (const FVector& Sample : Projectile.TrailSamples)
		{
			FBulletTrailPoint Point;
			Point.Position = Sample;
			Point.Color = TrailColor;
			Point.Width = TrailWidth;
			Chain.Points.push_back(Point);
		}
		Chains.push_back(std::move(Chain));
	}

	if (Chains.empty())
	{
		TrailRenderer->ClearTrailChains();
	}
	else
	{
		TrailRenderer->SetTrailChains(std::move(Chains));
	}
}

void UPlayerSprayProjectileComponent::ClearRender()
{
	if (UInstancedStaticMeshComponent* Renderer = RenderComponent.Get())
	{
		Renderer->ClearInstances();
	}
	if (UBulletTrailComponent* TrailRenderer = TrailComponent.Get())
	{
		TrailRenderer->ClearTrailChains();
	}
}

UInstancedStaticMeshComponent* UPlayerSprayProjectileComponent::EnsureRenderComponent()
{
	UInstancedStaticMeshComponent* Renderer = RenderComponent.Get();
	if (Renderer && Renderer->GetOwner() == GetOwner())
	{
		return Renderer;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	const FName ExpectedName("PlayerSprayProjectileRenderer");
	for (UActorComponent* Component : OwnerActor->GetComponents())
	{
		Renderer = Cast<UInstancedStaticMeshComponent>(Component);
		if (Renderer && Renderer->GetFName() == ExpectedName)
		{
			RenderComponent = Renderer;
			return Renderer;
		}
	}

	Renderer = OwnerActor->AddComponent<UInstancedStaticMeshComponent>();
	if (!Renderer)
	{
		return nullptr;
	}

	Renderer->SetFName(ExpectedName);
	if (USceneComponent* RootComponent = OwnerActor->GetRootComponent())
	{
		Renderer->AttachToComponent(RootComponent);
	}
	if (!MeshPath.empty() && MeshPath != "None")
	{
		Renderer->SetStaticMeshByPath(MeshPath);
	}
	if (!MaterialPath.empty() && MaterialPath != "None")
	{
		Renderer->SetMaterialByPath(0, MaterialPath);
	}
	RenderComponent = Renderer;
	return Renderer;
}

UBulletTrailComponent* UPlayerSprayProjectileComponent::EnsureTrailComponent()
{
	UBulletTrailComponent* TrailRenderer = TrailComponent.Get();
	if (TrailRenderer && TrailRenderer->GetOwner() == GetOwner())
	{
		return TrailRenderer;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	const FName ExpectedName("PlayerSprayProjectileTrail");
	for (UActorComponent* Component : OwnerActor->GetComponents())
	{
		TrailRenderer = Cast<UBulletTrailComponent>(Component);
		if (TrailRenderer && TrailRenderer->GetFName() == ExpectedName)
		{
			TrailComponent = TrailRenderer;
			return TrailRenderer;
		}
	}

	TrailRenderer = OwnerActor->AddComponent<UBulletTrailComponent>();
	if (!TrailRenderer)
	{
		return nullptr;
	}

	TrailRenderer->SetFName(ExpectedName);
	if (USceneComponent* RootComponent = OwnerActor->GetRootComponent())
	{
		TrailRenderer->AttachToComponent(RootComponent);
	}
	TrailComponent = TrailRenderer;
	return TrailRenderer;
}

FTransform UPlayerSprayProjectileComponent::MakeProjectileTransform(const FPlayerSprayProjectile& Projectile) const
{
	const FVector Direction = SafeDirection(Projectile.Velocity, FVector::ForwardVector);
	const float YawDegrees = std::atan2(Direction.Y, Direction.X) * (180.0f / Pi);
	const float FlatLength = std::sqrt(Direction.X * Direction.X + Direction.Y * Direction.Y);
	const float PitchDegrees = std::atan2(Direction.Z, FlatLength) * (180.0f / Pi);
	const float Scale = (std::max)(0.001f, RenderScale);
	return FTransform(
		Projectile.Position,
		FRotator(PitchDegrees, YawDegrees, 0.0f),
		FVector(Scale, Scale, Scale));
}

FVector UPlayerSprayProjectileComponent::BuildSpreadDirection(
	const FVector& CameraForward,
	int32 BurstIndex,
	int32 BurstCount) const
{
	const FVector Forward = SafeDirection(CameraForward, FVector::ForwardVector);
	FVector Right;
	FVector Up;
	MakeBasis(Forward, Right, Up);

	const int32 Count = (std::max)(1, BurstCount);
	const float T = Count > 1
		? static_cast<float>(BurstIndex) / static_cast<float>(Count - 1)
		: 0.5f;
	const float Angle = (static_cast<float>(BurstIndex) * 2.39996322973f);
	const float Radius = std::sqrt(ClampFloat(T, 0.0f, 1.0f));
	const float ConeRadians = ConeHalfAngleDegrees * (Pi / 180.0f);
	const FVector Offset =
		Right * std::cos(Angle) * std::sin(ConeRadians) * Radius
		+ Up * std::sin(Angle) * std::sin(ConeRadians) * Radius;
	return SafeDirection(Forward * std::cos(ConeRadians * Radius) + Offset, Forward);
}

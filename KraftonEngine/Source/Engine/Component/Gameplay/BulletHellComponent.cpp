#include "BulletHellComponent.h"

#include "Component/Primitive/InstancedStaticMeshComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/Gameplay/BulletHellHealthProbeComponent.h"
#include "Core/Logging/Log.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/Rotator.h"
#include "Profiling/Stats/BulletHellStats.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
	uint32 AdvanceNonZero(uint32 Value)
	{
		++Value;
		return Value == 0 ? 1 : Value;
	}

	FVector SafeDirection(const FVector& Direction, const FVector& Fallback)
	{
		return Direction.IsNearlyZero() ? Fallback : Direction.Normalized();
	}

	float ClampFloat(float Value, float MinValue, float MaxValue)
	{
		return (std::max)(MinValue, (std::min)(MaxValue, Value));
	}

	bool IsProperty(const char* PropertyName, const char* MemberName, const char* DisplayName)
	{
		return PropertyName
			&& (std::strcmp(PropertyName, MemberName) == 0 || std::strcmp(PropertyName, DisplayName) == 0);
	}
}

UBulletHellComponent::UBulletHellComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEnabled = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBulletHellComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	if (bEnableRendering)
	{
		EnsureRenderComponent();
		RebuildRendererFromBullets();
	}
}

void UBulletHellComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);

	if (IsProperty(PropertyName, "RendererMeshPath", "Renderer Mesh Path") ||
		IsProperty(PropertyName, "RendererMaterialPath", "Renderer Material Path") ||
		IsProperty(PropertyName, "bEnableRendering", "Enable Rendering") ||
		IsProperty(PropertyName, "bAutoCreateRenderer", "Auto Create Renderer") ||
		IsProperty(PropertyName, "RenderScale", "Render Scale"))
	{
		if (bEnableRendering)
		{
			EnsureRenderComponent();
			ApplyRenderAssets();
			RebuildRendererFromBullets();
		}
		else
		{
			ClearRenderer();
		}
	}
}

void UBulletHellComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	(void)TickType;
	(void)ThisTickFunction;

	ResetPerFrameDebugStats();
	TickBullets(DeltaTime);
	SyncRenderInstancesBulk();
	RecordOverlayStats();
}

FBulletHandle UBulletHellComponent::SpawnBullet(
	const FVector& Position,
	const FVector& Velocity,
	float Radius,
	float Lifetime)
{
	FBulletSpawnParams Params;
	Params.Position = Position;
	Params.Velocity = Velocity;
	Params.Archetype.MeshPath = RendererMeshPath;
	Params.Archetype.MaterialPath = RendererMaterialPath;
	Params.Archetype.Radius = Radius;
	Params.Archetype.Speed = Velocity.Length();
	Params.Archetype.Lifetime = Lifetime;
	Params.Archetype.RenderScale = RenderScale;
	Params.Archetype.BehaviorType = EBulletBehaviorType::Linear;
	Params.BehaviorType = EBulletBehaviorType::Linear;
	return SpawnBullet(Params);
}

FBulletHandle UBulletHellComponent::SpawnBullet(const FBulletSpawnParams& Params)
{
	const FBulletArchetype& Archetype = Params.Archetype;

	FBulletInstance Bullet;
	Bullet.Id = NextBulletId;
	Bullet.Generation = NextBulletGeneration;
	Bullet.MeshPath = Archetype.MeshPath;
	Bullet.MaterialPath = Archetype.MaterialPath;
	Bullet.Position = Params.Position;
	Bullet.PreviousPosition = Params.Position;
	Bullet.Velocity = Params.Velocity;
	Bullet.Radius = (std::max)(0.01f, Archetype.Radius);
	Bullet.Damage = (std::max)(0.0f, Archetype.Damage);
	Bullet.Age = 0.0f;
	Bullet.Lifetime = Archetype.Lifetime;
	Bullet.ArchetypeIndex = Params.ArchetypeIndex;
	Bullet.RenderSlotIndex = -1;
	Bullet.RenderScale = (std::max)(0.01f, Archetype.RenderScale);
	Bullet.BehaviorType = Params.BehaviorType;
	Bullet.BehaviorPhase = Params.BehaviorType == EBulletBehaviorType::ColdLaunch ? EBulletPhase::Waiting : EBulletPhase::Active;
	Bullet.HomingTargetPosition = Params.HomingTargetPosition;
	Bullet.HomingTargetActor = Params.HomingTargetActor;
	Bullet.HomingStrength = (std::max)(0.0f, Params.HomingStrength);
	Bullet.HomingMaxTurnRateDegrees = (std::max)(0.0f, Params.HomingMaxTurnRateDegrees);
	Bullet.ColdLaunchDelay = (std::max)(0.0f, Params.ColdLaunchDelay);
	Bullet.ColdLaunchVelocity = Params.ColdLaunchVelocity;
	Bullet.TimedActivationTime = Params.TimedActivationTime;
	Bullet.TimedVelocity = Params.TimedVelocity;
	Bullet.RenderInstanceIndex = -1;
	Bullet.bAlive = true;

	NextBulletId = AdvanceNonZero(NextBulletId);
	NextBulletGeneration = AdvanceNonZero(NextBulletGeneration);

	const int32 NewIndex = static_cast<int32>(Bullets.size());
	BulletIndexById[Bullet.Id] = NewIndex;
	Bullets.push_back(Bullet);
	const int32 RenderSlotIndex = FindOrCreateRenderSlot(Archetype);
	Bullets.back().RenderSlotIndex = RenderSlotIndex;
	if (RenderSlotIndex >= 0)
	{
		FBulletRenderSlot& Slot = RenderSlots[RenderSlotIndex];
		if (UInstancedStaticMeshComponent* Renderer = EnsureRenderSlotComponent(RenderSlotIndex))
		{
			Bullets.back().RenderInstanceIndex = Renderer->AddInstance(MakeBulletRenderTransform(Bullets.back()));
			Slot.BulletIndices.push_back(NewIndex);
		}
	}

	DebugStats.ActiveBulletCount = static_cast<int32>(Bullets.size());
	++DebugStats.TotalSpawned;
	UpdateBehaviorDebugStats();
	UpdateRenderDebugStats();

	return FBulletHandle{ Bullet.Id, Bullet.Generation };
}

bool UBulletHellComponent::KillBullet(const FBulletHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return false;
	}

	auto It = BulletIndexById.find(Handle.Id);
	if (It == BulletIndexById.end())
	{
		return false;
	}

	const int32 BulletIndex = It->second;
	if (BulletIndex < 0 || BulletIndex >= static_cast<int32>(Bullets.size()))
	{
		BulletIndexById.erase(It);
		return false;
	}

	const FBulletInstance& Bullet = Bullets[BulletIndex];
	if (!Bullet.bAlive || Bullet.Generation != Handle.Generation)
	{
		return false;
	}

	return RemoveBulletAtIndex(BulletIndex, false);
}

bool UBulletHellComponent::KillBulletById(int32 BulletId, int32 Generation)
{
	if (BulletId <= 0 || Generation <= 0)
	{
		return false;
	}

	return KillBullet(FBulletHandle{ static_cast<uint32>(BulletId), static_cast<uint32>(Generation) });
}

bool UBulletHellComponent::IsBulletAlive(const FBulletHandle& Handle) const
{
	return FindBullet(Handle) != nullptr;
}

const FBulletInstance* UBulletHellComponent::FindBullet(const FBulletHandle& Handle) const
{
	if (!Handle.IsValid())
	{
		return nullptr;
	}

	auto It = BulletIndexById.find(Handle.Id);
	if (It == BulletIndexById.end())
	{
		return nullptr;
	}

	const int32 BulletIndex = It->second;
	if (BulletIndex < 0 || BulletIndex >= static_cast<int32>(Bullets.size()))
	{
		return nullptr;
	}

	const FBulletInstance& Bullet = Bullets[BulletIndex];
	return Bullet.bAlive && Bullet.Generation == Handle.Generation ? &Bullet : nullptr;
}

void UBulletHellComponent::ClearBullets()
{
	DebugStats.TotalKilled += static_cast<uint32>(Bullets.size());
	Bullets.clear();
	BulletIndexById.clear();
	ClearRenderer();
	DebugStats.ActiveBulletCount = 0;
	DebugStats.DebugDrawSelectedCount = 0;
	DebugStats.DebugDrawTruncatedCount = 0;
	UpdateBehaviorDebugStats();
	UpdateRenderDebugStats();
}

int32 UBulletHellComponent::GetBulletCount() const
{
	return static_cast<int32>(Bullets.size());
}

void UBulletHellComponent::RecordDebugDrawStats(int32 SelectedCount, int32 TruncatedCount)
{
	DebugStats.DebugDrawSelectedCount = (std::max)(0, SelectedCount);
	DebugStats.DebugDrawTruncatedCount = (std::max)(0, TruncatedCount);
}

void UBulletHellComponent::TickBullets(float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		DebugStats.ActiveBulletCount = static_cast<int32>(Bullets.size());
		UpdateBehaviorDebugStats();
		return;
	}

	for (int32 Index = 0; Index < static_cast<int32>(Bullets.size());)
	{
		FBulletInstance& Bullet = Bullets[Index];
		UpdateBulletBehavior(Bullet, DeltaTime);
		Bullet.PreviousPosition = Bullet.Position;
		Bullet.Position += Bullet.Velocity * DeltaTime;
		Bullet.Age += DeltaTime;

		const EBulletCollisionKillReason CollisionKillReason = CheckBulletCollision(Bullet);
		if (CollisionKillReason != EBulletCollisionKillReason::None)
		{
			if (CollisionKillReason == EBulletCollisionKillReason::Erase)
			{
				++DebugStats.EraseKilledCount;
			}
			else
			{
				++DebugStats.CollisionKilledCount;
			}
			RemoveBulletAtIndex(Index, false);
			continue;
		}

		if (Bullet.Lifetime >= 0.0f && Bullet.Age >= Bullet.Lifetime)
		{
			RemoveBulletAtIndex(Index, true);
			continue;
		}

		++Index;
	}

	DebugStats.ActiveBulletCount = static_cast<int32>(Bullets.size());
	UpdateBehaviorDebugStats();
}

void UBulletHellComponent::UpdateBulletBehavior(FBulletInstance& Bullet, float DeltaTime)
{
	switch (Bullet.BehaviorType)
	{
	case EBulletBehaviorType::Homing:
		UpdateHomingBehavior(Bullet, DeltaTime);
		break;
	case EBulletBehaviorType::ColdLaunch:
		if (Bullet.BehaviorPhase == EBulletPhase::Waiting && Bullet.Age + DeltaTime >= Bullet.ColdLaunchDelay)
		{
			Bullet.Velocity = Bullet.ColdLaunchVelocity;
			Bullet.BehaviorPhase = EBulletPhase::Active;
			++DebugStats.BehaviorTransitionCount;
		}
		break;
	case EBulletBehaviorType::TimedVelocityChange:
		if (Bullet.BehaviorPhase == EBulletPhase::Active &&
			Bullet.TimedActivationTime >= 0.0f &&
			Bullet.Age + DeltaTime >= Bullet.TimedActivationTime)
		{
			Bullet.Velocity = Bullet.TimedVelocity;
			Bullet.BehaviorPhase = EBulletPhase::Complete;
			++DebugStats.BehaviorTransitionCount;
		}
		break;
	case EBulletBehaviorType::Linear:
	default:
		break;
	}
}

void UBulletHellComponent::UpdateHomingBehavior(FBulletInstance& Bullet, float DeltaTime)
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	const AActor* TargetActor = Bullet.HomingTargetActor.Get();
	const FVector TargetPosition = TargetActor ? TargetActor->GetActorLocation() : Bullet.HomingTargetPosition;
	const FVector DesiredDirection = SafeDirection(TargetPosition - Bullet.Position, Bullet.Velocity);
	const float CurrentSpeed = Bullet.Velocity.Length();
	if (CurrentSpeed <= 0.0f || DesiredDirection.IsNearlyZero())
	{
		return;
	}

	const FVector CurrentDirection = SafeDirection(Bullet.Velocity, DesiredDirection);
	const float Dot = ClampFloat(CurrentDirection.Dot(DesiredDirection), -1.0f, 1.0f);
	const float AngleRadians = std::acos(Dot);
	if (AngleRadians <= 0.0001f)
	{
		Bullet.Velocity = DesiredDirection * CurrentSpeed;
		return;
	}

	const float MaxTurnRadians = (std::max)(0.0f, Bullet.HomingMaxTurnRateDegrees) * (3.1415926535f / 180.0f) * DeltaTime;
	const float TurnAlpha = MaxTurnRadians > 0.0f ? ClampFloat(MaxTurnRadians / AngleRadians, 0.0f, 1.0f) : 1.0f;
	const float StrengthAlpha = ClampFloat((std::max)(0.0f, Bullet.HomingStrength) * DeltaTime, 0.0f, 1.0f);
	const float Alpha = (std::min)(TurnAlpha, StrengthAlpha);
	const FVector NewDirection = SafeDirection(
		CurrentDirection * (1.0f - Alpha) + DesiredDirection * Alpha,
		DesiredDirection);
	Bullet.Velocity = NewDirection * CurrentSpeed;
}

void UBulletHellComponent::UpdateBehaviorDebugStats()
{
	DebugStats.ActiveBulletCount = static_cast<int32>(Bullets.size());
	DebugStats.ActiveLinearCount = 0;
	DebugStats.ActiveHomingCount = 0;
	DebugStats.ActiveColdLaunchCount = 0;
	DebugStats.ActiveTimedVelocityChangeCount = 0;
	DebugStats.ActivePrimaryArchetypeCount = 0;
	DebugStats.ActiveSecondaryArchetypeCount = 0;

	for (const FBulletInstance& Bullet : Bullets)
	{
		if (Bullet.ArchetypeIndex == 1)
		{
			++DebugStats.ActiveSecondaryArchetypeCount;
		}
		else
		{
			++DebugStats.ActivePrimaryArchetypeCount;
		}

		switch (Bullet.BehaviorType)
		{
		case EBulletBehaviorType::Homing:
			++DebugStats.ActiveHomingCount;
			break;
		case EBulletBehaviorType::ColdLaunch:
			++DebugStats.ActiveColdLaunchCount;
			break;
		case EBulletBehaviorType::TimedVelocityChange:
			++DebugStats.ActiveTimedVelocityChangeCount;
			break;
		case EBulletBehaviorType::Linear:
		default:
			++DebugStats.ActiveLinearCount;
			break;
		}
	}
}

UBulletHellComponent::EBulletCollisionKillReason UBulletHellComponent::CheckBulletCollision(const FBulletInstance& Bullet)
{
	if (!bEnableCollision)
	{
		return EBulletCollisionKillReason::None;
	}

	FHitResult Hit;
	const uint32 EraseObjectTypeMask = BuildEraseObjectTypeMask();
	if (bEnableEraseVolumes && EraseObjectTypeMask != 0 && SweepBulletByObjectTypes(Bullet, EraseObjectTypeMask, Hit))
	{
		return EBulletCollisionKillReason::Erase;
	}

	const uint32 CollisionObjectTypeMask = BuildCollisionObjectTypeMask();
	if (CollisionObjectTypeMask != 0 && SweepBulletByObjectTypes(Bullet, CollisionObjectTypeMask, Hit))
	{
		ApplyDamageToHitTarget(Bullet, Hit);
		return EBulletCollisionKillReason::Collision;
	}

	if (bKillOnBlockingCollision && SweepBulletByChannel(Bullet, CollisionTraceChannel, Hit))
	{
		ApplyDamageToHitTarget(Bullet, Hit);
		return EBulletCollisionKillReason::Collision;
	}

	return EBulletCollisionKillReason::None;
}

bool UBulletHellComponent::SweepBulletByChannel(
	const FBulletInstance& Bullet,
	ECollisionChannel TraceChannel,
	FHitResult& OutHit)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	++DebugStats.CollisionQueryCount;
	const FCollisionShape SweepShape = FCollisionShape::MakeSphere((std::max)(0.01f, Bullet.Radius));
	const bool bHit = World->PhysicsSweep(
		Bullet.PreviousPosition,
		Bullet.Position,
		FQuat::Identity,
		SweepShape,
		OutHit,
		TraceChannel,
		GetOwner());

	if (bHit)
	{
		++DebugStats.CollisionHitCount;
	}

	return bHit;
}

bool UBulletHellComponent::SweepBulletByObjectTypes(
	const FBulletInstance& Bullet,
	uint32 ObjectTypeMask,
	FHitResult& OutHit)
{
	if (ObjectTypeMask == 0)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	++DebugStats.CollisionQueryCount;
	const FCollisionShape SweepShape = FCollisionShape::MakeSphere((std::max)(0.01f, Bullet.Radius));
	const bool bHit = World->PhysicsSweepByObjectTypes(
		Bullet.PreviousPosition,
		Bullet.Position,
		FQuat::Identity,
		SweepShape,
		OutHit,
		ObjectTypeMask,
		GetOwner());

	if (bHit)
	{
		++DebugStats.CollisionHitCount;
	}

	return bHit;
}

uint32 UBulletHellComponent::BuildCollisionObjectTypeMask() const
{
	uint32 Mask = 0;
	if (bKillOnWorldStatic)
	{
		Mask |= ObjectTypeBit(ECollisionChannel::WorldStatic);
	}
	if (bKillOnWorldDynamic)
	{
		Mask |= ObjectTypeBit(ECollisionChannel::WorldDynamic);
	}
	if (bKillOnPawn)
	{
		Mask |= ObjectTypeBit(ECollisionChannel::Pawn);
	}
	return Mask;
}

uint32 UBulletHellComponent::BuildEraseObjectTypeMask() const
{
	uint32 Mask = 0;
	if (bEraseOnTrigger)
	{
		Mask |= ObjectTypeBit(ECollisionChannel::Trigger);
	}
	if (bEraseOnProjectile)
	{
		Mask |= ObjectTypeBit(ECollisionChannel::Projectile);
	}
	return Mask;
}

void UBulletHellComponent::ApplyDamageToHitTarget(const FBulletInstance& Bullet, const FHitResult& Hit) const
{
	if (Bullet.Damage <= 0.0f)
	{
		return;
	}

	AActor* TargetActor = Hit.HitActor;
	if (!TargetActor && Hit.HitComponent)
	{
		TargetActor = Hit.HitComponent->GetOwner();
	}

	if (!TargetActor)
	{
		return;
	}

	if (UBulletHellHealthProbeComponent* HealthProbe = TargetActor->GetComponentByClass<UBulletHellHealthProbeComponent>())
	{
		HealthProbe->ApplyDamage(Bullet.Damage);
	}
}

bool UBulletHellComponent::RemoveBulletAtIndex(int32 BulletIndex, bool bExpired)
{
	if (BulletIndex < 0 || BulletIndex >= static_cast<int32>(Bullets.size()))
	{
		return false;
	}

	const int32 LastIndex = static_cast<int32>(Bullets.size()) - 1;
	const uint32 RemovedId = Bullets[BulletIndex].Id;
	const int32 RemovedRenderSlotIndex = Bullets[BulletIndex].RenderSlotIndex;
	const int32 RemovedRenderIndex = Bullets[BulletIndex].RenderInstanceIndex;

	if (RemovedRenderSlotIndex >= 0 && RemovedRenderSlotIndex < static_cast<int32>(RenderSlots.size()))
	{
		FBulletRenderSlot& Slot = RenderSlots[RemovedRenderSlotIndex];
		if (RemovedRenderIndex >= 0 && RemovedRenderIndex < static_cast<int32>(Slot.BulletIndices.size()))
		{
			const int32 LastRenderIndex = static_cast<int32>(Slot.BulletIndices.size()) - 1;
			if (RemovedRenderIndex != LastRenderIndex)
			{
				const int32 MovedBulletIndex = Slot.BulletIndices[LastRenderIndex];
				Slot.BulletIndices[RemovedRenderIndex] = MovedBulletIndex;
				if (MovedBulletIndex >= 0 && MovedBulletIndex < static_cast<int32>(Bullets.size()))
				{
					Bullets[MovedBulletIndex].RenderInstanceIndex = RemovedRenderIndex;
				}
			}
			Slot.BulletIndices.pop_back();
		}

		if (UInstancedStaticMeshComponent* Renderer = GetRenderSlotComponent(RemovedRenderSlotIndex))
		{
			Renderer->RemoveInstanceSwap(RemovedRenderIndex);
		}
	}
	BulletIndexById.erase(RemovedId);

	if (BulletIndex != LastIndex)
	{
		Bullets[BulletIndex] = Bullets[LastIndex];
		if (Bullets[BulletIndex].RenderSlotIndex >= 0 &&
			Bullets[BulletIndex].RenderSlotIndex < static_cast<int32>(RenderSlots.size()) &&
			Bullets[BulletIndex].RenderInstanceIndex >= 0 &&
			Bullets[BulletIndex].RenderInstanceIndex < static_cast<int32>(RenderSlots[Bullets[BulletIndex].RenderSlotIndex].BulletIndices.size()))
		{
			RenderSlots[Bullets[BulletIndex].RenderSlotIndex].BulletIndices[Bullets[BulletIndex].RenderInstanceIndex] = BulletIndex;
		}
		BulletIndexById[Bullets[BulletIndex].Id] = BulletIndex;
	}

	Bullets.pop_back();

	if (bExpired)
	{
		++DebugStats.TotalExpired;
	}
	else
	{
		++DebugStats.TotalKilled;
	}
	DebugStats.ActiveBulletCount = static_cast<int32>(Bullets.size());
	UpdateBehaviorDebugStats();
	UpdateRenderDebugStats();
	return true;
}

UInstancedStaticMeshComponent* UBulletHellComponent::EnsureRenderComponent()
{
	FBulletArchetype DefaultArchetype;
	DefaultArchetype.MeshPath = RendererMeshPath;
	DefaultArchetype.MaterialPath = RendererMaterialPath;
	DefaultArchetype.RenderScale = RenderScale;
	const int32 SlotIndex = FindOrCreateRenderSlot(DefaultArchetype);
	return SlotIndex >= 0 ? EnsureRenderSlotComponent(SlotIndex) : nullptr;
}

UInstancedStaticMeshComponent* UBulletHellComponent::GetRenderComponent() const
{
	return GetRenderSlotComponent(0);
}

int32 UBulletHellComponent::FindOrCreateRenderSlot(const FBulletArchetype& Archetype)
{
	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(RenderSlots.size()); ++SlotIndex)
	{
		const FBulletRenderSlot& Slot = RenderSlots[SlotIndex];
		if (Slot.MeshPath == Archetype.MeshPath && Slot.MaterialPath == Archetype.MaterialPath)
		{
			return SlotIndex;
		}
	}

	FBulletRenderSlot NewSlot;
	NewSlot.MeshPath = Archetype.MeshPath;
	NewSlot.MaterialPath = Archetype.MaterialPath;
	RenderSlots.push_back(NewSlot);
	const int32 NewSlotIndex = static_cast<int32>(RenderSlots.size()) - 1;
	EnsureRenderSlotComponent(NewSlotIndex);
	return NewSlotIndex;
}

UInstancedStaticMeshComponent* UBulletHellComponent::EnsureRenderSlotComponent(int32 SlotIndex)
{
	if (!bEnableRendering)
	{
		return nullptr;
	}

	if (SlotIndex < 0 || SlotIndex >= static_cast<int32>(RenderSlots.size()))
	{
		return nullptr;
	}

	FBulletRenderSlot& Slot = RenderSlots[SlotIndex];
	UInstancedStaticMeshComponent* Renderer = Slot.Renderer.Get();
	if (Renderer && Renderer->GetOwner() == GetOwner())
	{
		return Renderer;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !bAutoCreateRenderer)
	{
		UpdateRenderDebugStats();
		return nullptr;
	}

	Renderer = OwnerActor->AddComponent<UInstancedStaticMeshComponent>();
	if (!Renderer)
	{
		UpdateRenderDebugStats();
		return nullptr;
	}

	char NameBuffer[64];
	std::snprintf(NameBuffer, sizeof(NameBuffer), "BulletHellRenderer%d", SlotIndex);
	Renderer->SetFName(FName(NameBuffer));
	if (USceneComponent* RootComponent = OwnerActor->GetRootComponent())
	{
		Renderer->AttachToComponent(RootComponent);
	}

	Slot.Renderer = Renderer;
	if (SlotIndex == 0)
	{
		RenderComponent = Renderer;
	}
	ApplyRenderSlotAssets(SlotIndex);
	UpdateRenderDebugStats();
	return Renderer;
}

UInstancedStaticMeshComponent* UBulletHellComponent::GetRenderSlotComponent(int32 SlotIndex) const
{
	if (!bEnableRendering)
	{
		return nullptr;
	}

	if (SlotIndex < 0 || SlotIndex >= static_cast<int32>(RenderSlots.size()))
	{
		return nullptr;
	}

	UInstancedStaticMeshComponent* Renderer = RenderSlots[SlotIndex].Renderer.Get();
	return Renderer && Renderer->GetOwner() == GetOwner() ? Renderer : nullptr;
}

void UBulletHellComponent::ApplyRenderAssets()
{
	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(RenderSlots.size()); ++SlotIndex)
	{
		ApplyRenderSlotAssets(SlotIndex);
	}
}

void UBulletHellComponent::ApplyRenderSlotAssets(int32 SlotIndex)
{
	UInstancedStaticMeshComponent* Renderer = GetRenderSlotComponent(SlotIndex);
	if (!Renderer)
	{
		return;
	}

	const FBulletRenderSlot& Slot = RenderSlots[SlotIndex];
	if (!Slot.MeshPath.empty() && Slot.MeshPath != "None")
	{
		Renderer->SetStaticMeshByPath(Slot.MeshPath);
	}

	if (!Slot.MaterialPath.empty() && Slot.MaterialPath != "None" && Renderer->GetMaterialSlotCount() > 0)
	{
		Renderer->SetMaterialByPath(0, Slot.MaterialPath);
	}
}

void UBulletHellComponent::RebuildRendererFromBullets()
{
	if (!bEnableRendering)
	{
		ClearRenderer();
		return;
	}

	for (FBulletRenderSlot& Slot : RenderSlots)
	{
		Slot.BulletIndices.clear();
		if (UInstancedStaticMeshComponent* Renderer = Slot.Renderer.Get())
		{
			Renderer->ClearInstances();
		}
	}

	for (int32 Index = 0; Index < static_cast<int32>(Bullets.size()); ++Index)
	{
		FBulletInstance& Bullet = Bullets[Index];
		FBulletArchetype Archetype;
		Archetype.MeshPath = Bullet.MeshPath;
		Archetype.MaterialPath = Bullet.MaterialPath;
		Archetype.Radius = Bullet.Radius;
		Archetype.Lifetime = Bullet.Lifetime;
		Archetype.RenderScale = Bullet.RenderScale;
		Archetype.BehaviorType = Bullet.BehaviorType;
		const int32 SlotIndex = FindOrCreateRenderSlot(Archetype);
		Bullet.RenderSlotIndex = SlotIndex;
		Bullet.RenderScale = (std::max)(0.01f, Archetype.RenderScale);
		Bullet.RenderInstanceIndex = -1;
		if (SlotIndex < 0)
		{
			continue;
		}

		FBulletRenderSlot& Slot = RenderSlots[SlotIndex];
		if (UInstancedStaticMeshComponent* Renderer = EnsureRenderSlotComponent(SlotIndex))
		{
			Bullet.RenderInstanceIndex = Renderer->AddInstance(MakeBulletRenderTransform(Bullet));
			Slot.BulletIndices.push_back(Index);
		}
	}

	UpdateRenderDebugStats();
}

void UBulletHellComponent::SyncRenderInstancesBulk()
{
	if (!bEnableRendering)
	{
		ClearRenderer();
		return;
	}

	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(RenderSlots.size()); ++SlotIndex)
	{
		FBulletRenderSlot& Slot = RenderSlots[SlotIndex];
		UInstancedStaticMeshComponent* Renderer = EnsureRenderSlotComponent(SlotIndex);
		if (!Renderer)
		{
			continue;
		}

		TArray<FTransform> Transforms;
		Transforms.reserve(Slot.BulletIndices.size());
		for (int32 RenderIndex = 0; RenderIndex < static_cast<int32>(Slot.BulletIndices.size()); ++RenderIndex)
		{
			const int32 BulletIndex = Slot.BulletIndices[RenderIndex];
			if (BulletIndex < 0 || BulletIndex >= static_cast<int32>(Bullets.size()))
			{
				continue;
			}

			Bullets[BulletIndex].RenderSlotIndex = SlotIndex;
			Bullets[BulletIndex].RenderInstanceIndex = RenderIndex;
			Transforms.push_back(MakeBulletRenderTransform(Bullets[BulletIndex]));
		}

		Renderer->SetInstances(std::move(Transforms));
	}

	UpdateRenderDebugStats();
}

void UBulletHellComponent::ClearRenderer()
{
	for (FBulletRenderSlot& Slot : RenderSlots)
	{
		Slot.BulletIndices.clear();
		if (UInstancedStaticMeshComponent* Renderer = Slot.Renderer.Get())
		{
			Renderer->ClearInstances();
		}
	}

	for (FBulletInstance& Bullet : Bullets)
	{
		Bullet.RenderSlotIndex = -1;
		Bullet.RenderInstanceIndex = -1;
	}
	UpdateRenderDebugStats();
}

FTransform UBulletHellComponent::MakeBulletRenderTransform(const FBulletInstance& Bullet) const
{
	FVector RenderPosition = Bullet.Position;
	FVector RenderVelocity = Bullet.Velocity;
	if (const UInstancedStaticMeshComponent* Renderer = GetRenderSlotComponent(Bullet.RenderSlotIndex))
	{
		const FMatrix RendererWorldInverse = Renderer->GetWorldInverseMatrix();
		RenderPosition = RendererWorldInverse.TransformPositionWithW(Bullet.Position);
		RenderVelocity = RendererWorldInverse.TransformVector(Bullet.Velocity);
	}

	FRotator Rotation = FRotator::ZeroRotator;
	if (RenderOrientationMode == EBulletHellRenderOrientationMode::VelocityYaw && !RenderVelocity.IsNearlyZero())
	{
		const float YawDegrees = std::atan2(RenderVelocity.Y, RenderVelocity.X) * (180.0f / 3.1415926535f);
		Rotation = FRotator(0.0f, YawDegrees, 0.0f);
	}

	const float Scale = (std::max)(0.01f, Bullet.RenderScale);
	return FTransform(
		RenderPosition,
		Rotation,
		FVector(Scale, Scale, Scale));
}

void UBulletHellComponent::UpdateRenderDebugStats()
{
	DebugStats.ActiveBulletCount = static_cast<int32>(Bullets.size());
	DebugStats.RendererSlotCount = 0;
	DebugStats.RenderInstanceCount = 0;
	DebugStats.RendererSlot0InstanceCount = 0;
	DebugStats.RendererSlot1InstanceCount = 0;
	if (!bEnableRendering)
	{
		DebugStats.RenderMismatchCount = 0;
		return;
	}

	int32 MismatchCount = 0;
	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(RenderSlots.size()); ++SlotIndex)
	{
		const FBulletRenderSlot& Slot = RenderSlots[SlotIndex];
		UInstancedStaticMeshComponent* Renderer = GetRenderSlotComponent(SlotIndex);
		if (!Renderer)
		{
			if (!Slot.BulletIndices.empty())
			{
				++MismatchCount;
			}
			continue;
		}

		++DebugStats.RendererSlotCount;
		const int32 SlotInstanceCount = Renderer->GetInstanceCount();
		DebugStats.RenderInstanceCount += SlotInstanceCount;
		if (SlotIndex == 0)
		{
			DebugStats.RendererSlot0InstanceCount = SlotInstanceCount;
		}
		else if (SlotIndex == 1)
		{
			DebugStats.RendererSlot1InstanceCount = SlotInstanceCount;
		}
		if (SlotInstanceCount != static_cast<int32>(Slot.BulletIndices.size()))
		{
			++MismatchCount;
		}

		for (int32 RenderIndex = 0; RenderIndex < static_cast<int32>(Slot.BulletIndices.size()); ++RenderIndex)
		{
			const int32 BulletIndex = Slot.BulletIndices[RenderIndex];
			if (BulletIndex < 0 || BulletIndex >= static_cast<int32>(Bullets.size()))
			{
				++MismatchCount;
				continue;
			}

			const FBulletInstance& Bullet = Bullets[BulletIndex];
			if (Bullet.RenderSlotIndex != SlotIndex || Bullet.RenderInstanceIndex != RenderIndex)
			{
				++MismatchCount;
			}
		}
	}

	for (const FBulletInstance& Bullet : Bullets)
	{
		if (bEnableRendering && (Bullet.RenderSlotIndex < 0 || Bullet.RenderInstanceIndex < 0))
		{
			++MismatchCount;
		}
	}

	DebugStats.RenderMismatchCount = MismatchCount;
}

void UBulletHellComponent::ResetPerFrameDebugStats()
{
	DebugStats.ActiveBulletCount = static_cast<int32>(Bullets.size());
}

void UBulletHellComponent::RecordOverlayStats() const
{
	FBulletHellStatsSnapshot Snapshot;
	Snapshot.ActiveBulletCount = static_cast<uint32>((std::max)(0, DebugStats.ActiveBulletCount));
	Snapshot.TotalSpawned = DebugStats.TotalSpawned;
	Snapshot.TotalKilled = DebugStats.TotalKilled;
	Snapshot.TotalExpired = DebugStats.TotalExpired;
	Snapshot.CollisionQueryCount = DebugStats.CollisionQueryCount;
	Snapshot.CollisionHitCount = DebugStats.CollisionHitCount;
	Snapshot.CollisionKilledCount = DebugStats.CollisionKilledCount;
	Snapshot.EraseKilledCount = DebugStats.EraseKilledCount;
	Snapshot.BehaviorTransitionCount = DebugStats.BehaviorTransitionCount;
	Snapshot.ActiveLinearCount = static_cast<uint32>((std::max)(0, DebugStats.ActiveLinearCount));
	Snapshot.ActiveHomingCount = static_cast<uint32>((std::max)(0, DebugStats.ActiveHomingCount));
	Snapshot.ActiveColdLaunchCount = static_cast<uint32>((std::max)(0, DebugStats.ActiveColdLaunchCount));
	Snapshot.ActiveTimedVelocityChangeCount = static_cast<uint32>((std::max)(0, DebugStats.ActiveTimedVelocityChangeCount));
	Snapshot.ActivePrimaryArchetypeCount = static_cast<uint32>((std::max)(0, DebugStats.ActivePrimaryArchetypeCount));
	Snapshot.ActiveSecondaryArchetypeCount = static_cast<uint32>((std::max)(0, DebugStats.ActiveSecondaryArchetypeCount));
	Snapshot.DebugDrawSelectedCount = static_cast<uint32>((std::max)(0, DebugStats.DebugDrawSelectedCount));
	Snapshot.DebugDrawTruncatedCount = static_cast<uint32>((std::max)(0, DebugStats.DebugDrawTruncatedCount));
	Snapshot.RenderInstanceCount = static_cast<uint32>((std::max)(0, DebugStats.RenderInstanceCount));
	Snapshot.RendererSlotCount = static_cast<uint32>((std::max)(0, DebugStats.RendererSlotCount));
	Snapshot.RendererSlot0InstanceCount = static_cast<uint32>((std::max)(0, DebugStats.RendererSlot0InstanceCount));
	Snapshot.RendererSlot1InstanceCount = static_cast<uint32>((std::max)(0, DebugStats.RendererSlot1InstanceCount));
	Snapshot.RenderMismatchCount = static_cast<uint32>((std::max)(0, DebugStats.RenderMismatchCount));
	BULLETHELL_STATS_ADD_COMPONENT(Snapshot);
}

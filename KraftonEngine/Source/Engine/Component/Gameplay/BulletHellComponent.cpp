#include "BulletHellComponent.h"

#include "Component/Primitive/InstancedStaticMeshComponent.h"
#include "Core/Logging/Log.h"
#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/Rotator.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
	constexpr float TwoPi = 6.28318530718f;

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

	FVector RotateDirectionYaw(const FVector& Forward, const FVector& Right, float Degrees)
	{
		const float Radians = Degrees * (3.1415926535f / 180.0f);
		return SafeDirection(Forward * std::cos(Radians) + Right * std::sin(Radians), Forward);
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

	if (bAutoSpawnDebugPreset)
	{
		SpawnDebugPreset();
	}

	if (bEnableRendering)
	{
		EnsureRenderComponent();
		RebuildRendererFromBullets();
	}
}

void UBulletHellComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);

	if (IsProperty(PropertyName, "DebugSpawnRequest", "Debug Spawn Request") &&
		DebugSpawnRequest != LastDebugSpawnRequest)
	{
		LastDebugSpawnRequest = DebugSpawnRequest;
		const int32 SpawnedCount = SpawnDebugPreset();
		UE_LOG("BulletHell debug spawn request: Spawned=%d %s", SpawnedCount, GetBulletDebugStatsText().c_str());
	}
	else if (IsProperty(PropertyName, "DebugRandomKillRequest", "Debug Random Kill Request") &&
		DebugRandomKillRequest != LastDebugRandomKillRequest)
	{
		LastDebugRandomKillRequest = DebugRandomKillRequest;
		const bool bKilled = KillRandomDebugBullet();
		UE_LOG("BulletHell debug random kill request: Killed=%s %s", bKilled ? "true" : "false", GetBulletDebugStatsText().c_str());
	}
	else if (IsProperty(PropertyName, "DebugClearRequest", "Debug Clear Request") &&
		DebugClearRequest != LastDebugClearRequest)
	{
		LastDebugClearRequest = DebugClearRequest;
		ClearBullets();
		UE_LOG("BulletHell debug clear request: %s", GetBulletDebugStatsText().c_str());
	}
	else if (IsProperty(PropertyName, "DebugLogStatsRequest", "Debug Log Stats Request") &&
		DebugLogStatsRequest != LastDebugLogStatsRequest)
	{
		LastDebugLogStatsRequest = DebugLogStatsRequest;
		LogBulletDebugStats();
	}
	else if (IsProperty(PropertyName, "DebugLogFirstBulletRequest", "Debug Log First Bullet Request") &&
		DebugLogFirstBulletRequest != LastDebugLogFirstBulletRequest)
	{
		LastDebugLogFirstBulletRequest = DebugLogFirstBulletRequest;
		LogFirstBulletDebugInfo();
	}
	else if (IsProperty(PropertyName, "DebugRebuildRendererRequest", "Debug Rebuild Renderer Request") &&
		DebugRebuildRendererRequest != LastDebugRebuildRendererRequest)
	{
		LastDebugRebuildRendererRequest = DebugRebuildRendererRequest;
		RebuildRendererFromBullets();
		UE_LOG("BulletHell debug rebuild renderer request: %s", GetBulletDebugStatsText().c_str());
	}
	else if (IsProperty(PropertyName, "bLogDebugStats", "Log Debug Stats") ||
		IsProperty(PropertyName, "DebugStatsLogInterval", "Debug Stats Log Interval"))
	{
		DebugStatsLogAccumulator = 0.0f;
	}
	else if (IsProperty(PropertyName, "RendererMeshPath", "Renderer Mesh Path") ||
		IsProperty(PropertyName, "RendererMaterialPath", "Renderer Material Path") ||
		IsProperty(PropertyName, "bEnableRendering", "Enable Rendering") ||
		IsProperty(PropertyName, "bAutoCreateRenderer", "Auto Create Renderer"))
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

	if (bDrawBulletDebug)
	{
		DrawBulletDebug();
	}

	MaybeLogBulletDebugStats(DeltaTime);
}

FBulletHandle UBulletHellComponent::SpawnBullet(
	const FVector& Position,
	const FVector& Velocity,
	float Radius,
	float Lifetime)
{
	return SpawnBulletInternal(
		Position,
		Velocity,
		Radius,
		Lifetime,
		EBulletBehaviorType::Linear,
		SafeDirection(Velocity, ResolveDebugSpawnForward()));
}

FBulletHandle UBulletHellComponent::SpawnBulletInternal(
	const FVector& Position,
	const FVector& Velocity,
	float Radius,
	float Lifetime,
	EBulletBehaviorType BehaviorType,
	const FVector& DebugDirection)
{
	FBulletInstance Bullet;
	Bullet.Id = NextBulletId;
	Bullet.Generation = NextBulletGeneration;
	Bullet.Position = Position;
	Bullet.PreviousPosition = Position;
	Bullet.Velocity = Velocity;
	Bullet.Radius = (std::max)(0.01f, Radius);
	Bullet.Age = 0.0f;
	Bullet.Lifetime = Lifetime;
	Bullet.BehaviorType = BehaviorType;
	Bullet.BehaviorPhase = BehaviorType == EBulletBehaviorType::ColdLaunch ? EBulletPhase::Waiting : EBulletPhase::Active;
	Bullet.RenderInstanceIndex = -1;
	Bullet.bAlive = true;
	ConfigureDebugBulletBehavior(Bullet, DebugDirection);

	NextBulletId = AdvanceNonZero(NextBulletId);
	NextBulletGeneration = AdvanceNonZero(NextBulletGeneration);

	const int32 NewIndex = static_cast<int32>(Bullets.size());
	BulletIndexById[Bullet.Id] = NewIndex;
	Bullets.push_back(Bullet);
	if (UInstancedStaticMeshComponent* Renderer = EnsureRenderComponent())
	{
		Bullets.back().RenderInstanceIndex = Renderer->AddInstance(MakeBulletRenderTransform(Bullets.back()));
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

bool UBulletHellComponent::KillRandomDebugBullet()
{
	if (Bullets.empty())
	{
		return false;
	}

	DebugKillRandomState = DebugKillRandomState * 1664525u + 1013904223u;
	const int32 BulletIndex = static_cast<int32>(DebugKillRandomState % static_cast<uint32>(Bullets.size()));
	return RemoveBulletAtIndex(BulletIndex, false);
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

int32 UBulletHellComponent::SpawnDebugPreset()
{
	const int32 SafeCount = (std::max)(0, DebugSpawnCount);
	if (SafeCount == 0)
	{
		return 0;
	}

	const FVector Origin = ResolveDebugSpawnOrigin();
	const FVector Forward = ResolveDebugSpawnForward();
	const FVector Right = ResolveDebugSpawnRight();
	const float Spacing = (std::max)(DebugSpawnRadius * 3.0f, 1.0f);
	const float PatternRadius = (std::max)(Spacing, static_cast<float>(SafeCount) * Spacing / TwoPi);

	for (int32 Index = 0; Index < SafeCount; ++Index)
	{
		FVector Position = Origin;
		FVector Direction = Forward;

		switch (DebugSpawnPattern)
		{
		case EBulletHellDebugSpawnPattern::Line:
		{
			const float Offset = (static_cast<float>(Index) - static_cast<float>(SafeCount - 1) * 0.5f) * Spacing;
			Position = Origin + Right * Offset;
			Direction = Forward;
			break;
		}
		case EBulletHellDebugSpawnPattern::Ring:
		{
			const float Angle = SafeCount > 0 ? (TwoPi * static_cast<float>(Index) / static_cast<float>(SafeCount)) : 0.0f;
			const FVector Radial = SafeDirection(Forward * std::cos(Angle) + Right * std::sin(Angle), Forward);
			Position = Origin + Radial * PatternRadius;
			Direction = Forward;
			break;
		}
		case EBulletHellDebugSpawnPattern::Radial:
		default:
		{
			const float Angle = SafeCount > 0 ? (TwoPi * static_cast<float>(Index) / static_cast<float>(SafeCount)) : 0.0f;
			Direction = SafeDirection(Forward * std::cos(Angle) + Right * std::sin(Angle), Forward);
			Position = Origin;
			break;
		}
		}

		FVector Velocity = Direction * DebugSpawnSpeed;
		if (DebugBehaviorType == EBulletBehaviorType::ColdLaunch)
		{
			Velocity = FVector::ZeroVector;
		}

		SpawnBulletInternal(
			Position,
			Velocity,
			DebugSpawnRadius,
			DebugSpawnLifetime,
			DebugBehaviorType,
			Direction);
	}

	return SafeCount;
}

void UBulletHellComponent::ConfigureDebugBulletBehavior(FBulletInstance& Bullet, const FVector& Direction) const
{
	const FVector SafeForward = SafeDirection(Direction, ResolveDebugSpawnForward());
	const FVector Right = ResolveDebugSpawnRight();
	const FVector Up = FVector::UpVector;

	switch (Bullet.BehaviorType)
	{
	case EBulletBehaviorType::Homing:
		Bullet.HomingTargetPosition =
			ResolveDebugSpawnOrigin()
			+ SafeForward * DebugHomingTargetForwardOffset
			+ Right * DebugHomingTargetRightOffset
			+ Up * DebugHomingTargetUpOffset;
		Bullet.HomingStrength = (std::max)(0.0f, DebugHomingStrength);
		Bullet.HomingMaxTurnRateDegrees = (std::max)(0.0f, DebugHomingMaxTurnRateDegrees);
		break;
	case EBulletBehaviorType::ColdLaunch:
		Bullet.BehaviorPhase = EBulletPhase::Waiting;
		Bullet.ColdLaunchDelay = (std::max)(0.0f, DebugColdLaunchDelay);
		Bullet.ColdLaunchVelocity = SafeForward * (std::max)(0.0f, DebugColdLaunchSpeed);
		break;
	case EBulletBehaviorType::TimedVelocityChange:
		Bullet.TimedActivationTime = (std::max)(0.0f, DebugTimedActivationTime);
		Bullet.TimedVelocity = RotateDirectionYaw(SafeForward, Right, DebugTimedYawDegrees) * (std::max)(0.0f, DebugTimedSpeed);
		break;
	case EBulletBehaviorType::Linear:
	default:
		break;
	}
}

void UBulletHellComponent::LogBulletDebugStats() const
{
	UE_LOG("%s", GetBulletDebugStatsText().c_str());
}

void UBulletHellComponent::LogFirstBulletDebugInfo() const
{
	if (Bullets.empty())
	{
		UE_LOG("BulletHell first bullet: None");
		return;
	}

	const FBulletInstance& Bullet = Bullets.front();
	UE_LOG(
		"BulletHell first bullet: Id=%u Generation=%u Position=(%.2f,%.2f,%.2f) Previous=(%.2f,%.2f,%.2f) Velocity=(%.2f,%.2f,%.2f) Radius=%.2f Age=%.2f Lifetime=%.2f Behavior=%d Phase=%d RenderIndex=%d",
		Bullet.Id,
		Bullet.Generation,
		Bullet.Position.X,
		Bullet.Position.Y,
		Bullet.Position.Z,
		Bullet.PreviousPosition.X,
		Bullet.PreviousPosition.Y,
		Bullet.PreviousPosition.Z,
		Bullet.Velocity.X,
		Bullet.Velocity.Y,
		Bullet.Velocity.Z,
		Bullet.Radius,
		Bullet.Age,
		Bullet.Lifetime,
		static_cast<int32>(Bullet.BehaviorType),
		static_cast<int32>(Bullet.BehaviorPhase),
		Bullet.RenderInstanceIndex);
}

FString UBulletHellComponent::GetBulletDebugStatsText() const
{
	char Buffer[768];
	std::snprintf(
		Buffer,
		sizeof(Buffer),
		"BulletHell stats: Active=%d Spawned=%u Killed=%u Expired=%u CollisionQueries=%u CollisionHits=%u CollisionKilled=%u EraseKilled=%u BehaviorTransitions=%u BehaviorActive(L/H/C/T)=%d/%d/%d/%d DebugDrawSelected=%d DebugDrawTruncated=%d RenderInstances=%d RendererSlots=%d RenderMismatch=%d",
		DebugStats.ActiveBulletCount,
		DebugStats.TotalSpawned,
		DebugStats.TotalKilled,
		DebugStats.TotalExpired,
		DebugStats.CollisionQueryCount,
		DebugStats.CollisionHitCount,
		DebugStats.CollisionKilledCount,
		DebugStats.EraseKilledCount,
		DebugStats.BehaviorTransitionCount,
		DebugStats.ActiveLinearCount,
		DebugStats.ActiveHomingCount,
		DebugStats.ActiveColdLaunchCount,
		DebugStats.ActiveTimedVelocityChangeCount,
		DebugStats.DebugDrawSelectedCount,
		DebugStats.DebugDrawTruncatedCount,
		DebugStats.RenderInstanceCount,
		DebugStats.RendererSlotCount,
		DebugStats.RenderMismatchCount);
	return FString(Buffer);
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

	for (const FBulletInstance& Bullet : Bullets)
	{
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
		return EBulletCollisionKillReason::Collision;
	}

	if (bKillOnBlockingCollision && SweepBulletByChannel(Bullet, CollisionTraceChannel, Hit))
	{
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

	DrawCollisionSweepDebug(Bullet, bHit ? &OutHit : nullptr, false);
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

	DrawCollisionSweepDebug(Bullet, bHit ? &OutHit : nullptr, ObjectTypeMask == BuildEraseObjectTypeMask());
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

void UBulletHellComponent::DrawCollisionSweepDebug(
	const FBulletInstance& Bullet,
	const FHitResult* Hit,
	bool bErase) const
{
	if (!bDrawCollisionDebug)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FColor MissColor = bErase ? FColor(120, 120, 255) : FColor::Gray();
	const FColor HitColor = bErase ? FColor(255, 0, 255) : FColor::Red();
	DrawDebugLine(World, Bullet.PreviousPosition, Bullet.Position, Hit ? HitColor : MissColor, 0.0f);
	if (Hit)
	{
		DrawDebugSphere(World, Hit->WorldHitLocation, (std::max)(Bullet.Radius, 1.0f), 12, HitColor, 0.0f);
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
	const int32 RemovedRenderIndex = Bullets[BulletIndex].RenderInstanceIndex;
	int32 MovedRenderToIndex = -1;
	if (UInstancedStaticMeshComponent* Renderer = GetRenderComponent())
	{
		MovedRenderToIndex = Renderer->RemoveInstanceSwap(RemovedRenderIndex);
	}
	BulletIndexById.erase(RemovedId);

	if (BulletIndex != LastIndex)
	{
		Bullets[BulletIndex] = Bullets[LastIndex];
		if (MovedRenderToIndex >= 0)
		{
			Bullets[BulletIndex].RenderInstanceIndex = MovedRenderToIndex;
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
	if (!bEnableRendering)
	{
		return nullptr;
	}

	UInstancedStaticMeshComponent* Renderer = RenderComponent.Get();
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

	Renderer->SetFName(FName("BulletHellRenderer"));
	if (USceneComponent* RootComponent = OwnerActor->GetRootComponent())
	{
		Renderer->AttachToComponent(RootComponent);
	}

	RenderComponent = Renderer;
	ApplyRenderAssets();
	UpdateRenderDebugStats();
	return Renderer;
}

UInstancedStaticMeshComponent* UBulletHellComponent::GetRenderComponent() const
{
	if (!bEnableRendering)
	{
		return nullptr;
	}

	UInstancedStaticMeshComponent* Renderer = RenderComponent.Get();
	return Renderer && Renderer->GetOwner() == GetOwner() ? Renderer : nullptr;
}

void UBulletHellComponent::ApplyRenderAssets()
{
	UInstancedStaticMeshComponent* Renderer = GetRenderComponent();
	if (!Renderer)
	{
		return;
	}

	if (!RendererMeshPath.empty() && RendererMeshPath != "None")
	{
		Renderer->SetStaticMeshByPath(RendererMeshPath);
	}

	if (!RendererMaterialPath.empty() && RendererMaterialPath != "None" && Renderer->GetMaterialSlotCount() > 0)
	{
		Renderer->SetMaterialByPath(0, RendererMaterialPath);
	}
}

void UBulletHellComponent::RebuildRendererFromBullets()
{
	if (!bEnableRendering)
	{
		ClearRenderer();
		return;
	}

	UInstancedStaticMeshComponent* Renderer = EnsureRenderComponent();
	if (!Renderer)
	{
		for (FBulletInstance& Bullet : Bullets)
		{
			Bullet.RenderInstanceIndex = -1;
		}
		UpdateRenderDebugStats();
		return;
	}

	TArray<FTransform> Transforms;
	Transforms.reserve(Bullets.size());
	for (int32 Index = 0; Index < static_cast<int32>(Bullets.size()); ++Index)
	{
		Bullets[Index].RenderInstanceIndex = Index;
		Transforms.push_back(MakeBulletRenderTransform(Bullets[Index]));
	}

	Renderer->SetInstances(std::move(Transforms));
	UpdateRenderDebugStats();
}

void UBulletHellComponent::SyncRenderInstancesBulk()
{
	if (!bEnableRendering)
	{
		ClearRenderer();
		return;
	}

	UInstancedStaticMeshComponent* Renderer = EnsureRenderComponent();
	if (!Renderer)
	{
		UpdateRenderDebugStats();
		return;
	}

	TArray<FTransform> Transforms;
	Transforms.reserve(Bullets.size());
	for (int32 Index = 0; Index < static_cast<int32>(Bullets.size()); ++Index)
	{
		Bullets[Index].RenderInstanceIndex = Index;
		Transforms.push_back(MakeBulletRenderTransform(Bullets[Index]));
	}

	Renderer->SetInstances(std::move(Transforms));
	UpdateRenderDebugStats();
}

void UBulletHellComponent::ClearRenderer()
{
	if (UInstancedStaticMeshComponent* Renderer = RenderComponent.Get())
	{
		Renderer->ClearInstances();
	}

	for (FBulletInstance& Bullet : Bullets)
	{
		Bullet.RenderInstanceIndex = -1;
	}
	UpdateRenderDebugStats();
}

FTransform UBulletHellComponent::MakeBulletRenderTransform(const FBulletInstance& Bullet) const
{
	FVector RenderPosition = Bullet.Position;
	FVector RenderVelocity = Bullet.Velocity;
	if (const UInstancedStaticMeshComponent* Renderer = GetRenderComponent())
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

	const float Scale = (std::max)(0.01f, RenderScale);
	return FTransform(
		RenderPosition,
		Rotation,
		FVector(Scale, Scale, Scale));
}

void UBulletHellComponent::UpdateRenderDebugStats()
{
	DebugStats.ActiveBulletCount = static_cast<int32>(Bullets.size());
	UInstancedStaticMeshComponent* Renderer = GetRenderComponent();
	DebugStats.RendererSlotCount = Renderer ? 1 : 0;
	DebugStats.RenderInstanceCount = Renderer ? Renderer->GetInstanceCount() : 0;

	int32 MismatchCount = 0;
	if (Renderer)
	{
		if (Renderer->GetInstanceCount() != static_cast<int32>(Bullets.size()))
		{
			++MismatchCount;
		}

		for (int32 Index = 0; Index < static_cast<int32>(Bullets.size()); ++Index)
		{
			if (Bullets[Index].RenderInstanceIndex != Index)
			{
				++MismatchCount;
			}
		}
	}
	else if (bEnableRendering && !Bullets.empty())
	{
		++MismatchCount;
	}

	DebugStats.RenderMismatchCount = MismatchCount;
}

void UBulletHellComponent::DrawBulletDebug()
{
	if (DebugDrawMode == EBulletHellDebugDrawMode::Off)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const int32 MaxDrawCount = (std::max)(0, DebugDrawMaxCount);
	int32 EligibleCount = 0;
	int32 DrawnCount = 0;

	for (const FBulletInstance& Bullet : Bullets)
	{
		const bool bHighlighted = HighlightedBulletId > 0 && Bullet.Id == static_cast<uint32>(HighlightedBulletId);
		if (DebugDrawMode == EBulletHellDebugDrawMode::Highlighted && !bHighlighted)
		{
			continue;
		}

		++EligibleCount;
		if (DrawnCount >= MaxDrawCount)
		{
			continue;
		}

		const FColor Color = bHighlighted ? FColor::Yellow() : FColor(0, 210, 255);
		DrawBulletCross(Bullet.Position, Color, (std::max)(Bullet.Radius * 0.35f, 0.1f));
		DrawDebugSphere(World, Bullet.Position, Bullet.Radius, 12, Color, 0.0f);
		if (!Bullet.PreviousPosition.IsNearlyZero() || !Bullet.Position.IsNearlyZero())
		{
			DrawDebugLine(World, Bullet.PreviousPosition, Bullet.Position, FColor(255, 140, 0), 0.0f);
		}
		++DrawnCount;
	}

	DebugStats.DebugDrawSelectedCount = DrawnCount;
	DebugStats.DebugDrawTruncatedCount = (std::max)(0, EligibleCount - DrawnCount);
}

void UBulletHellComponent::DrawBulletCross(const FVector& Center, const FColor& Color, float Extent) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	DrawDebugLine(World, Center - FVector(Extent, 0.0f, 0.0f), Center + FVector(Extent, 0.0f, 0.0f), Color, 0.0f);
	DrawDebugLine(World, Center - FVector(0.0f, Extent, 0.0f), Center + FVector(0.0f, Extent, 0.0f), Color, 0.0f);
	DrawDebugLine(World, Center - FVector(0.0f, 0.0f, Extent), Center + FVector(0.0f, 0.0f, Extent), Color, 0.0f);
}

FVector UBulletHellComponent::ResolveDebugSpawnOrigin() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
}

FVector UBulletHellComponent::ResolveDebugSpawnForward() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? SafeDirection(OwnerActor->GetActorForward(), FVector::ForwardVector) : FVector::ForwardVector;
}

FVector UBulletHellComponent::ResolveDebugSpawnRight() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? SafeDirection(OwnerActor->GetActorRight(), FVector::RightVector) : FVector::RightVector;
}

void UBulletHellComponent::ResetPerFrameDebugStats()
{
	DebugStats.ActiveBulletCount = static_cast<int32>(Bullets.size());
	DebugStats.DebugDrawSelectedCount = 0;
	DebugStats.DebugDrawTruncatedCount = 0;
}

void UBulletHellComponent::MaybeLogBulletDebugStats(float DeltaTime)
{
	if (!bLogDebugStats)
	{
		DebugStatsLogAccumulator = 0.0f;
		return;
	}

	const float SafeInterval = (std::max)(0.0f, DebugStatsLogInterval);
	DebugStatsLogAccumulator += (std::max)(0.0f, DeltaTime);
	if (DebugStatsLogAccumulator < SafeInterval)
	{
		return;
	}

	DebugStatsLogAccumulator = 0.0f;
	LogBulletDebugStats();
}

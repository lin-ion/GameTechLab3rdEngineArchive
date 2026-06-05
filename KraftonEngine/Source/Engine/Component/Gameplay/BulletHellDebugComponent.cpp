#include "BulletHellDebugComponent.h"

#include "Core/Logging/Log.h"
#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
	constexpr float TwoPi = 6.28318530718f;

	FVector SafeDirection(const FVector& Direction, const FVector& Fallback)
	{
		return Direction.IsNearlyZero() ? Fallback : Direction.Normalized();
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

UBulletHellDebugComponent::UBulletHellDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bTickEnabled = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBulletHellDebugComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	if (bAutoSpawnDebugPreset)
	{
		SpawnDebugPreset();
	}
}

void UBulletHellDebugComponent::PostEditProperty(const char* PropertyName)
{
	UActorComponent::PostEditProperty(PropertyName);

	UBulletHellComponent* BulletHell = ResolveBulletHellComponent();
	if (!BulletHell)
	{
		return;
	}

	if (IsProperty(PropertyName, "DebugSpawnRequest", "Debug Spawn Request") &&
		DebugSpawnRequest != LastDebugSpawnRequest)
	{
		LastDebugSpawnRequest = DebugSpawnRequest;
		const int32 SpawnedCount = SpawnDebugPreset();
		UE_LOG("BulletHell debug spawn request: Spawned=%d", SpawnedCount);
	}
	else if (IsProperty(PropertyName, "DebugStressSpawnRequest", "Debug Stress Spawn Request") &&
		DebugStressSpawnRequest != LastDebugStressSpawnRequest)
	{
		LastDebugStressSpawnRequest = DebugStressSpawnRequest;
		const int32 SpawnedCount = SpawnDebugStressPreset();
		UE_LOG("BulletHell debug stress spawn request: Spawned=%d", SpawnedCount);
	}
	else if (IsProperty(PropertyName, "DebugRandomKillRequest", "Debug Random Kill Request") &&
		DebugRandomKillRequest != LastDebugRandomKillRequest)
	{
		LastDebugRandomKillRequest = DebugRandomKillRequest;
		const bool bKilled = KillRandomDebugBullet();
		UE_LOG("BulletHell debug random kill request: Killed=%s", bKilled ? "true" : "false");
	}
	else if (IsProperty(PropertyName, "DebugClearRequest", "Debug Clear Request") &&
		DebugClearRequest != LastDebugClearRequest)
	{
		LastDebugClearRequest = DebugClearRequest;
		BulletHell->ClearBullets();
		UE_LOG("BulletHell debug clear request");
	}
	else if (IsProperty(PropertyName, "DebugLogFirstBulletRequest", "Debug Log First Bullet Request") &&
		DebugLogFirstBulletRequest != LastDebugLogFirstBulletRequest)
	{
		LastDebugLogFirstBulletRequest = DebugLogFirstBulletRequest;
		LogFirstBulletDebugInfo();
	}
	else if (IsProperty(PropertyName, "DebugSampleBossPatternRequest", "Debug Sample Boss Pattern Request") &&
		DebugSampleBossPatternRequest != LastDebugSampleBossPatternRequest)
	{
		LastDebugSampleBossPatternRequest = DebugSampleBossPatternRequest;
		const int32 SpawnedCount = SpawnSampleBossPattern();
		UE_LOG("BulletHell sample boss pattern request: Spawned=%d", SpawnedCount);
	}
}

void UBulletHellDebugComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction& ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
	(void)DeltaTime;
	(void)TickType;
	(void)ThisTickFunction;

	DrawBulletDebug();
}

UBulletHellComponent* UBulletHellDebugComponent::ResolveBulletHellComponent() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->GetComponentByClass<UBulletHellComponent>() : nullptr;
}

FBulletArchetype UBulletHellDebugComponent::BuildDebugArchetype(int32 ArchetypeIndex) const
{
	FBulletArchetype Archetype;
	if (ArchetypeIndex == 1)
	{
		Archetype.MeshPath = SecondaryMeshPath;
		Archetype.MaterialPath = SecondaryMaterialPath;
		Archetype.Radius = (std::max)(0.01f, SecondaryRadius);
		Archetype.Speed = (std::max)(0.0f, SecondarySpeed);
		Archetype.Lifetime = SecondaryLifetime;
		Archetype.RenderScale = (std::max)(0.01f, SecondaryRenderScale);
		Archetype.BehaviorType = SecondaryBehaviorType;
		return Archetype;
	}

	Archetype.MeshPath = PrimaryMeshPath;
	Archetype.MaterialPath = PrimaryMaterialPath;
	Archetype.Radius = (std::max)(0.01f, DebugSpawnRadius);
	Archetype.Speed = (std::max)(0.0f, DebugSpawnSpeed);
	Archetype.Lifetime = DebugSpawnLifetime;
	Archetype.RenderScale = (std::max)(0.01f, PrimaryRenderScale);
	Archetype.BehaviorType = DebugBehaviorType;
	return Archetype;
}

int32 UBulletHellDebugComponent::ResolveDebugArchetypeIndex(int32 SpawnIndex) const
{
	switch (DebugArchetypeMode)
	{
	case EBulletHellDebugArchetypeMode::Secondary:
		return 1;
	case EBulletHellDebugArchetypeMode::Alternating:
		return (SpawnIndex % 2) == 0 ? 0 : 1;
	case EBulletHellDebugArchetypeMode::Primary:
	default:
		return 0;
	}
}

FBulletSpawnParams UBulletHellDebugComponent::BuildSpawnParams(
	const FVector& Position,
	const FVector& Direction,
	int32 ArchetypeIndex,
	const FBulletArchetype& Archetype) const
{
	const FVector Forward = SafeDirection(Direction, ResolveDebugSpawnForward());
	const FVector Right = ResolveDebugSpawnRight();
	const FVector Up = FVector::UpVector;

	FBulletSpawnParams Params;
	Params.Position = Position;
	Params.Archetype = Archetype;
	Params.ArchetypeIndex = ArchetypeIndex;
	Params.BehaviorType = Archetype.BehaviorType;
	Params.Velocity = Forward * Archetype.Speed;

	switch (Params.BehaviorType)
	{
	case EBulletBehaviorType::Homing:
		Params.HomingTargetPosition =
			ResolveDebugSpawnOrigin()
			+ Forward * DebugHomingTargetForwardOffset
			+ Right * DebugHomingTargetRightOffset
			+ Up * DebugHomingTargetUpOffset;
		Params.HomingStrength = DebugHomingStrength;
		Params.HomingMaxTurnRateDegrees = DebugHomingMaxTurnRateDegrees;
		break;
	case EBulletBehaviorType::ColdLaunch:
		Params.Velocity = Forward * (std::max)(0.0f, DebugColdLaunchInitialSpeed);
		Params.ColdLaunchDelay = DebugColdLaunchDelay;
		Params.ColdLaunchVelocity = Forward * (std::max)(0.0f, DebugColdLaunchSpeed > 0.0f ? DebugColdLaunchSpeed : Archetype.Speed);
		break;
	case EBulletBehaviorType::TimedVelocityChange:
		Params.TimedActivationTime = DebugTimedActivationTime;
		Params.TimedVelocity = RotateDirectionYaw(Forward, Right, DebugTimedYawDegrees) * (std::max)(0.0f, DebugTimedSpeed);
		break;
	case EBulletBehaviorType::Linear:
	default:
		break;
	}

	return Params;
}

int32 UBulletHellDebugComponent::SpawnDebugPreset()
{
	UBulletHellComponent* BulletHell = ResolveBulletHellComponent();
	if (!BulletHell)
	{
		return 0;
	}

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

		const int32 ArchetypeIndex = ResolveDebugArchetypeIndex(Index);
		const FBulletArchetype Archetype = BuildDebugArchetype(ArchetypeIndex);
		BulletHell->SpawnBullet(BuildSpawnParams(Position, Direction, ArchetypeIndex, Archetype));
	}

	return SafeCount;
}

int32 UBulletHellDebugComponent::SpawnDebugStressPreset()
{
	UBulletHellComponent* BulletHell = ResolveBulletHellComponent();
	if (!BulletHell)
	{
		return 0;
	}

	if (bDebugStressClearBeforeSpawn)
	{
		BulletHell->ClearBullets();
	}

	const int32 PreviousDebugSpawnCount = DebugSpawnCount;
	const EBulletHellDebugArchetypeMode PreviousArchetypeMode = DebugArchetypeMode;
	DebugSpawnCount = (std::max)(0, DebugStressSpawnCount);
	DebugArchetypeMode = EBulletHellDebugArchetypeMode::Alternating;
	const int32 SpawnedCount = SpawnDebugPreset();
	DebugArchetypeMode = PreviousArchetypeMode;
	DebugSpawnCount = PreviousDebugSpawnCount;
	return SpawnedCount;
}

bool UBulletHellDebugComponent::KillRandomDebugBullet()
{
	UBulletHellComponent* BulletHell = ResolveBulletHellComponent();
	if (!BulletHell)
	{
		return false;
	}

	const TArray<FBulletInstance>& Bullets = BulletHell->GetBulletInstances();
	if (Bullets.empty())
	{
		return false;
	}

	DebugKillRandomState = DebugKillRandomState * 1664525u + 1013904223u;
	const int32 BulletIndex = static_cast<int32>(DebugKillRandomState % static_cast<uint32>(Bullets.size()));
	const FBulletInstance& Bullet = Bullets[BulletIndex];
	return BulletHell->KillBullet(FBulletHandle{ Bullet.Id, Bullet.Generation });
}

void UBulletHellDebugComponent::LogFirstBulletDebugInfo() const
{
	const UBulletHellComponent* BulletHell = ResolveBulletHellComponent();
	if (!BulletHell || BulletHell->GetBulletInstances().empty())
	{
		UE_LOG("BulletHell first bullet: None");
		return;
	}

	const FBulletInstance& Bullet = BulletHell->GetBulletInstances().front();
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

int32 UBulletHellDebugComponent::SpawnSampleBossPattern()
{
	UBulletHellComponent* BulletHell = ResolveBulletHellComponent();
	if (!BulletHell)
	{
		return 0;
	}

	const FVector Origin = ResolveDebugSpawnOrigin();
	const FVector Forward = ResolveDebugSpawnForward();
	const FVector Right = ResolveDebugSpawnRight();

	int32 SpawnedCount = 0;
	constexpr int32 RingCount = 24;
	FBulletArchetype RingArchetype = BuildDebugArchetype(0);
	RingArchetype.BehaviorType = EBulletBehaviorType::Linear;
	for (int32 Index = 0; Index < RingCount; ++Index)
	{
		const float Angle = TwoPi * static_cast<float>(Index) / static_cast<float>(RingCount);
		const FVector Direction = SafeDirection(Forward * std::cos(Angle) + Right * std::sin(Angle), Forward);
		BulletHell->SpawnBullet(BuildSpawnParams(Origin, Direction, 0, RingArchetype));
		++SpawnedCount;
	}

	FBulletArchetype HomingArchetype = BuildDebugArchetype(1);
	HomingArchetype.BehaviorType = EBulletBehaviorType::Homing;
	for (int32 Index = -2; Index <= 2; ++Index)
	{
		const FVector Position = Origin + Right * (static_cast<float>(Index) * 0.5f);
		FBulletSpawnParams Params = BuildSpawnParams(Position, Forward, 1, HomingArchetype);
		Params.HomingTargetPosition = Origin + Forward * 8.0f + Right * (static_cast<float>(Index) * 0.75f);
		BulletHell->SpawnBullet(Params);
		++SpawnedCount;
	}

	return SpawnedCount;
}

void UBulletHellDebugComponent::DrawBulletDebug()
{
	UBulletHellComponent* BulletHell = ResolveBulletHellComponent();
	if (!BulletHell)
	{
		return;
	}

	if (!bDrawBulletDebug || DebugDrawMode == EBulletHellDebugDrawMode::Off)
	{
		BulletHell->RecordDebugDrawStats(0, 0);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		BulletHell->RecordDebugDrawStats(0, 0);
		return;
	}

	const int32 MaxDrawCount = (std::max)(0, DebugDrawMaxCount);
	int32 EligibleCount = 0;
	int32 DrawnCount = 0;

	for (const FBulletInstance& Bullet : BulletHell->GetBulletInstances())
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
		if (ShouldDrawDebugBounds())
		{
			DrawBulletCross(Bullet.Position, Color, (std::max)(Bullet.Radius * 0.35f, 0.02f));
			DrawDebugSphere(World, Bullet.Position, Bullet.Radius, 12, Color, 0.0f);
		}
		if (ShouldDrawDebugVelocity() && (!Bullet.PreviousPosition.IsNearlyZero() || !Bullet.Position.IsNearlyZero()))
		{
			const FVector VelocitySegment = Bullet.Position - Bullet.PreviousPosition;
			const FVector VelocityEnd = Bullet.PreviousPosition + VelocitySegment * (std::max)(1.0f, DebugVelocityDrawScale);
			DrawDebugLine(World, Bullet.PreviousPosition, VelocityEnd, FColor(255, 140, 0), 0.0f);
		}
		if (ShouldDrawDebugCollision())
		{
			DrawDebugLine(World, Bullet.PreviousPosition, Bullet.Position, FColor::Gray(), 0.0f);
		}
		++DrawnCount;
	}

	BulletHell->RecordDebugDrawStats(DrawnCount, (std::max)(0, EligibleCount - DrawnCount));
}

void UBulletHellDebugComponent::DrawBulletCross(const FVector& Center, const FColor& Color, float Extent) const
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

bool UBulletHellDebugComponent::ShouldDrawDebugBounds() const
{
	return DebugVisualMode == EBulletHellDebugVisualMode::Bounds ||
		DebugVisualMode == EBulletHellDebugVisualMode::BoundsAndVelocity ||
		DebugVisualMode == EBulletHellDebugVisualMode::All;
}

bool UBulletHellDebugComponent::ShouldDrawDebugVelocity() const
{
	return DebugVisualMode == EBulletHellDebugVisualMode::Velocity ||
		DebugVisualMode == EBulletHellDebugVisualMode::BoundsAndVelocity ||
		DebugVisualMode == EBulletHellDebugVisualMode::All;
}

bool UBulletHellDebugComponent::ShouldDrawDebugCollision() const
{
	return DebugVisualMode == EBulletHellDebugVisualMode::Collision ||
		DebugVisualMode == EBulletHellDebugVisualMode::All;
}

FVector UBulletHellDebugComponent::ResolveDebugSpawnOrigin() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
}

FVector UBulletHellDebugComponent::ResolveDebugSpawnForward() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? SafeDirection(OwnerActor->GetActorForward(), FVector::ForwardVector) : FVector::ForwardVector;
}

FVector UBulletHellDebugComponent::ResolveDebugSpawnRight() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? SafeDirection(OwnerActor->GetActorRight(), FVector::RightVector) : FVector::RightVector;
}

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
	FBulletInstance Bullet;
	Bullet.Id = NextBulletId;
	Bullet.Generation = NextBulletGeneration;
	Bullet.Position = Position;
	Bullet.PreviousPosition = Position;
	Bullet.Velocity = Velocity;
	Bullet.Radius = (std::max)(0.01f, Radius);
	Bullet.Age = 0.0f;
	Bullet.Lifetime = Lifetime;
	Bullet.RenderInstanceIndex = -1;
	Bullet.bAlive = true;

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

		SpawnBullet(
			Position,
			Direction * DebugSpawnSpeed,
			DebugSpawnRadius,
			DebugSpawnLifetime);
	}

	return SafeCount;
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
		"BulletHell first bullet: Id=%u Generation=%u Position=(%.2f,%.2f,%.2f) Previous=(%.2f,%.2f,%.2f) Velocity=(%.2f,%.2f,%.2f) Radius=%.2f Age=%.2f Lifetime=%.2f RenderIndex=%d",
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
		Bullet.RenderInstanceIndex);
}

FString UBulletHellComponent::GetBulletDebugStatsText() const
{
	char Buffer[384];
	std::snprintf(
		Buffer,
		sizeof(Buffer),
		"BulletHell stats: Active=%d Spawned=%u Killed=%u Expired=%u DebugDrawSelected=%d DebugDrawTruncated=%d RenderInstances=%d RendererSlots=%d RenderMismatch=%d",
		DebugStats.ActiveBulletCount,
		DebugStats.TotalSpawned,
		DebugStats.TotalKilled,
		DebugStats.TotalExpired,
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
		return;
	}

	for (int32 Index = 0; Index < static_cast<int32>(Bullets.size());)
	{
		FBulletInstance& Bullet = Bullets[Index];
		Bullet.PreviousPosition = Bullet.Position;
		Bullet.Position += Bullet.Velocity * DeltaTime;
		Bullet.Age += DeltaTime;

		if (Bullet.Lifetime >= 0.0f && Bullet.Age >= Bullet.Lifetime)
		{
			RemoveBulletAtIndex(Index, true);
			continue;
		}

		++Index;
	}

	DebugStats.ActiveBulletCount = static_cast<int32>(Bullets.size());
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

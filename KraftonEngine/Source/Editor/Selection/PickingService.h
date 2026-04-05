#pragma once
#pragma once

#include "Editor/Selection/PickingTypes.h"
#include "Editor/Selection/PickingPerf.h"
#include "Collision/RayUtils.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Render/Pipeline/PrimitiveProxy.h"
#include "Object/Object.h"

#include <cfloat>

class UWorld;
class AActor;
class UPrimitiveComponent;

class FPickingService
{
public:
	//	UUID 기반으로 픽킹 ID 생성. Object가 nullptr이면 0 반환 (픽킹 실패 시 ID)
	static uint32 MakeObjectPickingId(const UObject* Object)
	{
		return Object ? Object->GetUUID() : 0u;
	}

	static UObject* ResolveObjectFromPickingId(uint32 PickingId)
	{
		if (PickingId == 0u)
		{
			return nullptr;
		}

		return UObjectManager::Get().FindByUUID(PickingId);
	}

	static bool PickGizmo(UPrimitiveComponent* Gizmo, const FRay& Ray, EPickingMode Mode, FHitResult& OutHit, uint32* OutPickingId = nullptr)
	{
		if (!Gizmo)
		{
			if (OutPickingId) *OutPickingId = 0u;
			return false;
		}

		if (OutPickingId)
		{
			*OutPickingId = MakeObjectPickingId(Gizmo);
		}

		if (Mode == EPickingMode::IDBuffer)
		{
			if (OutPickingId) *OutPickingId = 0u;
			return false;
		}

		return FRayUtils::RaycastComponent(Gizmo, Ray, OutHit);
	}

	static AActor* PickActor(UWorld* World, const FRay& Ray, EPickingMode Mode, float& OutClosestDistance, uint32* OutPickingId = nullptr)
	{
		//	World 정보가 없을 시 Picking 실패로 간주 (0u 반환)
		if (!World)
		{
			if (OutPickingId) *OutPickingId = 0u;
			return nullptr;
		}

		if (Mode == EPickingMode::IDBuffer)
		{
			OutClosestDistance = FLT_MAX;
			if (OutPickingId) *OutPickingId = 0u;
			return nullptr;
		}

		return PickActorByRay(World, Ray, OutClosestDistance, OutPickingId);
	}

private:
	static AActor* PickActorByRay(UWorld* World, const FRay& Ray, float& OutClosestDistance, uint32* OutPickingId)
	{
		AActor* BestActor = nullptr;
		OutClosestDistance = FLT_MAX;
		if (OutPickingId) *OutPickingId = 0u;
		FHitResult HitResult{};
		uint64 BroadPhaseCycles = 0;
		uint64 NarrowPhaseCycles = 0;
		TArray<FPrimitiveProxy*> BroadCandidates;

		const uint64 BroadStart = FPickingPlatformTime::Cycles64();
		if (UScene* Scene = World->GetActiveScene())
		{
			Scene->GetRenderProxy().QueryByRay(Ray, BroadCandidates, true);
			if (BroadCandidates.empty())
			{
				Scene->GetRenderProxy().QueryByRay(Ray, BroadCandidates, false);
			}
		}
		BroadPhaseCycles += (FPickingPlatformTime::Cycles64() - BroadStart);

		for (FPrimitiveProxy* Proxy : BroadCandidates)
		{
			if (!Proxy)
			{
				continue;
			}

			UPrimitiveComponent* PrimitiveComp = Proxy->GetOwner();
			if (!PrimitiveComp || !PrimitiveComp->IsVisible())
			{
				continue;
			}

			AActor* Actor = PrimitiveComp->GetOwner();
			if (!Actor || !Actor->GetRootComponent() || !Actor->IsVisible())
			{
				continue;
			}

			float NearT = 0.0f;
			const FBoundingBox Bounds = PrimitiveComp->GetWorldBoundingBox();
			if (!FRayUtils::CheckRayAABBNearT(Ray, Bounds.Min, Bounds.Max, NearT))
			{
				continue;
			}
			if (NearT >= OutClosestDistance)
			{
				continue;
			}

			HitResult = {};
			const uint64 NarrowStart = FPickingPlatformTime::Cycles64();
			const bool bTriangleHit = PrimitiveComp->LineTraceComponent(Ray, HitResult);
			NarrowPhaseCycles += (FPickingPlatformTime::Cycles64() - NarrowStart);
			if (bTriangleHit && HitResult.Distance < OutClosestDistance)
			{
				OutClosestDistance = HitResult.Distance;
				BestActor = Actor;
				if (OutPickingId)
				{
					*OutPickingId = MakeObjectPickingId(Actor);
				}
			}
		}

		FPickingPerf::RecordRayBroadPhase(BroadPhaseCycles);
		FPickingPerf::RecordRayNarrowPhase(NarrowPhaseCycles);

		return BestActor;
	}
};

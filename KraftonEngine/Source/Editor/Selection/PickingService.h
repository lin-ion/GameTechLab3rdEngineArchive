#pragma once

#include "Editor/Selection/PickingTypes.h"
#include "Collision/PickingTuning.h"
#include "Collision/RayUtils.h"
#include "GameFramework/World.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Render/Pipeline/PrimitiveProxy.h"
#include "Render/Pipeline/IPrimitiveSpatialQuery.h"
#include "Object/Object.h"
#include "Profiling/Stats.h"

#include <cfloat>
#include <algorithm>

class UWorld;
class AActor;
class UPrimitiveComponent;

class FPickingService
{
public:
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
	struct FTemporalRayNearTHint
	{
		bool bValid = false;
		FRay Ray = {};
		float ClosestDistance = FLT_MAX;
	};

	static AActor* PickActorByRay(UWorld* World, const FRay& Ray, float& OutClosestDistance, uint32* OutPickingId)
	{
		thread_local FTemporalRayNearTHint TemporalHint;

		AActor* BestActor = nullptr;
		OutClosestDistance = FLT_MAX;
		if (OutPickingId) *OutPickingId = 0u;
		FHitResult HitResult{};
		thread_local TArray<FRayQueryCandidate> RayCandidates;
		thread_local int32 RayCandidateReserveHint = 0;
		RayCandidates.clear();
		if (RayCandidateReserveHint > 0)
		{
			RayCandidates.reserve(static_cast<size_t>(RayCandidateReserveHint));
		}

		auto ExecuteBroadQuery = [&](float MaxNearT)
		{
			SCOPE_STAT("Picking.Ray.Broad");
			RayCandidates.clear();
			if (UScene* Scene = World->GetActiveScene())
			{
				Scene->GetRenderProxy().QueryByRayWithNearT(Ray, RayCandidates, MaxNearT);
				if (static_cast<int32>(RayCandidates.size()) > RayCandidateReserveHint)
				{
					RayCandidateReserveHint = static_cast<int32>(RayCandidates.size());
				}
			}
		};

		bool bUsedNearTHint = false;
		if (TemporalHint.bValid)
		{
			const float DirSimilarity = Ray.Direction.Dot(TemporalHint.Ray.Direction);
			const FVector OriginDelta = Ray.Origin - TemporalHint.Ray.Origin;
			const float OriginDeltaSq = OriginDelta.Dot(OriginDelta);
			if (DirSimilarity > 0.9995f && OriginDeltaSq < 1.0f)
			{
				const float HintNearT = TemporalHint.ClosestDistance * 1.5f + 2.0f;
				ExecuteBroadQuery(HintNearT);
				bUsedNearTHint = true;
			}
		}

		if (!bUsedNearTHint)
		{
			ExecuteBroadQuery(FLT_MAX);
		}

		auto ExecuteNarrowQuery = [&]()
		{
			SCOPE_STAT("Picking.Ray.Narrow");

			const float NearTEpsilon = FPickingTuning::NearTEpsilon();
			const auto NearTMinHeapCompare = [](const FRayQueryCandidate& L, const FRayQueryCandidate& R)
			{
				return L.NearT > R.NearT;
			};

			if (RayCandidates.size() > 1)
			{
				std::make_heap(RayCandidates.begin(), RayCandidates.end(), NearTMinHeapCompare);
			}

			while (!RayCandidates.empty())
			{
				if (RayCandidates.size() > 1)
				{
					std::pop_heap(RayCandidates.begin(), RayCandidates.end(), NearTMinHeapCompare);
				}

				const FRayQueryCandidate Candidate = RayCandidates.back();
				RayCandidates.pop_back();

				if (Candidate.NearT >= OutClosestDistance - NearTEpsilon)
				{
					break;
				}

				FPrimitiveProxy* Proxy = Candidate.Proxy;
				if (!Proxy)
				{
					continue;
				}

				UPrimitiveComponent* PrimitiveComp = Proxy->GetOwner();
				if (!PrimitiveComp)
				{
					continue;
				}

				AActor* Actor = PrimitiveComp->GetOwner();
				if (!Actor || !Actor->GetRootComponent())
				{
					continue;
				}

				HitResult = {};
				const bool bTriangleHit = PrimitiveComp->LineTraceComponent(Ray, HitResult, OutClosestDistance);
				if (bTriangleHit && HitResult.Distance < OutClosestDistance)
				{
					OutClosestDistance = HitResult.Distance;
					BestActor = Actor;
				}
			}
		};

		ExecuteNarrowQuery();
		if (!BestActor && bUsedNearTHint)
		{
			ExecuteBroadQuery(FLT_MAX);
			ExecuteNarrowQuery();
		}

		if (OutPickingId && BestActor)
		{
			*OutPickingId = MakeObjectPickingId(BestActor);
		}

		TemporalHint.Ray = Ray;
		TemporalHint.ClosestDistance = OutClosestDistance;
		TemporalHint.bValid = (BestActor != nullptr) && (OutClosestDistance < FLT_MAX);

		return BestActor;
	}
};

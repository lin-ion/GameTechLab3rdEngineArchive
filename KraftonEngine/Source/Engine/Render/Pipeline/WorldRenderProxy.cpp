#include "WorldRenderProxy.h"
#include "PrimitiveProxy.h"
#include "Collision/PickingTuning.h"
#include "Render/Pipeline/FrustumCulling.h"
#include "Render/Pipeline/FixedWorldOctree.h"
#include "Render/Pipeline/WorldBVH.h"
#include "Render/Pipeline/IPrimitiveSpatialQuery.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/TextRenderComponent.h"
#include "Mesh/StaticMesh.h"
#include "Profiling/Stats.h"

#include <algorithm>
#include <cmath>

FWorldRenderProxy::FWorldRenderProxy()
{
	FrustumSpatialIndex = new FFixedWorldOctree();
	RaySpatialIndex = new FWorldBVH();
}

FWorldRenderProxy::~FWorldRenderProxy()
{
	delete FrustumSpatialIndex;
	FrustumSpatialIndex = nullptr;
	delete RaySpatialIndex;
	RaySpatialIndex = nullptr;
	Proxies.clear();
}

void FWorldRenderProxy::AddProxy(FPrimitiveProxy* Proxy)
{
	if (!this || !Proxy) return;

	auto it = std::find(Proxies.begin(), Proxies.end(), Proxy);
	if (it == Proxies.end())
	{
		Proxies.push_back(Proxy);
		Proxy->SetWorldRenderProxy(this);
		++SpatialChangeSerial;
		bSpatialIndexDirty = true;
		FrustumVisiblePickFrameTag = 0u;
		FrustumVisiblePickableCache.clear();
		FrustumVisiblePickableSoA.Clear();
		RayPickableSoA.Clear();
	}
}

void FWorldRenderProxy::MarkSpatialIndexDirty()
{
	if (SpatialIndexDeferDepth > 0)
	{
		if (!bDeferredSpatialIndexDirtyPending)
		{
			++SpatialChangeSerial;
		}
		bDeferredSpatialIndexDirtyPending = true;
		FrustumVisiblePickFrameTag = 0u;
		FrustumVisiblePickableCache.clear();
		FrustumVisiblePickableSoA.Clear();
		return;
	}

	if (!bSpatialIndexDirty)
	{
		++SpatialChangeSerial;
	}
	bSpatialIndexDirty = true;
	FrustumVisiblePickFrameTag = 0u;
	FrustumVisiblePickableCache.clear();
	FrustumVisiblePickableSoA.Clear();
}

void FWorldRenderProxy::BeginDeferSpatialIndexInvalidation()
{
	++SpatialIndexDeferDepth;
}

void FWorldRenderProxy::EndDeferSpatialIndexInvalidation()
{
	if (SpatialIndexDeferDepth <= 0)
	{
		SpatialIndexDeferDepth = 0;
		return;
	}

	--SpatialIndexDeferDepth;
	if (SpatialIndexDeferDepth == 0 && bDeferredSpatialIndexDirtyPending)
	{
		bSpatialIndexDirty = true;
		bDeferredSpatialIndexDirtyPending = false;
		FrustumVisiblePickFrameTag = 0u;
		FrustumVisiblePickableCache.clear();
		FrustumVisiblePickableSoA.Clear();
	}
}

void FWorldRenderProxy::WarmupSpatialIndices()
{
	RebuildSpatialIndexIfDirty(false, true);
	if (FrustumSpatialIndex)
	{
		FrustumSpatialIndex->Warmup();
	}
	if (RaySpatialIndex)
	{
		RaySpatialIndex->Warmup();
	}
}

void FWorldRenderProxy::RemoveProxy(FPrimitiveProxy* Proxy)
{
	if (!this || !Proxy) return;

	auto it = std::find(Proxies.begin(), Proxies.end(), Proxy);
	if (it != Proxies.end())
	{
		Proxies.erase(it);
		Proxy->SetWorldRenderProxy(nullptr);
		++SpatialChangeSerial;
		bSpatialIndexDirty = true;
		FrustumVisiblePickFrameTag = 0u;
		FrustumVisiblePickableCache.clear();
		FrustumVisiblePickableSoA.Clear();
		RayPickableSoA.Clear();
	}
}

void FWorldRenderProxy::GatherCandidates(FViewContext& Context)
{
	if (!this) return;
	if (!Context.GetShowFlags().bPrimitives) return;

	// 통계 초기화
	LastCullingStats = {};
	LastCullingStats.RegisteredProxyCount = static_cast<int32>(Proxies.size());

	const FFrustumPlanes Frustum = FFrustumCulling::BuildFrustumPlanes(Context.GetView(), Context.GetProj());

	RebuildSpatialIndexIfDirty(true, true);
	if (FrustumSpatialIndex)
	{
		FrustumSpatialIndex->Warmup();
	}
	if (RaySpatialIndex)
	{
		RaySpatialIndex->Warmup();
	}

	TArray<FPrimitiveProxy*> LocalCandidates;
	if (FrustumSpatialIndex)
	{
		FrustumSpatialIndex->QueryFrustum(Frustum, LocalCandidates);
		const FSpatialQueryDebugStats& SpatialStats = FrustumSpatialIndex->GetLastDebugStats();
		LastCullingStats.SpatialTotalNodes = SpatialStats.TotalNodes;
		LastCullingStats.SpatialTotalItems = SpatialStats.TotalItems;
		LastCullingStats.SpatialOutsideItems = SpatialStats.OutsideItems;
		LastCullingStats.SpatialFrustumIntersectedNodes = SpatialStats.FrustumIntersectedNodes;
		LastCullingStats.SpatialFrustumCandidateItems = SpatialStats.FrustumCandidateItems;
	}

	++FrustumVisiblePickFrameTag;
	if (FrustumVisiblePickFrameTag == 0u)
	{
		FrustumVisiblePickFrameTag = 1u;
	}
	FrustumVisiblePickableCache.clear();
	if (!LocalCandidates.empty())
	{
		FrustumVisiblePickableCache.reserve(LocalCandidates.size());
	}
	FrustumVisiblePickableSoA.Clear();
	if (!LocalCandidates.empty())
	{
		FrustumVisiblePickableSoA.Reserve(LocalCandidates.size());
	}

	for (FPrimitiveProxy* Proxy : LocalCandidates)
	{
		if (Proxy)
		{
			if (UPrimitiveComponent* Owner = Proxy->GetOwner())
			{
				const bool bExcluded = Owner->IsA<UTextRenderComponent>() || Owner->IsA<UGizmoComponent>();
				if (!bExcluded && Owner->IsVisible())
				{
					if (AActor* ActorOwner = Owner->GetOwner())
					{
						if (ActorOwner->IsVisible() && ActorOwner->GetRootComponent())
						{
							Proxy->MarkFrustumVisibleForPick(FrustumVisiblePickFrameTag);
							FrustumVisiblePickableCache.push_back(Proxy);
							FrustumVisiblePickableSoA.Add(Owner->GetWorldBoundingBox(), Proxy);
						}
					}
				}
			}
		}

		Context.AddCandidateProxy(Proxy);
	}

	LastCullingStats.CandidateProxyCount = static_cast<int32>(LocalCandidates.size());
}

void FWorldRenderProxy::QueryByRay(const FRay& Ray, TArray<FPrimitiveProxy*>& OutCandidates)
{
	thread_local TArray<FRayQueryCandidate> CandidatesWithNearT;
	CandidatesWithNearT.clear();
	QueryByRayWithNearT(Ray, CandidatesWithNearT, FLT_MAX);

	OutCandidates.clear();
	OutCandidates.reserve(CandidatesWithNearT.size());
	for (const FRayQueryCandidate& Candidate : CandidatesWithNearT)
	{
		OutCandidates.push_back(Candidate.Proxy);
	}
}

void FWorldRenderProxy::QueryByRayWithNearT(const FRay& Ray, TArray<FRayQueryCandidate>& OutCandidates, float MaxNearT)
{
	OutCandidates.clear();

	// During transform defer window, spatial index can be stale; bypass to direct broad test.
	if (SpatialIndexDeferDepth > 0 || bDeferredSpatialIndexDirtyPending)
	{
		const FRayAABBKernel Kernel = FRayAABBKernel::Build(Ray);
		OutCandidates.reserve(RayPickableSoA.Size());
		for (size_t i = 0; i < RayPickableSoA.Size(); ++i)
		{
			float NearT = 0.0f;
			float FarT = 0.0f;
			if (!IntersectRayAABBNearTMinMax(
				Ray,
				Kernel,
				RayPickableSoA.MinX[i], RayPickableSoA.MinY[i], RayPickableSoA.MinZ[i],
				RayPickableSoA.MaxX[i], RayPickableSoA.MaxY[i], RayPickableSoA.MaxZ[i],
				NearT, FarT) || NearT > MaxNearT)
			{
				continue;
			}

			OutCandidates.push_back({ RayPickableSoA.Proxies[i], NearT });
		}
		return;
	}

	RebuildSpatialIndexIfDirty(false, false);
	if (!RaySpatialIndex)
	{
		return;
	}

	// TODO: expose camera-jump signal to bypass gate on large view deltas.
	const bool bBypassFrustumGate = bSpatialIndexDirty || (SpatialIndexDeferDepth > 0) || bDeferredSpatialIndexDirtyPending;
	const bool bUseFrustumGate = bRayFrustumGateOptimizationEnabled && !bBypassFrustumGate && (FrustumVisiblePickFrameTag != 0u);
	const uint32 LinearThreshold = FPickingTuning::BroadLinearVisibleThreshold();

	if (bUseFrustumGate && !FrustumVisiblePickableCache.empty() && FrustumVisiblePickableCache.size() <= LinearThreshold)
	{
		const FRayAABBKernel Kernel = FRayAABBKernel::Build(Ray);
		OutCandidates.reserve(FrustumVisiblePickableSoA.Size());
		for (size_t i = 0; i < FrustumVisiblePickableSoA.Size(); ++i)
		{
			float NearT = 0.0f;
			float FarT = 0.0f;
			if (!IntersectRayAABBNearTMinMax(
				Ray,
				Kernel,
				FrustumVisiblePickableSoA.MinX[i], FrustumVisiblePickableSoA.MinY[i], FrustumVisiblePickableSoA.MinZ[i],
				FrustumVisiblePickableSoA.MaxX[i], FrustumVisiblePickableSoA.MaxY[i], FrustumVisiblePickableSoA.MaxZ[i],
				NearT, FarT) || NearT > MaxNearT)
			{
				continue;
			}
			OutCandidates.push_back({ FrustumVisiblePickableSoA.Proxies[i], NearT });
		}
		return;
	}

	RaySpatialIndex->Warmup();
	RaySpatialIndex->QueryRayWithNearT(Ray, OutCandidates, MaxNearT);

	if (bUseFrustumGate)
	{
		size_t WriteIndex = 0;
		const size_t RawCount = OutCandidates.size();
		for (size_t ReadIndex = 0; ReadIndex < RawCount; ++ReadIndex)
		{
			const FRayQueryCandidate& Candidate = OutCandidates[ReadIndex];
			if (!Candidate.Proxy)
			{
				continue;
			}
			if (!Candidate.Proxy->IsFrustumVisibleForPick(FrustumVisiblePickFrameTag))
			{
				continue;
			}

			OutCandidates[WriteIndex++] = Candidate;
		}
		OutCandidates.resize(WriteIndex);
	}
}

void FWorldRenderProxy::SubmitRenderCommands(FViewContext& Context, const TArray<AActor*>& SelectedActors)
{
	SCOPE_STAT("Render.SubmitCommands");
	if (!this) return;

	const TArray<FPrimitiveProxy*>& CandidateProxies = Context.GetCandidateProxies();
	LastCullingStats.RenderedProxyCount = 0;

	for (FPrimitiveProxy* Proxy : CandidateProxies)
	{
		if (!Proxy) continue;

		// 현재 WorldRenderProxy에 속한 프록시만 처리 (Stats 관리 및 중복 제출 방지)
		if (Proxy->GetWorldRenderProxy() != this) continue;

		UPrimitiveComponent* Owner = Proxy->GetOwner();
		if (!Owner || !Owner->IsVisible()) continue;

		bool bSelected = false;
		if (AActor* ActorOwner = Owner->GetOwner())
		{
			if (ActorOwner->IsVisible())
			{
				if (!SelectedActors.empty())
				{
					bSelected = std::find(SelectedActors.begin(), SelectedActors.end(), ActorOwner) != SelectedActors.end();
				}
			}
			else
			{
				continue;
			}
		}

		Proxy->SetSelected(bSelected);
		Proxy->SubmitRenderCommand(Context);
		LastCullingStats.RenderedProxyCount++;
	}
}

void FWorldRenderProxy::InjectAlwaysVisibleCandidates(FViewContext& Context, const TArray<AActor*>& SelectedActors, bool bIncludeGizmo)
{
	if (!this)
	{
		return;
	}

	// 1. 선택된 액터의 프록시를 즉시 주입 (전체 순회 제거)
	for (AActor* ActorOwner : SelectedActors)
	{
		if (!ActorOwner || !ActorOwner->IsVisible())
		{
			continue;
		}

		TArray<UPrimitiveComponent*> Components;
		Components = ActorOwner->GetPrimitiveComponents();

		for (UPrimitiveComponent* Owner : Components)
		{
			if (!Owner || !Owner->IsVisible())
			{
				continue;
			}

			if (FPrimitiveProxy* Proxy = Owner->GetProxy())
			{
				if (Proxy->GetWorldRenderProxy() == this)
				{
					Context.AddCandidateProxyUnique(Proxy);
				}
			}
		}
	}

	// 2. 기즈모 처리 (필요한 경우만 순회)
	if (bIncludeGizmo)
	{
		for (FPrimitiveProxy* Proxy : Proxies)
		{
			if (!Proxy) continue;

			UPrimitiveComponent* Owner = Proxy->GetOwner();
			if (Owner && Owner->IsVisible() && Owner->IsA<UGizmoComponent>())
			{
				Context.AddCandidateProxyUnique(Proxy);
			}
		}
	}
}

void FWorldRenderProxy::CollectWorld(FViewContext& Context, const TArray<AActor*>& SelectedActors)
{
	SCOPE_STAT("Render.CollectWorld");
	if (!this) return;
	LastCullingStats = {};
	LastCullingStats.RegisteredProxyCount = static_cast<int32>(Proxies.size());

	GatherCandidates(Context);
	SubmitRenderCommands(Context, SelectedActors);
}

void FWorldRenderProxy::RebuildSpatialIndexIfDirty(bool bTrackInsertedStats, bool bPrewarmStaticMeshBVH)
{
	if (!bSpatialIndexDirty)
	{
		return;
	}
	SCOPE_STAT("Render.SpatialBuild");

	if (FrustumSpatialIndex) FrustumSpatialIndex->Clear();
	if (RaySpatialIndex) RaySpatialIndex->Clear();
	RayPickableSoA.Clear();
	RayPickableSoA.Reserve(Proxies.size());

	for (FPrimitiveProxy* Proxy : Proxies)
	{
		if (!Proxy)
		{
			continue;
		}

		UPrimitiveComponent* Owner = Proxy->GetOwner();
		if (!Owner)
		{
			continue;
		}

		// Keep this off the broad query path to avoid first-click broad spikes.
		if (bPrewarmStaticMeshBVH)
		{
			if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Owner))
			{
				if (UStaticMesh* StaticMesh = StaticMeshComp->GetStaticMesh())
				{
					if (FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset())
					{
						if (!Asset->Vertices.empty() && !Asset->Indices.empty() && !Asset->GetBVH())
						{
							Asset->BuildBVH();
						}
					}
				}
			}
		}

		Owner->GetWorldMatrix();
		const FBoundingBox Bounds = Owner->GetWorldBoundingBox();

		if (FrustumSpatialIndex) FrustumSpatialIndex->Insert(Proxy, Bounds);
		if (RaySpatialIndex)
		{
			const bool bOwnerVisible = Owner->IsVisible();
			const bool bExcludedComponent = Owner->IsA<UTextRenderComponent>() || Owner->IsA<UGizmoComponent>();
			AActor* ActorOwner = Owner->GetOwner();
			const bool bActorPickable = (ActorOwner != nullptr) && ActorOwner->IsVisible() && (ActorOwner->GetRootComponent() != nullptr);

			if (bOwnerVisible && !bExcludedComponent && bActorPickable)
			{
				RaySpatialIndex->Insert(Proxy, Bounds);
				RayPickableSoA.Add(Bounds, Proxy);
			}
		}

		if (bTrackInsertedStats)
		{
			++LastCullingStats.InsertedProxyCount;
		}
	}

	bSpatialIndexDirty = false;
}

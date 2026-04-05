#include "WorldRenderProxy.h"
#include "PrimitiveProxy.h"
#include "Collision/RayUtils.h"
#include "Render/Pipeline/FrustumCulling.h"
#include "Render/Pipeline/FixedWorldOctree.h"
#include "Render/Pipeline/IPrimitiveSpatialQuery.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/GizmoComponent.h"
#include "Component/TextRenderComponent.h"
#include "Profiling/Stats.h"

#include <algorithm>

FWorldRenderProxy::FWorldRenderProxy()
{
	SpatialIndex = new FFixedWorldOctree();
}

FWorldRenderProxy::~FWorldRenderProxy()
{
	delete SpatialIndex;
	SpatialIndex = nullptr;
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
		bSpatialIndexDirty = true;
	}
}

void FWorldRenderProxy::MarkSpatialIndexDirty()
{
	if (SpatialIndexDeferDepth > 0)
	{
		bDeferredSpatialIndexDirtyPending = true;
		return;
	}

	bSpatialIndexDirty = true;
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
		bSpatialIndexDirty = true;
	}
}

void FWorldRenderProxy::GatherCandidates(FViewContext& Context, bool bUseSpatialIndex)
{
	if (!this) return;
	if (!Context.GetShowFlags().bPrimitives) return;

	// 통계 초기화
	LastCullingStats = {};
	LastCullingStats.RegisteredProxyCount = static_cast<int32>(Proxies.size());

	const FFrustumPlanes Frustum = FFrustumCulling::BuildFrustumPlanes(Context.GetView(), Context.GetProj());

	TArray<FPrimitiveProxy*> LocalCandidates;
	if (bUseSpatialIndex && SpatialIndex)
	{
		const bool bNeedsRebuild = bSpatialIndexDirty;
		RebuildSpatialIndexIfDirty(true);

		SpatialIndex->QueryFrustum(Frustum, LocalCandidates);
		const FSpatialQueryDebugStats& OctreeStats = SpatialIndex->GetLastDebugStats();
		if (!bNeedsRebuild)
		{
			LastCullingStats.InsertedProxyCount = 0;
		}
		LastCullingStats.OctreeTotalNodes = OctreeStats.TotalNodes;
		LastCullingStats.OctreeTotalItems = OctreeStats.TotalItems;
		LastCullingStats.OctreeOutsideItems = OctreeStats.OutsideItems;
		LastCullingStats.OctreeFrustumIntersectedNodes = OctreeStats.FrustumIntersectedNodes;
		LastCullingStats.OctreeFrustumCandidateItems = OctreeStats.FrustumCandidateItems;
	}
	else
	{
		for (FPrimitiveProxy* Proxy : Proxies)
		{
			if (!Proxy) continue;

			UPrimitiveComponent* Owner = Proxy->GetOwner();
			if (!Owner || !Owner->IsVisible()) continue;

			if (AActor* ActorOwner = Owner->GetOwner())
			{
				if (!ActorOwner->IsVisible()) continue;
			}

			if (!FFrustumCulling::IntersectsAABB(Frustum, Owner->GetWorldBoundingBox())) continue;
			LocalCandidates.push_back(Proxy);
		}
	}

	for (FPrimitiveProxy* Proxy : LocalCandidates)
	{
		Context.AddCandidateProxy(Proxy);
	}

	LastCullingStats.CandidateProxyCount = static_cast<int32>(LocalCandidates.size());
}

void FWorldRenderProxy::QueryByRay(const FRay& Ray, TArray<FPrimitiveProxy*>& OutCandidates, bool bUseSpatialIndex)
{
	OutCandidates.clear();

	if (bUseSpatialIndex && SpatialIndex)
	{
		RebuildSpatialIndexIfDirty(false);
		SpatialIndex->QueryRay(Ray, OutCandidates);
		return;
	}

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

		if (Owner->IsA<UTextRenderComponent>())
		{
			continue;
		}

		Owner->UpdateWorldAABB();
		const FBoundingBox Bounds = Owner->GetWorldBoundingBox();
		if (!FRayUtils::CheckRayAABB(Ray, Bounds.Min, Bounds.Max))
		{
			continue;
		}

		OutCandidates.push_back(Proxy);
	}
}

void FWorldRenderProxy::SubmitRenderCommands(FViewContext& Context, const TArray<AActor*>& SelectedActors)
{
	SCOPE_STAT("Render.SubmitCommands");
	if (!this) return;

	const TSet<AActor*> SelectedActorSet(SelectedActors.begin(), SelectedActors.end());
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
			if (!ActorOwner->IsVisible()) continue;
			bSelected = SelectedActorSet.find(ActorOwner) != SelectedActorSet.end();
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

	const TSet<AActor*> SelectedActorSet(SelectedActors.begin(), SelectedActors.end());

	for (FPrimitiveProxy* Proxy : Proxies)
	{
		if (!Proxy)
		{
			continue;
		}

		UPrimitiveComponent* Owner = Proxy->GetOwner();
		if (!Owner || !Owner->IsVisible())
		{
			continue;
		}

		if (bIncludeGizmo && Owner->IsA<UGizmoComponent>())
		{
			Context.AddCandidateProxyUnique(Proxy);
			continue;
		}

		AActor* ActorOwner = Owner->GetOwner();
		if (!ActorOwner || !ActorOwner->IsVisible())
		{
			continue;
		}

		if (SelectedActorSet.find(ActorOwner) != SelectedActorSet.end())
		{
			Context.AddCandidateProxyUnique(Proxy);
		}
	}
}

void FWorldRenderProxy::CollectWorld(FViewContext& Context, const TArray<AActor*>& SelectedActors, bool bUseSpatialIndex)
{
	SCOPE_STAT("Render.CollectWorld");
	if (!this) return;
	LastCullingStats = {};
	LastCullingStats.RegisteredProxyCount = static_cast<int32>(Proxies.size());

	GatherCandidates(Context, bUseSpatialIndex);
	SubmitRenderCommands(Context, SelectedActors);
}

void FWorldRenderProxy::RebuildSpatialIndexIfDirty(bool bTrackInsertedStats)
{
	if (!SpatialIndex || !bSpatialIndexDirty)
	{
		return;
	}
	SCOPE_STAT("Render.OctreeBuild");

	SpatialIndex->Clear();

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

		Owner->GetWorldMatrix();
		SpatialIndex->Insert(Proxy, Owner->GetWorldBoundingBox());
		if (bTrackInsertedStats)
		{
			++LastCullingStats.InsertedProxyCount;
		}
	}

	bSpatialIndexDirty = false;
}

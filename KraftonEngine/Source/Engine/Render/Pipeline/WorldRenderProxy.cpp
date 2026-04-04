#include "WorldRenderProxy.h"
#include "PrimitiveProxy.h"
#include "Render/Pipeline/FrustumCulling.h"
#include "Render/Pipeline/FixedWorldOctree.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/GizmoComponent.h"

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

	// 인스턴스별 통계 초기화 (등록된 프록시 수는 유지)
	LastCullingStats.CandidateProxyCount = 0;
	LastCullingStats.RenderedProxyCount = 0;
	LastCullingStats.InsertedProxyCount = 0;
	LastCullingStats.OctreeFrustumIntersectedNodes = 0;
	LastCullingStats.OctreeFrustumCandidateItems = 0;

	const FFrustumPlanes Frustum = FFrustumCulling::BuildFrustumPlanes(Context.GetView(), Context.GetProj());

	TArray<FPrimitiveProxy*> LocalCandidates;
	if (bUseSpatialIndex && SpatialIndex)
	{
		const bool bNeedsRebuild = bSpatialIndexDirty;

		if (bNeedsRebuild)
		{
			SpatialIndex->Clear();

			for (FPrimitiveProxy* Proxy : Proxies)
			{
				if (!Proxy) continue;

				UPrimitiveComponent* Owner = Proxy->GetOwner();
				if (!Owner) continue;
				Owner->GetWorldMatrix();

				SpatialIndex->Insert(Proxy, Owner->GetWorldBoundingBox());
				++LastCullingStats.InsertedProxyCount;
			}

			bSpatialIndexDirty = false;
		}

		SpatialIndex->Query(Frustum, LocalCandidates);
		const FOctreeDebugStats& OctreeStats = SpatialIndex->GetLastDebugStats();
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

void FWorldRenderProxy::SubmitRenderCommands(FViewContext& Context, const TArray<AActor*>& SelectedActors)
{
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

void FWorldRenderProxy::CollectWorld(FViewContext& Context, const TArray<AActor*>& SelectedActors, bool bUseSpatialIndex)
{
	if (!this) return;
	LastCullingStats = {};
	LastCullingStats.RegisteredProxyCount = static_cast<int32>(Proxies.size());

	GatherCandidates(Context, bUseSpatialIndex);
	SubmitRenderCommands(Context, SelectedActors);
}

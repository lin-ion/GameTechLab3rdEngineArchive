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
}

void FWorldRenderProxy::AddProxy(FPrimitiveProxy* Proxy)
{
	if (!this || !Proxy) return;

	auto it = std::find(Proxies.begin(), Proxies.end(), Proxy);
	if (it == Proxies.end())
	{
		Proxies.push_back(Proxy);
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
		bSpatialIndexDirty = true;
	}
}

void FWorldRenderProxy::CollectWorld(FRenderBus& Bus, const TArray<AActor*>& SelectedActors)
{
	if (!this) return;
	if (!Bus.GetShowFlags().bPrimitives) return;
	LastCullingStats = {};
	LastCullingStats.RegisteredProxyCount = static_cast<int32>(Proxies.size());

	const FFrustumPlanes Frustum = FFrustumCulling::BuildFrustumPlanes(Bus.GetView(), Bus.GetProj());
	const TSet<AActor*> SelectedActorSet(SelectedActors.begin(), SelectedActors.end());

	bool bNeedsRebuild = bSpatialIndexDirty;

	if (SpatialIndex && bNeedsRebuild)
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

	TArray<FPrimitiveProxy*> CandidateProxies;
	if (SpatialIndex)
	{
		SpatialIndex->Query(Frustum, CandidateProxies);
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
	LastCullingStats.CandidateProxyCount = static_cast<int32>(CandidateProxies.size());

	for (FPrimitiveProxy* Proxy : CandidateProxies)
	{
		if (!Proxy) continue;

		UPrimitiveComponent* Owner = Proxy->GetOwner();
		if (!Owner || !Owner->IsVisible()) continue;

		bool bSelected = false;
		if (AActor* ActorOwner = Owner->GetOwner())
		{
			if (!ActorOwner->IsVisible()) continue;
			bSelected = SelectedActorSet.find(ActorOwner) != SelectedActorSet.end();
		}

		Proxy->CollectRender(Bus, bSelected);
		++LastCullingStats.RenderedProxyCount;
	}
}

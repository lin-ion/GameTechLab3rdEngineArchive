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
		RebuildSpatialIndexIfDirty(false);

		const size_t Count = CullingSoA.Proxies.size();
		for (size_t i = 0; i < Count; i += 4)
		{
			int remaining = (int)Count - (int)i;
			int batchSize = remaining > 4 ? 4 : remaining;

			float minX[4], minY[4], minZ[4], maxX[4], maxY[4], maxZ[4];

			if (remaining >= 4)
			{
				uint32 mask = FFrustumCulling::TestAABB4(Frustum, 
					&CullingSoA.MinX[i], &CullingSoA.MinY[i], &CullingSoA.MinZ[i],
					&CullingSoA.MaxX[i], &CullingSoA.MaxY[i], &CullingSoA.MaxZ[i]);

				for (int j = 0; j < 4; ++j)
				{
					if (!(mask & (1 << j)))
					{
						FPrimitiveProxy* Proxy = CullingSoA.Proxies[i + j];
						UPrimitiveComponent* Owner = Proxy->GetOwner();
						if (Owner && Owner->IsVisible())
						{
							if (AActor* ActorOwner = Owner->GetOwner())
							{
								if (!ActorOwner->IsVisible()) continue;
							}
							LocalCandidates.push_back(Proxy);
						}
					}
				}
			}
			else
			{
				// 4개 미만 남은 경우 안전하게 패딩하여 처리
				for (int j = 0; j < 4; ++j) {
					int idx = (j < remaining) ? (int)i + j : (int)i; // 패딩용으로 i번째 반복
					minX[j] = CullingSoA.MinX[idx]; minY[j] = CullingSoA.MinY[idx]; minZ[j] = CullingSoA.MinZ[idx];
					maxX[j] = CullingSoA.MaxX[idx]; maxY[j] = CullingSoA.MaxY[idx]; maxZ[j] = CullingSoA.MaxZ[idx];
				}

				uint32 mask = FFrustumCulling::TestAABB4(Frustum, minX, minY, minZ, maxX, maxY, maxZ);

				for (int j = 0; j < remaining; ++j)
				{
					if (!(mask & (1 << j)))
					{
						FPrimitiveProxy* Proxy = CullingSoA.Proxies[i + j];
						UPrimitiveComponent* Owner = Proxy->GetOwner();
						if (Owner && Owner->IsVisible())
						{
							if (AActor* ActorOwner = Owner->GetOwner())
							{
								if (!ActorOwner->IsVisible()) continue;
							}
							LocalCandidates.push_back(Proxy);
						}
					}
				}
			}
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
	if (!bSpatialIndexDirty)
	{
		return;
	}
	SCOPE_STAT("Render.OctreeBuild");

	if (SpatialIndex) SpatialIndex->Clear();
	CullingSoA.Clear();
	CullingSoA.Reserve(Proxies.size());

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
		const FBoundingBox Bounds = Owner->GetWorldBoundingBox();

		if (SpatialIndex) SpatialIndex->Insert(Proxy, Bounds);
		CullingSoA.Add(Bounds, Proxy);

		if (bTrackInsertedStats)
		{
			++LastCullingStats.InsertedProxyCount;
		}
	}

	bSpatialIndexDirty = false;
}

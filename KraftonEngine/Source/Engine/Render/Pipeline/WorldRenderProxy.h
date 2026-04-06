#pragma once
#include "Core/CoreTypes.h"
#include "Core/RayTypes.h"
#include "Render/Pipeline/ViewContext.h"

class FPrimitiveProxy;
class AActor;
class IPrimitiveSpatialQuery;

struct FWorldProxyCullingStats
{
	int32 RegisteredProxyCount = 0;
	int32 InsertedProxyCount = 0;
	int32 CandidateProxyCount = 0;
	int32 RenderedProxyCount = 0;

	int32 OctreeTotalNodes = 0;
	int32 OctreeTotalItems = 0;
	int32 OctreeOutsideItems = 0;
	int32 OctreeFrustumIntersectedNodes = 0;
	int32 OctreeFrustumCandidateItems = 0;
};

struct FBoundingBoxSoA
{
	TArray<float> MinX, MinY, MinZ;
	TArray<float> MaxX, MaxY, MaxZ;
	TArray<FPrimitiveProxy*> Proxies;

	void Clear()
	{
		MinX.clear(); MinY.clear(); MinZ.clear();
		MaxX.clear(); MaxY.clear(); MaxZ.clear();
		Proxies.clear();
	}

	void Reserve(size_t Capacity)
	{
		MinX.reserve(Capacity); MinY.reserve(Capacity); MinZ.reserve(Capacity);
		MaxX.reserve(Capacity); MaxY.reserve(Capacity); MaxZ.reserve(Capacity);
		Proxies.reserve(Capacity);
	}

	void Add(const FBoundingBox& Box, FPrimitiveProxy* Proxy)
	{
		MinX.push_back(Box.Min.X); MinY.push_back(Box.Min.Y); MinZ.push_back(Box.Min.Z);
		MaxX.push_back(Box.Max.X); MaxY.push_back(Box.Max.Y); MaxZ.push_back(Box.Max.Z);
		Proxies.push_back(Proxy);
	}
};

class FWorldRenderProxy
{
public:
	FWorldRenderProxy();
	~FWorldRenderProxy();

	void AddProxy(FPrimitiveProxy* Proxy);
	void RemoveProxy(FPrimitiveProxy* Proxy);
	void MarkSpatialIndexDirty();
	void BeginDeferSpatialIndexInvalidation();
	void EndDeferSpatialIndexInvalidation();

	// Phase 1: 씬으로부터 잠재적 후보군 수집
	void GatherCandidates(FViewContext& context, bool bUseSpatialIndex);

	// Phase 2: 최종 생존한 후보들에 대해 렌더 커맨드 생성
	void SubmitRenderCommands(FViewContext& context, const TArray<AActor*>& SelectedActors);
	void InjectAlwaysVisibleCandidates(FViewContext& context, const TArray<AActor*>& SelectedActors, bool bIncludeGizmo);

	// 레거시 지원 (필요시)
	void CollectWorld(FViewContext& context, const TArray<AActor*>& SelectedActors, bool bUseSpatialIndex);
	void QueryByRay(const FRay& Ray, TArray<FPrimitiveProxy*>& OutCandidates, bool bUseSpatialIndex = true);

	const FWorldProxyCullingStats& GetLastCullingStats() const { return LastCullingStats; }

private:
	void RebuildSpatialIndexIfDirty(bool bTrackInsertedStats);

	TArray<FPrimitiveProxy*> Proxies;
	IPrimitiveSpatialQuery* SpatialIndex = nullptr;
	FBoundingBoxSoA CullingSoA;
	bool bSpatialIndexDirty = true;
	int32 SpatialIndexDeferDepth = 0;
	bool bDeferredSpatialIndexDirtyPending = false;
	FWorldProxyCullingStats LastCullingStats;
};

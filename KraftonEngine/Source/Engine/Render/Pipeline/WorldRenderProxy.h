#pragma once
#include "Core/CoreTypes.h"
#include "Render/Pipeline/ViewContext.h"

class FPrimitiveProxy;
class AActor;
class FFixedWorldOctree;

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

class FWorldRenderProxy
{
public:
	FWorldRenderProxy() = default;
	~FWorldRenderProxy();

	void AddProxy(FPrimitiveProxy* Proxy);
	void RemoveProxy(FPrimitiveProxy* Proxy);
	void MarkSpatialIndexDirty() { bSpatialIndexDirty = true; }

	// World의 정보들을 바탕으로 Proxies 업데이트
	void CollectWorld(FViewContext& context, const TArray<AActor*>& SelectedActors);
	const FWorldProxyCullingStats& GetLastCullingStats() const { return LastCullingStats; }

private:
	TArray<FPrimitiveProxy*> Proxies;
	FFixedWorldOctree* SpatialIndex = nullptr;
	bool bSpatialIndexDirty = true;
	FWorldProxyCullingStats LastCullingStats;
};

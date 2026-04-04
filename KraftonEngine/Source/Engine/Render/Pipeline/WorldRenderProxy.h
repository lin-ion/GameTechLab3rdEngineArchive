#pragma once
#include "Core/CoreTypes.h"
#include "Render/Pipeline/RenderBus.h"

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
	FWorldRenderProxy();
	~FWorldRenderProxy();

	void AddProxy(FPrimitiveProxy* Proxy);
	void RemoveProxy(FPrimitiveProxy* Proxy);
	void MarkSpatialIndexDirty() { bSpatialIndexDirty = true; }

	void CollectWorld(FRenderBus& Bus, const TArray<AActor*>& SelectedActors, bool bUseSpatialIndex = true);
	const FWorldProxyCullingStats& GetLastCullingStats() const { return LastCullingStats; }

private:
	TArray<FPrimitiveProxy*> Proxies;
	FFixedWorldOctree* SpatialIndex = nullptr;
	bool bSpatialIndexDirty = true;
	FWorldProxyCullingStats LastCullingStats;
};

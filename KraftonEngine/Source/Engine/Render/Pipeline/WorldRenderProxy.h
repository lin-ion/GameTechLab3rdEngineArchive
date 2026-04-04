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
	FWorldRenderProxy();
	~FWorldRenderProxy();

	void AddProxy(FPrimitiveProxy* Proxy);
	void RemoveProxy(FPrimitiveProxy* Proxy);
	void MarkSpatialIndexDirty() { bSpatialIndexDirty = true; }

	// Phase 1: 씬으로부터 잠재적 후보군 수집
	void GatherCandidates(FViewContext& context, bool bUseSpatialIndex);

	// Phase 2: 최종 생존한 후보들에 대해 렌더 커맨드 생성
	void SubmitRenderCommands(FViewContext& context, const TArray<AActor*>& SelectedActors);

	// 레거시 지원 (필요시)
	void CollectWorld(FViewContext& context, const TArray<AActor*>& SelectedActors, bool bUseSpatialIndex);

	const FWorldProxyCullingStats& GetLastCullingStats() const { return LastCullingStats; }

private:
	TArray<FPrimitiveProxy*> Proxies;
	FFixedWorldOctree* SpatialIndex = nullptr;
	bool bSpatialIndexDirty = true;
	FWorldProxyCullingStats LastCullingStats;
};

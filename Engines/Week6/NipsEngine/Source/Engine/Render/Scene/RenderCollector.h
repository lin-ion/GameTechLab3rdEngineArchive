#pragma once
#include "RenderBus.h"
#include "Render/PostProcess/PostProcessTypes.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Spatial/WorldSpatialIndex.h"
#include <unordered_set>

class UWorld;
class AActor;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UGizmoComponent;
struct FFrustum;

class UFireBallComponent;
class UHeightFogComponent;


class FRenderCollector {
public:
	struct FCullingStats
	{
		int32 TotalVisiblePrimitiveCount{0};
		int32 BVHPassedPrimitiveCount{0};
		int32 FallbackPassedPrimitiveCount{0};
	};

	struct FDecalStats
	{
		int32  ActiveDecals         = 0;   // 이번 프레임에 처리된 UDecalComponent 수
		int32  QueryCandidates      = 0;   // Decal frustum query가 반환한 전체 primitive 수
		int32  StaticMeshCandidates = 0;   // Query 후보 중 StaticMesh primitive 수
		int32  CulledByAABBAxes     = 0;   // SAT 축 4~6 (AABB 면 법선) 에서 제외된 수
		int32  CulledByCrossAxes    = 0;   // SAT 축 7~15 (외적) 에서 제외된 수
		int32  AffectedPrimitives   = 0;   // Draw Call 수 (SAT 전체 통과한 StaticMesh 수)
		double QueryTimeMs          = 0.0; // Decal BVH frustum query CPU 소요 시간 (ms)
		double SATAABBAxesTimeMs    = 0.0; // SAT 축 4~6 필터링 CPU 소요 시간 (ms)
		double SATCrossAxesTimeMs   = 0.0; // SAT 축 7~15 필터링 CPU 소요 시간 (ms)
		double CollectTimeMs        = 0.0; // Decal 충돌 판정(BVH query + SAT) CPU 소요 시간 (ms)
	};

private:
	FMeshBufferManager MeshBufferManager;
	FWorldSpatialIndex::FPrimitiveFrustumQueryScratch FrustumQueryScratch;
	TArray<UPrimitiveComponent*> VisiblePrimitiveScratch;
	FCullingStats LastCullingStats;
	FDecalStats   LastDecalStats;
	FWorldSpatialIndex::FPrimitiveFrustumQueryScratch DecalQueryScratch;
	TArray<UPrimitiveComponent*> DecalCandidateScratch;
	TArray<UStaticMeshComponent*> DecalAffectedMeshScratch;
public:
	void Initialize(ID3D11Device* InDevice) { MeshBufferManager.Create(InDevice); }
	void Release() { MeshBufferManager.Release(); }

	void CollectWorld(UWorld* World, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus,
	                  const FFrustum* ViewFrustum = nullptr);
	void CollectSelection(const TArray<AActor*>& SelectedActors, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus,
	                      FOutlinePostProcessData& OutOutlineData);
	void CollectGizmo(UGizmoComponent* Gizmo, const FShowFlags& ShowFlags, FRenderBus& RenderBus, bool bIsActiveOperation);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus, bool bOrthographic = false);
	const FCullingStats& GetLastCullingStats() const { return LastCullingStats; }
	const FDecalStats&   GetLastDecalStats()   const { return LastDecalStats; }

private:
	void ResetCullingStats();
	void ResetDecalStats();
	void CollectWorldWithFrustum(UWorld* World, const FFrustum& ViewFrustum, const FShowFlags& ShowFlags, EViewMode ViewMode,
	                             FRenderBus& RenderBus);
	void CollectFromActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus);
	bool CollectFromSelectedActor(AActor* Actor, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus);
	void CollectFromComponent(UPrimitiveComponent* Primitive, const FShowFlags& ShowFlags, EViewMode ViewMode, FRenderBus& RenderBus);
	void CollectBVHInternalNodeAABBs(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus,
	                                 std::unordered_set<int32>& SeenNodeIndices);
	void CollectAABBCommand(const FAABB& Box, const FColor& Color, FRenderBus& RenderBus);
	void CollectAABBCommand(UPrimitiveComponent* PrimitiveComponent, const FShowFlags& ShowFlags, FRenderBus& RenderBus);

	FFireBallInfo CollectFireBallInfoFromFireBallComponent(UFireBallComponent* InFireBallComponent);

	FHeightFogInfo CollectHeightFogInfoFromFogComponent(UHeightFogComponent* InHeightFogComponent);
};

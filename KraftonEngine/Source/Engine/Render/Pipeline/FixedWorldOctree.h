#pragma once
#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Render/Pipeline/FrustumCulling.h"
#include "Render/Pipeline/IPrimitiveSpatialQuery.h"

#include <memory>

class FFixedWorldOctree : public IPrimitiveSpatialQuery
{
public:
	//	고정 월드 바운드 기반 Octree (초기 버전: 동적 루트 확장 없음)
	FFixedWorldOctree(
		const FBoundingBox& InWorldBounds = FBoundingBox(FVector(-10000.0f, -10000.0f, -10000.0f), FVector(10000.0f, 10000.0f, 10000.0f)),
		int32 InMaxDepth = 6,
		int32 InMaxItemsPerNode = 16);

	//	프레임 단위 재구축을 위한 초기화
	void Clear() override;
	//	프록시의 월드 AABB를 공간 인덱스에 삽입
	void Insert(FPrimitiveProxy* Proxy, const FBoundingBox& Bounds) override;
	//	프러스텀과 겹치는 후보 프록시만 수집
	void QueryFrustum(const FFrustumPlanes& Frustum, TArray<FPrimitiveProxy*>& OutProxies) const override;
	void QueryRay(const FRay& Ray, TArray<FPrimitiveProxy*>& OutProxies) const override;
	const FSpatialQueryDebugStats& GetLastDebugStats() const override { return LastDebugStats; }

private:
	struct FOctreeItem
	{
		FPrimitiveProxy* Proxy = nullptr;
		FBoundingBox Bounds;
	};

	struct FNode
	{
		explicit FNode(const FBoundingBox& InBounds, int32 InDepth)
			: Bounds(InBounds)
			, Depth(InDepth)
		{
		}

		FBoundingBox Bounds;
		int32 Depth = 0;
		TArray<FOctreeItem> Items;
		std::unique_ptr<FNode> Children[8];

		bool HasChildren() const;
	};

	static bool ContainsAABB(const FBoundingBox& Outer, const FBoundingBox& Inner);
	FBoundingBox BuildChildBounds(const FBoundingBox& Parent, int32 ChildIndex) const;
	int32 GetContainingChildIndex(const FBoundingBox& ParentBounds, const FBoundingBox& ItemBounds) const;
	void EnsureChildren(FNode& Node);
	void InsertNode(FNode& Node, const FOctreeItem& Item);
	void QueryNodeByFrustum(const FNode& Node, const FFrustumPlanes& Frustum, TArray<FPrimitiveProxy*>& OutProxies) const;
	void QueryNodeByRay(const FNode& Node, const FRay& Ray, TArray<FPrimitiveProxy*>& OutProxies) const;
	void AccumulateNodeStats(const FNode& Node, int32& OutNodeCount, int32& OutItemCount) const;

	FBoundingBox WorldBounds;
	int32 MaxDepth = 6;
	int32 MaxItemsPerNode = 16;
	std::unique_ptr<FNode> Root;
	TArray<FOctreeItem> OutsideItems;
	mutable FSpatialQueryDebugStats LastDebugStats;
};

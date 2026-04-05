#include "Render/Pipeline/FixedWorldOctree.h"
#include "Collision/RayUtils.h"


FFixedWorldOctree::FFixedWorldOctree(const FBoundingBox& InWorldBounds, int32 InMaxDepth, int32 InMaxItemsPerNode)
	: WorldBounds(InWorldBounds)
	, MaxDepth(InMaxDepth)
	, MaxItemsPerNode(InMaxItemsPerNode)
{
	Root = std::make_unique<FNode>(WorldBounds, 0);
}

void FFixedWorldOctree::Clear()
{
	Root = std::make_unique<FNode>(WorldBounds, 0);
	OutsideItems.clear();
	LastDebugStats = {};
}

void FFixedWorldOctree::Insert(FPrimitiveProxy* Proxy, const FBoundingBox& Bounds)
{
	if (!Proxy || !Bounds.IsValid())
	{
		return;
	}

	FOctreeItem Item = { Proxy, Bounds };
	//	루트 바운드 밖은 별도 리스트로 보관 (누락 방지)
	if (!ContainsAABB(WorldBounds, Bounds))
	{
		OutsideItems.push_back(Item);
		return;
	}

	InsertNode(*Root, Item);
}

void FFixedWorldOctree::QueryFrustum(const FFrustumPlanes& Frustum, TArray<FPrimitiveProxy*>& OutProxies) const
{
	LastDebugStats = {};
	LastDebugStats.OutsideItems = static_cast<int32>(OutsideItems.size());

	if (!Root)
	{
		return;
	}

	AccumulateNodeStats(*Root, LastDebugStats.TotalNodes, LastDebugStats.TotalItems);
	LastDebugStats.TotalItems += LastDebugStats.OutsideItems;

	QueryNodeByFrustum(*Root, Frustum, OutProxies);

	for (const FOctreeItem& Item : OutsideItems)
	{
		if (FFrustumCulling::IntersectsAABB(Frustum, Item.Bounds))
		{
			OutProxies.push_back(Item.Proxy);
			++LastDebugStats.FrustumCandidateItems;
		}
	}

}

void FFixedWorldOctree::QueryRay(const FRay& Ray, TArray<FPrimitiveProxy*>& OutProxies) const
{
	LastDebugStats = {};
	LastDebugStats.OutsideItems = static_cast<int32>(OutsideItems.size());

	if (!Root)
	{
		return;
	}

	AccumulateNodeStats(*Root, LastDebugStats.TotalNodes, LastDebugStats.TotalItems);
	LastDebugStats.TotalItems += LastDebugStats.OutsideItems;

	QueryNodeByRay(*Root, Ray, OutProxies);

	for (const FOctreeItem& Item : OutsideItems)
	{
		if (FRayUtils::CheckRayAABB(Ray, Item.Bounds.Min, Item.Bounds.Max))
		{
			OutProxies.push_back(Item.Proxy);
			++LastDebugStats.RayCandidateItems;
		}
	}
}

bool FFixedWorldOctree::FNode::HasChildren() const
{
	for (const std::unique_ptr<FNode>& Child : Children)
	{
		if (Child)
		{
			return true;
		}
	}
	return false;
}

void FFixedWorldOctree::AccumulateNodeStats(const FNode& Node, int32& OutNodeCount, int32& OutItemCount) const
{
	++OutNodeCount;
	OutItemCount += static_cast<int32>(Node.Items.size());

	for (const std::unique_ptr<FNode>& Child : Node.Children)
	{
		if (Child)
		{
			AccumulateNodeStats(*Child, OutNodeCount, OutItemCount);
		}
	}
}

bool FFixedWorldOctree::ContainsAABB(const FBoundingBox& Outer, const FBoundingBox& Inner)
{
	return Inner.Min.X >= Outer.Min.X && Inner.Max.X <= Outer.Max.X
		&& Inner.Min.Y >= Outer.Min.Y && Inner.Max.Y <= Outer.Max.Y
		&& Inner.Min.Z >= Outer.Min.Z && Inner.Max.Z <= Outer.Max.Z;
}

FBoundingBox FFixedWorldOctree::BuildChildBounds(const FBoundingBox& Parent, int32 ChildIndex) const
{
	const FVector Center = Parent.GetCenter();

	const bool bUpperX = (ChildIndex & 1) != 0;
	const bool bUpperY = (ChildIndex & 2) != 0;
	const bool bUpperZ = (ChildIndex & 4) != 0;

	const FVector Min(
		bUpperX ? Center.X : Parent.Min.X,
		bUpperY ? Center.Y : Parent.Min.Y,
		bUpperZ ? Center.Z : Parent.Min.Z);

	const FVector Max(
		bUpperX ? Parent.Max.X : Center.X,
		bUpperY ? Parent.Max.Y : Center.Y,
		bUpperZ ? Parent.Max.Z : Center.Z);

	return FBoundingBox(Min, Max);
}

int32 FFixedWorldOctree::GetContainingChildIndex(const FBoundingBox& ParentBounds, const FBoundingBox& ItemBounds) const
{
	//	완전히 포함되는 단일 자식만 선택, 걸치면 부모에 남김
	for (int32 ChildIndex = 0; ChildIndex < 8; ++ChildIndex)
	{
		const FBoundingBox ChildBounds = BuildChildBounds(ParentBounds, ChildIndex);
		if (ContainsAABB(ChildBounds, ItemBounds))
		{
			return ChildIndex;
		}
	}

	return -1;
}

void FFixedWorldOctree::EnsureChildren(FNode& Node)
{
	if (Node.HasChildren())
	{
		return;
	}

	for (int32 ChildIndex = 0; ChildIndex < 8; ++ChildIndex)
	{
		Node.Children[ChildIndex] = std::make_unique<FNode>(BuildChildBounds(Node.Bounds, ChildIndex), Node.Depth + 1);
	}
}

void FFixedWorldOctree::InsertNode(FNode& Node, const FOctreeItem& Item)
{
	if (Node.Depth >= MaxDepth)
	{
		Node.Items.push_back(Item);
		return;
	}

	if (Node.HasChildren())
	{
		const int32 ChildIndex = GetContainingChildIndex(Node.Bounds, Item.Bounds);
		if (ChildIndex >= 0)
		{
			InsertNode(*Node.Children[ChildIndex], Item);
			return;
		}
	}

	Node.Items.push_back(Item);

	if (Node.Items.size() <= static_cast<size_t>(MaxItemsPerNode) || Node.Depth >= MaxDepth)
	{
		return;
	}

	//	Capacity를 넘으면 분할 후, 완전 포함되는 항목만 하위로 재배치
	EnsureChildren(Node);

	auto It = Node.Items.begin();
	while (It != Node.Items.end())
	{
		const int32 ChildIndex = GetContainingChildIndex(Node.Bounds, It->Bounds);
		if (ChildIndex >= 0)
		{
			InsertNode(*Node.Children[ChildIndex], *It);
			It = Node.Items.erase(It);
		}
		else
		{
			++It;
		}
	}
}

void FFixedWorldOctree::QueryNodeByFrustum(const FNode& Node, const FFrustumPlanes& Frustum, TArray<FPrimitiveProxy*>& OutProxies) const
{
	//	노드 단위 early-out으로 하위 순회 비용 절감
	if (!FFrustumCulling::IntersectsAABB(Frustum, Node.Bounds))
	{
		return;
	}

	++LastDebugStats.FrustumIntersectedNodes;

	for (const FOctreeItem& Item : Node.Items)
	{
		if (FFrustumCulling::IntersectsAABB(Frustum, Item.Bounds))
		{
			OutProxies.push_back(Item.Proxy);
			++LastDebugStats.FrustumCandidateItems;
		}
	}

	for (const std::unique_ptr<FNode>& Child : Node.Children)
	{
		if (Child)
		{
			QueryNodeByFrustum(*Child, Frustum, OutProxies);
		}
	}
}

void FFixedWorldOctree::QueryNodeByRay(const FNode& Node, const FRay& Ray, TArray<FPrimitiveProxy*>& OutProxies) const
{
	if (!FRayUtils::CheckRayAABB(Ray, Node.Bounds.Min, Node.Bounds.Max))
	{
		return;
	}

	++LastDebugStats.RayIntersectedNodes;

	for (const FOctreeItem& Item : Node.Items)
	{
		if (FRayUtils::CheckRayAABB(Ray, Item.Bounds.Min, Item.Bounds.Max))
		{
			OutProxies.push_back(Item.Proxy);
			++LastDebugStats.RayCandidateItems;
		}
	}

	for (const std::unique_ptr<FNode>& Child : Node.Children)
	{
		if (Child)
		{
			QueryNodeByRay(*Child, Ray, OutProxies);
		}
	}
}

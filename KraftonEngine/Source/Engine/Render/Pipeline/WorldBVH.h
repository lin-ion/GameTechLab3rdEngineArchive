#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Collision/PickingTuning.h"
#include "Render/Pipeline/FrustumCulling.h"
#include "Render/Pipeline/IPrimitiveSpatialQuery.h"

#include <algorithm>
#include <array>
#include <cfloat>

class FWorldBVH final : public IPrimitiveSpatialQuery
{
public:
	void Clear() override
	{
		Items.clear();
		ItemOrder.clear();
		Nodes.clear();
		OrderedItemProxy.clear();
		OrderedItemMinX.clear(); OrderedItemMinY.clear(); OrderedItemMinZ.clear();
		OrderedItemMaxX.clear(); OrderedItemMaxY.clear(); OrderedItemMaxZ.clear();
		bBuildDirty = false;
		LastDebugStats = {};
	}

	void Insert(FPrimitiveProxy* Proxy, const FBoundingBox& Bounds) override
	{
		if (!Proxy || !Bounds.IsValid())
		{
			return;
		}

		FItem Item;
		Item.Proxy = Proxy;
		Item.Bounds = Bounds;
		Item.Centroid = (Bounds.Min + Bounds.Max) * 0.5f;
		Items.push_back(Item);
		bBuildDirty = true;
	}

	void Warmup() override
	{
		EnsureBuilt();
	}

	void QueryFrustum(const FFrustumPlanes& Frustum, TArray<FPrimitiveProxy*>& OutProxies) const override
	{
		EnsureBuilt();

		LastDebugStats = {};
		LastDebugStats.TotalNodes = static_cast<int32>(Nodes.size());
		LastDebugStats.TotalItems = static_cast<int32>(Items.size());
		if (Nodes.empty())
		{
			return;
		}

		thread_local TArray<int32> Stack;
		Stack.clear();
		Stack.reserve(64);
		Stack.push_back(0);
		while (!Stack.empty())
		{
			const int32 NodeIndex = Stack.back();
			Stack.pop_back();
			if (NodeIndex < 0 || NodeIndex >= static_cast<int32>(Nodes.size()))
			{
				continue;
			}

			const FNode& Node = Nodes[NodeIndex];
			if (!FFrustumCulling::IntersectsAABB(Frustum, Node.Bounds))
			{
				continue;
			}

			++LastDebugStats.FrustumIntersectedNodes;
			if (Node.IsLeaf())
			{
				const uint32 EndIndex = Node.FirstItem + Node.ItemCount;
				for (uint32 Slot = Node.FirstItem; Slot < EndIndex; ++Slot)
				{
					const FItem& Item = Items[ItemOrder[Slot]];
					if (FFrustumCulling::IntersectsAABB(Frustum, Item.Bounds))
					{
						OutProxies.push_back(Item.Proxy);
						++LastDebugStats.FrustumCandidateItems;
					}
				}
				continue;
			}

			if (Node.Left >= 0)
			{
				Stack.push_back(Node.Left);
			}
			if (Node.Right >= 0)
			{
				Stack.push_back(Node.Right);
			}
		}
	}

	void QueryRay(const FRay& Ray, TArray<FPrimitiveProxy*>& OutProxies) const override
	{
		thread_local TArray<FRayQueryCandidate> CandidatesWithNearT;
		CandidatesWithNearT.clear();
		QueryRayWithNearT(Ray, CandidatesWithNearT, FLT_MAX);

		OutProxies.clear();
		OutProxies.reserve(CandidatesWithNearT.size());
		for (const FRayQueryCandidate& Candidate : CandidatesWithNearT)
		{
			OutProxies.push_back(Candidate.Proxy);
		}
	}

	void QueryRayWithNearT(const FRay& Ray, TArray<FRayQueryCandidate>& OutCandidates, float MaxNearT = FLT_MAX) const override
	{
		EnsureBuilt();

		if (RayCandidateReserveHint > 0)
		{
			OutCandidates.reserve(OutCandidates.size() + static_cast<size_t>(RayCandidateReserveHint));
		}

		LastDebugStats = {};
		LastDebugStats.TotalNodes = static_cast<int32>(Nodes.size());
		LastDebugStats.TotalItems = static_cast<int32>(Items.size());
		if (Nodes.empty())
		{
			return;
		}

		const FRayAABBKernel Kernel = FRayAABBKernel::Build(Ray);
		float RootNearT = 0.0f;
		float RootFarT = 0.0f;
		if (!IntersectRayAABBNearT(Ray, Kernel, Nodes[0].Bounds, RootNearT, RootFarT) || RootNearT > MaxNearT)
		{
			return;
		}

		thread_local TArray<FNodeVisit> Stack;
		thread_local int32 StackReserveHint = 64;
		const size_t PreviousCapacity = Stack.capacity();
		Stack.clear();
		if (StackReserveHint > 0)
		{
			Stack.reserve(static_cast<size_t>(StackReserveHint));
		}
		Stack.push_back({ 0, RootNearT });

		while (!Stack.empty())
		{
			const FNodeVisit Visit = Stack.back();
			Stack.pop_back();
			if (Visit.NearT > MaxNearT)
			{
				continue;
			}

			const int32 NodeIndex = Visit.NodeIndex;
			if (NodeIndex < 0 || NodeIndex >= static_cast<int32>(Nodes.size()))
			{
				continue;
			}

			const FNode& Node = Nodes[NodeIndex];
			++LastDebugStats.RayIntersectedNodes;
			if (Node.IsLeaf())
			{
				const uint32 EndIndex = Node.FirstItem + Node.ItemCount;
				for (uint32 Slot = Node.FirstItem; Slot < EndIndex; ++Slot)
				{
					float NearT = 0.0f;
					float FarT = 0.0f;
					if (!IntersectRayAABBNearTMinMax(
						Ray, Kernel,
						OrderedItemMinX[Slot], OrderedItemMinY[Slot], OrderedItemMinZ[Slot],
						OrderedItemMaxX[Slot], OrderedItemMaxY[Slot], OrderedItemMaxZ[Slot],
						NearT, FarT) || NearT > MaxNearT)
					{
						continue;
					}

					OutCandidates.push_back({ OrderedItemProxy[Slot], NearT });
					++LastDebugStats.RayCandidateItems;
				}
				continue;
			}

			float LeftNearT = FLT_MAX;
			float LeftFarT = FLT_MAX;
			float RightNearT = FLT_MAX;
			float RightFarT = FLT_MAX;
			const bool bHitLeft = (Node.Left >= 0) && IntersectRayAABBNearT(Ray, Kernel, Nodes[Node.Left].Bounds, LeftNearT, LeftFarT) && (LeftNearT <= MaxNearT);
			const bool bHitRight = (Node.Right >= 0) && IntersectRayAABBNearT(Ray, Kernel, Nodes[Node.Right].Bounds, RightNearT, RightFarT) && (RightNearT <= MaxNearT);

			if (bHitLeft && bHitRight)
			{
				if (LeftNearT <= RightNearT)
				{
					Stack.push_back({ Node.Right, RightNearT });
					Stack.push_back({ Node.Left, LeftNearT });
				}
				else
				{
					Stack.push_back({ Node.Left, LeftNearT });
					Stack.push_back({ Node.Right, RightNearT });
				}
			}
			else if (bHitLeft)
			{
				Stack.push_back({ Node.Left, LeftNearT });
			}
			else if (bHitRight)
			{
				Stack.push_back({ Node.Right, RightNearT });
			}
		}

		if (LastDebugStats.RayCandidateItems > RayCandidateReserveHint)
		{
			RayCandidateReserveHint = LastDebugStats.RayCandidateItems;
		}
		if (Stack.capacity() > PreviousCapacity)
		{
			StackReserveHint = static_cast<int32>(Stack.capacity());
		}
	}

	const FSpatialQueryDebugStats& GetLastDebugStats() const override
	{
		return LastDebugStats;
	}

private:
	static float ComputeBoundsSurfaceArea(const FBoundingBox& Bounds)
	{
		if (!Bounds.IsValid())
		{
			return 0.0f;
		}

		const FVector Size = Bounds.Max - Bounds.Min;
		const float X = (std::max)(0.0f, Size.X);
		const float Y = (std::max)(0.0f, Size.Y);
		const float Z = (std::max)(0.0f, Size.Z);
		return 2.0f * (X * Y + Y * Z + Z * X);
	}

	static uint32 GetLeafItemThreshold(uint32 ItemCount)
	{
		return (ItemCount >= FPickingTuning::WorldBVHLargeObjectCutoff())
			? FPickingTuning::WorldBVHLeafCountLarge()
			: FPickingTuning::WorldBVHLeafCountSmall();
	}

	struct FItem
	{
		FPrimitiveProxy* Proxy = nullptr;
		FBoundingBox Bounds;
		FVector Centroid;
	};

	struct FNode
	{
		FBoundingBox Bounds;
		int32 Left = -1;
		int32 Right = -1;
		uint32 FirstItem = 0;
		uint32 ItemCount = 0;

		bool IsLeaf() const
		{
			return Left < 0 && Right < 0;
		}
	};

	struct FNodeVisit
	{
		int32 NodeIndex = -1;
		float NearT = FLT_MAX;
	};

	void EnsureBuilt() const
	{
		if (!bBuildDirty)
		{
			return;
		}

		Nodes.clear();
		ItemOrder.clear();
		ItemOrder.reserve(Items.size());

		for (uint32 i = 0; i < static_cast<uint32>(Items.size()); ++i)
		{
			ItemOrder.push_back(i);
		}

		if (!Items.empty())
		{
			BuildNode(0, static_cast<uint32>(Items.size()), 0);
			BuildOrderedItemCache();
		}

		bBuildDirty = false;
	}

	int32 BuildNode(uint32 FirstItem, uint32 ItemCount, uint32 Depth) const
	{
		const int32 NodeIndex = static_cast<int32>(Nodes.size());
		Nodes.emplace_back();

		Nodes[NodeIndex].FirstItem = FirstItem;
		Nodes[NodeIndex].ItemCount = ItemCount;

		FBoundingBox Bounds;
		for (uint32 i = FirstItem; i < FirstItem + ItemCount; ++i)
		{
			const FItem& Item = Items[ItemOrder[i]];
			Bounds.Expand(Item.Bounds.Min);
			Bounds.Expand(Item.Bounds.Max);
		}
		Nodes[NodeIndex].Bounds = Bounds;

		const uint32 MaxLeafItems = GetLeafItemThreshold(ItemCount);
		constexpr uint32 MaxDepth = 40;
		if (ItemCount <= MaxLeafItems || Depth >= MaxDepth)
		{
			return NodeIndex;
		}

		FBoundingBox CentroidBounds;
		for (uint32 i = FirstItem; i < FirstItem + ItemCount; ++i)
		{
			CentroidBounds.Expand(Items[ItemOrder[i]].Centroid);
		}
		const FVector Extent = CentroidBounds.GetExtent();
		if (Extent.X < 1e-6f && Extent.Y < 1e-6f && Extent.Z < 1e-6f)
		{
			return NodeIndex;
		}

		constexpr int32 BinCount = 12;
		float BestCost = FLT_MAX;
		int32 BestAxis = -1;
		int32 BestSplitBin = -1;
		float BestAxisMin = 0.0f;
		float BestAxisScale = 0.0f;

		const bool bUseSAH = FPickingTuning::UseWorldBVHSAH();
		for (int32 Axis = 0; Axis < 3 && bUseSAH; ++Axis)
		{
			const float AxisMin = CentroidBounds.Min.Data[Axis];
			const float AxisMax = CentroidBounds.Max.Data[Axis];
			const float AxisRange = AxisMax - AxisMin;
			if (AxisRange < 1e-6f)
			{
				continue;
			}

			struct FBin
			{
				uint32 Count = 0;
				FBoundingBox Bounds;
			};
			std::array<FBin, BinCount> Bins;
			const float AxisScale = static_cast<float>(BinCount) / AxisRange;
			for (uint32 i = FirstItem; i < FirstItem + ItemCount; ++i)
			{
				const FItem& Item = Items[ItemOrder[i]];
				int32 BinIndex = static_cast<int32>((Item.Centroid.Data[Axis] - AxisMin) * AxisScale);
				if (BinIndex < 0) BinIndex = 0;
				if (BinIndex >= BinCount) BinIndex = BinCount - 1;

				FBin& Bin = Bins[BinIndex];
				++Bin.Count;
				Bin.Bounds.Expand(Item.Bounds.Min);
				Bin.Bounds.Expand(Item.Bounds.Max);
			}

			std::array<uint32, BinCount> PrefixCount = {};
			std::array<FBoundingBox, BinCount> PrefixBounds;
			std::array<uint32, BinCount> SuffixCount = {};
			std::array<FBoundingBox, BinCount> SuffixBounds;

			uint32 RunningPrefixCount = 0;
			FBoundingBox RunningPrefixBounds;
			for (int32 i = 0; i < BinCount; ++i)
			{
				RunningPrefixCount += Bins[i].Count;
				if (Bins[i].Count > 0)
				{
					RunningPrefixBounds.Expand(Bins[i].Bounds.Min);
					RunningPrefixBounds.Expand(Bins[i].Bounds.Max);
				}
				PrefixCount[i] = RunningPrefixCount;
				PrefixBounds[i] = RunningPrefixBounds;
			}

			uint32 RunningSuffixCount = 0;
			FBoundingBox RunningSuffixBounds;
			for (int32 i = BinCount - 1; i >= 0; --i)
			{
				RunningSuffixCount += Bins[i].Count;
				if (Bins[i].Count > 0)
				{
					RunningSuffixBounds.Expand(Bins[i].Bounds.Min);
					RunningSuffixBounds.Expand(Bins[i].Bounds.Max);
				}
				SuffixCount[i] = RunningSuffixCount;
				SuffixBounds[i] = RunningSuffixBounds;
			}

			for (int32 SplitBin = 0; SplitBin < BinCount - 1; ++SplitBin)
			{
				const uint32 LeftCount = PrefixCount[SplitBin];
				const uint32 RightCount = SuffixCount[SplitBin + 1];
				if (LeftCount == 0 || RightCount == 0)
				{
					continue;
				}

				const float LeftArea = ComputeBoundsSurfaceArea(PrefixBounds[SplitBin]);
				const float RightArea = ComputeBoundsSurfaceArea(SuffixBounds[SplitBin + 1]);
				const float Cost = LeftArea * static_cast<float>(LeftCount) + RightArea * static_cast<float>(RightCount);
				if (Cost < BestCost)
				{
					BestCost = Cost;
					BestAxis = Axis;
					BestSplitBin = SplitBin;
					BestAxisMin = AxisMin;
					BestAxisScale = AxisScale;
				}
			}
		}

		uint32 Mid = FirstItem + ItemCount / 2;
		if (bUseSAH && BestAxis >= 0 && BestSplitBin >= 0)
		{
			auto MidIt = std::partition(
				ItemOrder.begin() + FirstItem,
				ItemOrder.begin() + (FirstItem + ItemCount),
				[&](uint32 ItemIdx)
				{
					int32 BinIndex = static_cast<int32>((Items[ItemIdx].Centroid.Data[BestAxis] - BestAxisMin) * BestAxisScale);
					if (BinIndex < 0) BinIndex = 0;
					if (BinIndex >= BinCount) BinIndex = BinCount - 1;
					return BinIndex <= BestSplitBin;
				});
			Mid = static_cast<uint32>(MidIt - ItemOrder.begin());
		}
		else
		{
			int32 Axis = 0;
			if (Extent.Y > Extent.X && Extent.Y >= Extent.Z)
			{
				Axis = 1;
			}
			else if (Extent.Z > Extent.X && Extent.Z >= Extent.Y)
			{
				Axis = 2;
			}

			std::nth_element(
				ItemOrder.begin() + FirstItem,
				ItemOrder.begin() + Mid,
				ItemOrder.begin() + (FirstItem + ItemCount),
				[&](uint32 Lhs, uint32 Rhs)
				{
					return Items[Lhs].Centroid.Data[Axis] < Items[Rhs].Centroid.Data[Axis];
				});
		}

		const uint32 LeftCount = Mid - FirstItem;
		const uint32 RightCount = ItemCount - LeftCount;
		if (LeftCount == 0 || RightCount == 0)
		{
			return NodeIndex;
		}

		Nodes[NodeIndex].Left = BuildNode(FirstItem, LeftCount, Depth + 1);
		Nodes[NodeIndex].Right = BuildNode(Mid, RightCount, Depth + 1);
		return NodeIndex;
	}

	void BuildOrderedItemCache() const
	{
		OrderedItemProxy.clear();
		OrderedItemMinX.clear(); OrderedItemMinY.clear(); OrderedItemMinZ.clear();
		OrderedItemMaxX.clear(); OrderedItemMaxY.clear(); OrderedItemMaxZ.clear();

		OrderedItemProxy.reserve(ItemOrder.size());
		OrderedItemMinX.reserve(ItemOrder.size()); OrderedItemMinY.reserve(ItemOrder.size()); OrderedItemMinZ.reserve(ItemOrder.size());
		OrderedItemMaxX.reserve(ItemOrder.size()); OrderedItemMaxY.reserve(ItemOrder.size()); OrderedItemMaxZ.reserve(ItemOrder.size());

		for (uint32 ItemIdx : ItemOrder)
		{
			const FItem& Item = Items[ItemIdx];
			OrderedItemProxy.push_back(Item.Proxy);
			OrderedItemMinX.push_back(Item.Bounds.Min.X);
			OrderedItemMinY.push_back(Item.Bounds.Min.Y);
			OrderedItemMinZ.push_back(Item.Bounds.Min.Z);
			OrderedItemMaxX.push_back(Item.Bounds.Max.X);
			OrderedItemMaxY.push_back(Item.Bounds.Max.Y);
			OrderedItemMaxZ.push_back(Item.Bounds.Max.Z);
		}
	}

	mutable TArray<FItem> Items;
	mutable TArray<uint32> ItemOrder;
	mutable TArray<FNode> Nodes;
	mutable TArray<FPrimitiveProxy*> OrderedItemProxy;
	mutable TArray<float> OrderedItemMinX, OrderedItemMinY, OrderedItemMinZ;
	mutable TArray<float> OrderedItemMaxX, OrderedItemMaxY, OrderedItemMaxZ;
	mutable bool bBuildDirty = false;
	mutable int32 RayCandidateReserveHint = 0;
	mutable FSpatialQueryDebugStats LastDebugStats;
};

#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

struct FStaticMeshBVH
{
	struct FTriangleRef
	{
		uint32 I0 = 0;
		uint32 I1 = 0;
		uint32 I2 = 0;
		uint32 FirstIndex = 0;
		FBoundingBox Bounds;
		FVector Centroid;
	};

	struct FNode
	{
		FBoundingBox Bounds;
		int32 Left = -1;
		int32 Right = -1;
		uint32 FirstTri = 0;
		uint32 TriCount = 0;

		bool IsLeaf() const
		{
			return Left < 0 && Right < 0;
		}
	};

	void Build(const void* PositionData, uint32 PositionStride, const TArray<uint32>& InIndices)
	{
		Triangles.clear();
		TriOrder.clear();
		Nodes.clear();

		if (!PositionData || PositionStride == 0 || InIndices.size() < 3)
		{
			return;
		}

		Triangles.reserve(InIndices.size() / 3);
		TriOrder.reserve(InIndices.size() / 3);

		const uint8* BasePtr = static_cast<const uint8*>(PositionData);
		for (uint32 i = 0; i + 2 < static_cast<uint32>(InIndices.size()); i += 3)
		{
			const uint32 I0 = InIndices[i];
			const uint32 I1 = InIndices[i + 1];
			const uint32 I2 = InIndices[i + 2];

			const FVector& V0 = *reinterpret_cast<const FVector*>(BasePtr + static_cast<size_t>(I0) * PositionStride);
			const FVector& V1 = *reinterpret_cast<const FVector*>(BasePtr + static_cast<size_t>(I1) * PositionStride);
			const FVector& V2 = *reinterpret_cast<const FVector*>(BasePtr + static_cast<size_t>(I2) * PositionStride);

			FTriangleRef Tri;
			Tri.I0 = I0;
			Tri.I1 = I1;
			Tri.I2 = I2;
			Tri.FirstIndex = i;
			Tri.Bounds = FBoundingBox(V0, V0);
			Tri.Bounds.Expand(V1);
			Tri.Bounds.Expand(V2);
			Tri.Centroid = (V0 + V1 + V2) / 3.0f;
			Triangles.push_back(Tri);
			TriOrder.push_back(static_cast<uint32>(TriOrder.size()));
		}

		if (!Triangles.empty())
		{
			BuildNode(0, static_cast<uint32>(Triangles.size()), 0);
		}
	}

	bool IntersectLocalRay(const FRay& LocalRay, const void* PositionData, uint32 PositionStride, FHitResult& OutHitResult) const
	{
		if (Nodes.empty() || Triangles.empty() || !PositionData || PositionStride == 0)
		{
			OutHitResult = {};
			return false;
		}

		const uint8* BasePtr = static_cast<const uint8*>(PositionData);
		TArray<int32> Stack;
		Stack.push_back(0);

		bool bHit = false;
		float ClosestT = FLT_MAX;
		int32 HitFaceIndex = -1;

		while (!Stack.empty())
		{
			const int32 NodeIndex = Stack.back();
			Stack.pop_back();

			if (NodeIndex < 0 || NodeIndex >= static_cast<int32>(Nodes.size()))
			{
				continue;
			}

			const FNode& Node = Nodes[NodeIndex];
			float NodeNearT = 0.0f;
			float NodeFarT = 0.0f;
			if (!IntersectRayAABB(LocalRay, Node.Bounds, NodeNearT, NodeFarT))
			{
				continue;
			}
			if (NodeNearT >= ClosestT)
			{
				continue;
			}

			if (Node.IsLeaf())
			{
				const uint32 EndTri = Node.FirstTri + Node.TriCount;
				for (uint32 TriSlot = Node.FirstTri; TriSlot < EndTri; ++TriSlot)
				{
					const FTriangleRef& Tri = Triangles[TriOrder[TriSlot]];
					const FVector& V0 = *reinterpret_cast<const FVector*>(BasePtr + static_cast<size_t>(Tri.I0) * PositionStride);
					const FVector& V1 = *reinterpret_cast<const FVector*>(BasePtr + static_cast<size_t>(Tri.I1) * PositionStride);
					const FVector& V2 = *reinterpret_cast<const FVector*>(BasePtr + static_cast<size_t>(Tri.I2) * PositionStride);

					float T = 0.0f;
					if (IntersectTriangle(LocalRay.Origin, LocalRay.Direction, V0, V1, V2, T) && T < ClosestT)
					{
						ClosestT = T;
						HitFaceIndex = static_cast<int32>(Tri.FirstIndex);
						bHit = true;
					}
				}
			}
			else
			{
				float LeftNearT = FLT_MAX;
				float LeftFarT = FLT_MAX;
				float RightNearT = FLT_MAX;
				float RightFarT = FLT_MAX;
				const bool bHitLeft = (Node.Left >= 0) && IntersectRayAABB(LocalRay, Nodes[Node.Left].Bounds, LeftNearT, LeftFarT);
				const bool bHitRight = (Node.Right >= 0) && IntersectRayAABB(LocalRay, Nodes[Node.Right].Bounds, RightNearT, RightFarT);

				if (bHitLeft && LeftNearT < ClosestT && bHitRight && RightNearT < ClosestT)
				{
					if (LeftNearT <= RightNearT)
					{
						Stack.push_back(Node.Right);
						Stack.push_back(Node.Left);
					}
					else
					{
						Stack.push_back(Node.Left);
						Stack.push_back(Node.Right);
					}
				}
				else if (bHitLeft && LeftNearT < ClosestT)
				{
					Stack.push_back(Node.Left);
				}
				else if (bHitRight && RightNearT < ClosestT)
				{
					Stack.push_back(Node.Right);
				}
			}
		}

		OutHitResult = {};
		OutHitResult.bHit = bHit;
		if (bHit)
		{
			OutHitResult.Distance = ClosestT;
			OutHitResult.FaceIndex = HitFaceIndex;
		}
		return bHit;
	}

	bool IsBuilt() const
	{
		return !Nodes.empty();
	}

private:
	int32 BuildNode(uint32 FirstTri, uint32 TriCount, uint32 Depth)
	{
		const int32 NodeIndex = static_cast<int32>(Nodes.size());
		Nodes.emplace_back();

		Nodes[NodeIndex].FirstTri = FirstTri;
		Nodes[NodeIndex].TriCount = TriCount;

		FBoundingBox Bounds;
		for (uint32 i = FirstTri; i < FirstTri + TriCount; ++i)
		{
			Bounds.Expand(Triangles[TriOrder[i]].Bounds.Min);
			Bounds.Expand(Triangles[TriOrder[i]].Bounds.Max);
		}
		Nodes[NodeIndex].Bounds = Bounds;

		constexpr uint32 MaxLeafTriangles = 16;
		constexpr uint32 MaxDepth = 32;
		if (TriCount <= MaxLeafTriangles || Depth >= MaxDepth)
		{
			return NodeIndex;
		}

		FBoundingBox CentroidBounds;
		for (uint32 i = FirstTri; i < FirstTri + TriCount; ++i)
		{
			CentroidBounds.Expand(Triangles[TriOrder[i]].Centroid);
		}
		FVector CentroidExtent = CentroidBounds.GetExtent();

		int32 Axis = 0;
		if (CentroidExtent.Y > CentroidExtent.X && CentroidExtent.Y >= CentroidExtent.Z)
		{
			Axis = 1;
		}
		else if (CentroidExtent.Z > CentroidExtent.X && CentroidExtent.Z >= CentroidExtent.Y)
		{
			Axis = 2;
		}

		const float MinAxis = CentroidBounds.Min.Data[Axis];
		const float MaxAxis = CentroidBounds.Max.Data[Axis];
		if (std::abs(MaxAxis - MinAxis) < 1e-6f)
		{
			return NodeIndex;
		}

		const uint32 Mid = FirstTri + TriCount / 2;
		std::nth_element(
			TriOrder.begin() + FirstTri,
			TriOrder.begin() + Mid,
			TriOrder.begin() + (FirstTri + TriCount),
			[&](uint32 LHS, uint32 RHS)
			{
				return Triangles[LHS].Centroid.Data[Axis] < Triangles[RHS].Centroid.Data[Axis];
			});

		const uint32 LeftCount = Mid - FirstTri;
		const uint32 RightCount = TriCount - LeftCount;
		if (LeftCount == 0 || RightCount == 0)
		{
			return NodeIndex;
		}

		const int32 LeftNode = BuildNode(FirstTri, LeftCount, Depth + 1);
		const int32 RightNode = BuildNode(Mid, RightCount, Depth + 1);
		Nodes[NodeIndex].Left = LeftNode;
		Nodes[NodeIndex].Right = RightNode;
		return NodeIndex;
	}

	static bool IntersectRayAABB(const FRay& Ray, const FBoundingBox& AABB, float& OutNearT, float& OutFarT)
	{
		float TMin = -FLT_MAX;
		float TMax = FLT_MAX;

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float Origin = Ray.Origin.Data[Axis];
			const float Direction = Ray.Direction.Data[Axis];
			const float MinV = AABB.Min.Data[Axis];
			const float MaxV = AABB.Max.Data[Axis];

			if (std::abs(Direction) < 1e-8f)
			{
				if (Origin < MinV || Origin > MaxV)
				{
					return false;
				}
				continue;
			}

			const float InvDir = 1.0f / Direction;
			float T1 = (MinV - Origin) * InvDir;
			float T2 = (MaxV - Origin) * InvDir;
			if (T1 > T2)
			{
				std::swap(T1, T2);
			}

			TMin = (std::max)(TMin, T1);
			TMax = (std::min)(TMax, T2);
			if (TMin > TMax)
			{
				return false;
			}
		}

		if (TMax < 0.0f)
		{
			return false;
		}

		OutNearT = (TMin > 0.0f) ? TMin : 0.0f;
		OutFarT = TMax;
		return true;
	}

	static bool IntersectTriangle(const FVector& RayOrigin, const FVector& RayDir,
		const FVector& V0, const FVector& V1, const FVector& V2, float& OutT)
	{
		const FVector Edge1 = V1 - V0;
		const FVector Edge2 = V2 - V0;
		const FVector PVec = RayDir.Cross(Edge2);
		const float Det = Edge1.Dot(PVec);
		if (Det <= 0.0001f)
		{
			return false;
		}

		const float InvDet = 1.0f / Det;
		const FVector TVec = RayOrigin - V0;
		const float U = TVec.Dot(PVec) * InvDet;
		if (U < 0.0f || U > 1.0f)
		{
			return false;
		}

		const FVector QVec = TVec.Cross(Edge1);
		const float V = RayDir.Dot(QVec) * InvDet;
		if (V < 0.0f || U + V > 1.0f)
		{
			return false;
		}

		OutT = Edge2.Dot(QVec) * InvDet;
		return OutT > 0.0f;
	}

	TArray<FTriangleRef> Triangles;
	TArray<uint32> TriOrder;
	TArray<FNode> Nodes;
};

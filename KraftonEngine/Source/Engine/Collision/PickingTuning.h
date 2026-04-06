#pragma once

#include "Core/CoreTypes.h"
#include "Core/RayTypes.h"
#include "Core/EngineTypes.h"

#include <cfloat>
#include <cmath>
#include <algorithm>

struct FPickingTuning
{
	static float& RayParallelEpsilon()
	{
		static float Value = 1e-8f;
		return Value;
	}

	static float& NearTEpsilon()
	{
		static float Value = 1e-4f;
		return Value;
	}

	static float& TriangleDetEpsilon()
	{
		static float Value = 1e-4f;
		return Value;
	}

	static bool& UseBackFaceCull()
	{
		static bool Value = true;
		return Value;
	}

	static bool& UseWorldBVHSAH()
	{
		static bool Value = true;
		return Value;
	}

	static bool& UseStaticMeshBVHSAH()
	{
		static bool Value = true;
		return Value;
	}

	static uint32& BroadLinearVisibleThreshold()
	{
		static uint32 Value = 128u;
		return Value;
	}

	static uint32& WorldBVHLeafCountSmall()
	{
		static uint32 Value = 6u;
		return Value;
	}

	static uint32& WorldBVHLeafCountLarge()
	{
		static uint32 Value = 8u;
		return Value;
	}

	static uint32& WorldBVHLargeObjectCutoff()
	{
		static uint32 Value = 2048u;
		return Value;
	}

	static uint32& StaticMeshBVHLeafCountSmall()
	{
		static uint32 Value = 12u;
		return Value;
	}

	static uint32& StaticMeshBVHLeafCountLarge()
	{
		static uint32 Value = 16u;
		return Value;
	}

	static uint32& StaticMeshBVHLargeObjectCutoff()
	{
		static uint32 Value = 4096u;
		return Value;
	}
};

struct FRayAABBKernel
{
	FVector InvDir = FVector(0, 0, 0);
	bool bParallelAxis[3] = { true, true, true };

	static FRayAABBKernel Build(const FRay& Ray)
	{
		FRayAABBKernel Kernel;
		const float Eps = FPickingTuning::RayParallelEpsilon();

		Kernel.bParallelAxis[0] = std::abs(Ray.Direction.X) <= Eps;
		Kernel.bParallelAxis[1] = std::abs(Ray.Direction.Y) <= Eps;
		Kernel.bParallelAxis[2] = std::abs(Ray.Direction.Z) <= Eps;
		Kernel.InvDir.X = Kernel.bParallelAxis[0] ? 0.0f : (1.0f / Ray.Direction.X);
		Kernel.InvDir.Y = Kernel.bParallelAxis[1] ? 0.0f : (1.0f / Ray.Direction.Y);
		Kernel.InvDir.Z = Kernel.bParallelAxis[2] ? 0.0f : (1.0f / Ray.Direction.Z);
		return Kernel;
	}
};

inline bool IntersectRayAABBNearT(const FRay& Ray, const FRayAABBKernel& Kernel, const FBoundingBox& Box, float& OutNearT, float& OutFarT)
{
	float TMin = -FLT_MAX;
	float TMax = FLT_MAX;

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const float Origin = Ray.Origin.Data[Axis];
		const float MinV = Box.Min.Data[Axis];
		const float MaxV = Box.Max.Data[Axis];

		if (Kernel.bParallelAxis[Axis])
		{
			if (Origin < MinV || Origin > MaxV)
			{
				return false;
			}
			continue;
		}

		float T1 = (MinV - Origin) * Kernel.InvDir.Data[Axis];
		float T2 = (MaxV - Origin) * Kernel.InvDir.Data[Axis];
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

	OutNearT = (std::max)(0.0f, TMin);
	OutFarT = TMax;
	return true;
}

inline bool IntersectRayAABBNearTMinMax(
	const FRay& Ray,
	const FRayAABBKernel& Kernel,
	float MinX, float MinY, float MinZ,
	float MaxX, float MaxY, float MaxZ,
	float& OutNearT,
	float& OutFarT)
{
	float TMin = -FLT_MAX;
	float TMax = FLT_MAX;

	const float Origin[3] = { Ray.Origin.X, Ray.Origin.Y, Ray.Origin.Z };
	const float MinV[3] = { MinX, MinY, MinZ };
	const float MaxV[3] = { MaxX, MaxY, MaxZ };

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (Kernel.bParallelAxis[Axis])
		{
			if (Origin[Axis] < MinV[Axis] || Origin[Axis] > MaxV[Axis])
			{
				return false;
			}
			continue;
		}

		float T1 = (MinV[Axis] - Origin[Axis]) * Kernel.InvDir.Data[Axis];
		float T2 = (MaxV[Axis] - Origin[Axis]) * Kernel.InvDir.Data[Axis];
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

	OutNearT = (std::max)(0.0f, TMin);
	OutFarT = TMax;
	return true;
}

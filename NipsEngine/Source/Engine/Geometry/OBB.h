#pragma once

#include "Engine/Math/Vector.h"
#include "Engine/Geometry/AABB.h"
#include "Engine/Core/Containers/StaticArray.h"

struct FOBB
{
	FVector Center{};
	FVector Extents{};
	TStaticArray<FVector, 3> Axes{};

	FOBB() = default;
	FOBB(const FVector& InCenter, const FVector& InExtents, const TStaticArray<FVector, 3>& InAxes);

	FAABB ToAABB() const;
	void GetCorners(TStaticArray<FVector, 8>& OutCorners) const;
	bool IntersectAABBWithSAT(const FAABB& AABB, bool bTestAABBAxes, bool bTestOBBAxes, bool bTestCrossAxes) const;

private:
	static bool TestAABBAxes(const FVector& T, const FVector& AABBExtents, const FVector& OBBExtents, const TStaticArray<FVector, 3>& OBBAxes);
	static bool TestOBBAxes(const FVector& T, const FVector& AABBExtents, const FVector& OBBExtents, const TStaticArray<FVector, 3>& OBBAxes);
	static bool TestCrossAxes(const FVector& T, const FVector& AABBExtents, const FVector& OBBExtents, const TStaticArray<FVector, 3>& OBBAxes);
};

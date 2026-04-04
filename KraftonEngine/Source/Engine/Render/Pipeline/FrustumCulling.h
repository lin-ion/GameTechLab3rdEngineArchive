#pragma once
#pragma once

#include "Core/EngineTypes.h"
#include "Math/Matrix.h"

struct FPlane
{
	//	평면 방정식: Normal * X + Distance = 0
	FVector Normal;
	float Distance = 0.0f;

	void Normalize();
	float SignedDistance(const FVector& Point) const;
};

struct FFrustumPlanes
{
	//	Left, Right, Bottom, Top, Near, Far 순서
	FPlane Planes[6];
};

class FFrustumCulling
{
public:
	//	ViewProj로 월드 공간 프러스텀 평면 6개를 생성
	static FFrustumPlanes BuildFrustumPlanes(const FMatrix& View, const FMatrix& Proj);
	//	AABB가 프러스텀과 교차하면 true (완전 외부면 false)
	static bool IntersectsAABB(const FFrustumPlanes& Frustum, const FBoundingBox& Box);
};

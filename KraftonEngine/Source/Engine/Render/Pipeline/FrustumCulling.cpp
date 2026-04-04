#include "Render/Pipeline/FrustumCulling.h"
#include "Render/Pipeline/FrustumCulling.h"
void FPlane::Normalize()
{
	//	거리 계산 안정성을 위해 평면 법선을 단위 벡터로 정규화
	const float LenSq = Normal.Dot(Normal);
	if (LenSq <= 1e-8f)
	{
		return;
	}

	const float InvLen = 1.0f / std::sqrt(LenSq);
	Normal *= InvLen;
	Distance *= InvLen;
}

float FPlane::SignedDistance(const FVector& Point) const
{
	return Normal.Dot(Point) + Distance;
}

FFrustumPlanes FFrustumCulling::BuildFrustumPlanes(const FMatrix& View, const FMatrix& Proj)
{
	//	현재 엔진의 행렬 방향에 맞춰 View * Proj 기준으로 평면 추출
	const FMatrix VP = View * Proj;
	FFrustumPlanes Frustum = {};

	Frustum.Planes[0].Normal = FVector(VP.M[0][3] + VP.M[0][0], VP.M[1][3] + VP.M[1][0], VP.M[2][3] + VP.M[2][0]);
	Frustum.Planes[0].Distance = VP.M[3][3] + VP.M[3][0];

	Frustum.Planes[1].Normal = FVector(VP.M[0][3] - VP.M[0][0], VP.M[1][3] - VP.M[1][0], VP.M[2][3] - VP.M[2][0]);
	Frustum.Planes[1].Distance = VP.M[3][3] - VP.M[3][0];

	Frustum.Planes[2].Normal = FVector(VP.M[0][3] + VP.M[0][1], VP.M[1][3] + VP.M[1][1], VP.M[2][3] + VP.M[2][1]);
	Frustum.Planes[2].Distance = VP.M[3][3] + VP.M[3][1];

	Frustum.Planes[3].Normal = FVector(VP.M[0][3] - VP.M[0][1], VP.M[1][3] - VP.M[1][1], VP.M[2][3] - VP.M[2][1]);
	Frustum.Planes[3].Distance = VP.M[3][3] - VP.M[3][1];

	Frustum.Planes[4].Normal = FVector(VP.M[0][2], VP.M[1][2], VP.M[2][2]);
	Frustum.Planes[4].Distance = VP.M[3][2];

	Frustum.Planes[5].Normal = FVector(VP.M[0][3] - VP.M[0][2], VP.M[1][3] - VP.M[1][2], VP.M[2][3] - VP.M[2][2]);
	Frustum.Planes[5].Distance = VP.M[3][3] - VP.M[3][2];

	// 평면의 inside 방향을 안정화: 카메라 전방의 한 점이 항상 inside(>=0)가 되도록 보정
	const FMatrix InvView = View.GetInverseFast();
	const FVector CameraPos = InvView.GetLocation();
	FVector CameraForward = InvView.TransformVector(FVector(0.0f, 0.0f, 1.0f));
	CameraForward.Normalize();
	const float NearZ = (std::fabsf(Proj.M[2][2]) > 1e-6f) ? (-Proj.M[3][2] / Proj.M[2][2]) : 0.1f;
	const FVector InsidePoint = CameraPos + CameraForward * (NearZ + 1.0f);

	for (FPlane& Plane : Frustum.Planes)
	{
		//	추출한 평면은 길이가 다를 수 있으므로 모두 정규화
		Plane.Normalize();
		if (Plane.SignedDistance(InsidePoint) < 0.0f)
		{
			Plane.Normal *= -1.0f;
			Plane.Distance *= -1.0f;
		}
	}

	return Frustum;
}

bool FFrustumCulling::IntersectsAABB(const FFrustumPlanes& Frustum, const FBoundingBox& Box)
{
	if (!Box.IsValid())
	{
		return false;
	}

	//	각 평면 기준 Positive Vertex 검사로 빠르게 outside 판정
	for (const FPlane& Plane : Frustum.Planes)
	{
		const FVector Positive(
			(Plane.Normal.X >= 0.0f) ? Box.Max.X : Box.Min.X,
			(Plane.Normal.Y >= 0.0f) ? Box.Max.Y : Box.Min.Y,
			(Plane.Normal.Z >= 0.0f) ? Box.Max.Z : Box.Min.Z);

		if (Plane.SignedDistance(Positive) < 0.0f)
		{
			return false;
		}
	}

	return true;
}

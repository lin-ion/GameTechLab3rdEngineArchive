#include "OBB.h"
#include "Engine/Math/Matrix.h"

constexpr float SATParallelAxisEpsilonSq = 1e-6f;

FOBB::FOBB(const FVector& InCenter, const FVector& InExtents, const TStaticArray<FVector, 3>& InAxes)
	: Center(InCenter), Extents(InExtents), Axes(InAxes)
{
}

bool FOBB::TestAABBAxes(const FVector& T, const FVector& AABBExtents, const FVector& OBBExtents, const TStaticArray<FVector, 3>& OBBAxes)
{
	// AABB의 로컬 축(월드 축) 3개에 대해 검사
	for (uint32 i = 0; i < 3; ++i)
	{
		// R_A : AABB의 해당 축 방향 투영 반지름
		float R = AABBExtents[i];

		// R_B : OBB를 AABB 축에 투영
		// OBB의 각 로컬 축이 AABB 축에 기여하는 정도를 계산
		for (uint32 j = 0; j < 3; ++j)
		{
			// AABBAxis는 월드 단위축이므로 Dot(AABBAxis[i], OBBAxis[j]) == OBBAxis[j][i]
			R += std::abs(OBBExtents[j] * OBBAxes[j][i]);
		}

		if (std::abs(T[i]) > R) { return false; }
	}
	return true;
}

bool FOBB::TestOBBAxes(const FVector& T, const FVector& AABBExtents, const FVector& OBBExtents, const TStaticArray<FVector, 3>& OBBAxes)
{
	// OBB의 기저벡터 3축에 대해 검사
	for (uint32 i = 0; i < 3; ++i)
	{
		const FVector& OBBAxis = OBBAxes[i].GetSafeNormal();
		// R_B : OBB의 해당 축 방향 반지름
		float R = OBBExtents[i];

		// R_A : AABB를 OBB 축에 투영
		R += std::abs(AABBExtents.X * OBBAxis.X);
		R += std::abs(AABBExtents.Y * OBBAxis.Y);
		R += std::abs(AABBExtents.Z * OBBAxis.Z);

		// T를 OBB 축에 투영
		const float ProjectedT = std::abs(FVector::DotProduct(T, OBBAxis));
		if (ProjectedT > R) { return false; }
	}
	return true;
}

bool FOBB::TestCrossAxes(const FVector& T, const FVector& AABBExtents, const FVector& OBBExtents, const TStaticArray<FVector, 3>& OBBAxes)
{
	const TStaticArray<FVector, 3> AABBAxes = { FVector::UnitX(), FVector::UnitY(), FVector::UnitZ() };

	// AABB 축과 OBB 축의 외적 9개 축에 대해 검사
	for (uint32 i = 0; i < 3; ++i)
	{
		for (uint32 j = 0; j < 3; ++j)
		{
			const FVector LRaw = FVector::CrossProduct(AABBAxes[i], OBBAxes[j]);
			// 두 축이 거의 평행하면 무시 (OBB/AABB 축 검사에서 이미 처리됨)
			if (LRaw.SizeSquared() < SATParallelAxisEpsilonSq) { continue; }
			const FVector L = LRaw.GetSafeNormal(); // 반드시 정규화 후 투영

			float R = 0.0f;
			// R_A : AABB를 외적 축 L에 투영
			R += std::abs(AABBExtents[0] * L[0]);
			R += std::abs(AABBExtents[1] * L[1]);
			R += std::abs(AABBExtents[2] * L[2]);
			// R_B : OBB를 외적 축 L에 투영
			R += std::abs(OBBExtents[0] * FVector::DotProduct(L, OBBAxes[0]));
			R += std::abs(OBBExtents[1] * FVector::DotProduct(L, OBBAxes[1]));
			R += std::abs(OBBExtents[2] * FVector::DotProduct(L, OBBAxes[2]));

			// T를 축 L에 투영
			const float ProjectedT = std::abs(FVector::DotProduct(T, L));
			if (ProjectedT > R) { return false; }
		}
	}
	return true;
}

FAABB FOBB::ToAABB() const
{
	FVector WorldExtents = FVector::Zero();
	for (uint32 i = 0; i < 3; ++i)
	{
		WorldExtents.X += std::abs(Extents[i] * Axes[i].X);
		WorldExtents.Y += std::abs(Extents[i] * Axes[i].Y);
		WorldExtents.Z += std::abs(Extents[i] * Axes[i].Z);
	}

	return FAABB(Center - WorldExtents, Center + WorldExtents);
}

void FOBB::GetCorners(TStaticArray<FVector, 8>& OutCorners) const
{
	const FVector AxisX = Axes[0] * Extents.X;
	const FVector AxisY = Axes[1] * Extents.Y;
	const FVector AxisZ = Axes[2] * Extents.Z;

	OutCorners[0] = Center + AxisX + AxisY + AxisZ;
	OutCorners[1] = Center + AxisX + AxisY - AxisZ;
	OutCorners[2] = Center + AxisX - AxisY + AxisZ;
	OutCorners[3] = Center + AxisX - AxisY - AxisZ;
	OutCorners[4] = Center - AxisX + AxisY + AxisZ;
	OutCorners[5] = Center - AxisX + AxisY - AxisZ;
	OutCorners[6] = Center - AxisX - AxisY + AxisZ;
	OutCorners[7] = Center - AxisX - AxisY - AxisZ;
}

// 교차 시 True, 분리 가능 축이 존재하면 False 반환
bool FOBB::IntersectAABBWithSAT(const FAABB& AABB, bool bTestAABBAxes, bool bTestOBBAxes, bool bTestCrossAxes) const
{
	// Separating Axis Theorem
	// 옵션에 따라 AABB 3축 / OBB 3축 / 외적 9축 검사를 선택적으로 수행
	const FVector T = AABB.GetCenter() - Center;
	const FVector AABBExtents = AABB.GetExtent();

	if (bTestAABBAxes && !TestAABBAxes(T, AABBExtents, Extents, Axes)) { return false; }
	if (bTestOBBAxes && !TestOBBAxes(T, AABBExtents, Extents, Axes)) { return false; }
	if (bTestCrossAxes && !TestCrossAxes(T, AABBExtents, Extents, Axes)) { return false; }
	return true;
}

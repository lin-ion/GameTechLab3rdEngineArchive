#pragma once

#include "Editor/Selection/PickingTypes.h"
#include "Profiling/PlatformTime.h"

struct FPickingPerfBucket
{
	uint64 TotalPickCount = 0;
	double LastPickTimeMs = 0.0;
	double TotalPickTimeMs = 0.0;

	double GetAverageMs() const
	{
		return TotalPickCount > 0 ? (TotalPickTimeMs / static_cast<double>(TotalPickCount)) : 0.0;
	}
};

// FPickingPlatformTime은 FPlatformTime으로 통일 — 하위 호환 typedef
typedef FPlatformTime FPickingPlatformTime;

class FPickingPerf
{
public:
	static void Reset()
	{
		Ray = {};
		IdBuffer = {};
		RayBroadPhase = {};
		RayNarrowPhase = {};
	}

	static void Record(EPickingMode Mode, uint64 CycleDiff)
	{
		double Ms = FPlatformTime::ToMilliseconds(CycleDiff);
		FPickingPerfBucket& Bucket = (Mode == EPickingMode::IDBuffer) ? IdBuffer : Ray;
		Bucket.LastPickTimeMs = Ms;
		Bucket.TotalPickTimeMs += Ms;
		++Bucket.TotalPickCount;
	}

	static const FPickingPerfBucket& GetRay() { return Ray; }
	static const FPickingPerfBucket& GetIdBuffer() { return IdBuffer; }
	static const FPickingPerfBucket& GetRayBroadPhase() { return RayBroadPhase; }
	static const FPickingPerfBucket& GetRayNarrowPhase() { return RayNarrowPhase; }

	static void RecordRayBroadPhase(uint64 CycleDiff)
	{
		double Ms = FPlatformTime::ToMilliseconds(CycleDiff);
		RayBroadPhase.LastPickTimeMs = Ms;
		RayBroadPhase.TotalPickTimeMs += Ms;
		++RayBroadPhase.TotalPickCount;
	}

	static void RecordRayNarrowPhase(uint64 CycleDiff)
	{
		double Ms = FPlatformTime::ToMilliseconds(CycleDiff);
		RayNarrowPhase.LastPickTimeMs = Ms;
		RayNarrowPhase.TotalPickTimeMs += Ms;
		++RayNarrowPhase.TotalPickCount;
	}

private:
	inline static FPickingPerfBucket Ray;
	inline static FPickingPerfBucket IdBuffer;
	inline static FPickingPerfBucket RayBroadPhase;
	inline static FPickingPerfBucket RayNarrowPhase;
};

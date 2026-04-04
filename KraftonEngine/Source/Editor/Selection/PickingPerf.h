#pragma once

#include "Editor/Selection/PickingTypes.h"

#define NOMINMAX
#include <Windows.h>

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

class FPickingPlatformTime
{
public:
	static uint64 GetFrequency()
	{
		LARGE_INTEGER Frequency;
		QueryPerformanceFrequency(&Frequency);
		return static_cast<uint64>(Frequency.QuadPart);
	}

	static uint64 Cycles64()
	{
		LARGE_INTEGER Cycles;
		QueryPerformanceCounter(&Cycles);
		return static_cast<uint64>(Cycles.QuadPart);
	}

	static double ToMilliseconds(uint64 CycleDiff)
	{
		const double Frequency = static_cast<double>(GetFrequency());
		if (Frequency <= 0.0)
		{
			return 0.0;
		}
		return (static_cast<double>(CycleDiff) / Frequency) * 1000.0;
	}
};

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
		double Ms = FPickingPlatformTime::ToMilliseconds(CycleDiff);
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
		double Ms = FPickingPlatformTime::ToMilliseconds(CycleDiff);
		RayBroadPhase.LastPickTimeMs = Ms;
		RayBroadPhase.TotalPickTimeMs += Ms;
		++RayBroadPhase.TotalPickCount;
	}

	static void RecordRayNarrowPhase(uint64 CycleDiff)
	{
		double Ms = FPickingPlatformTime::ToMilliseconds(CycleDiff);
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

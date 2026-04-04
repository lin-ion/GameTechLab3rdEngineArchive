#include "Profiling/Stats.h"

#include <algorithm>

FStatManager::FStatManager()
{
	FPlatformTime::InitTiming();
	QueryPerformanceFrequency(&Frequency);
}

void FStatManager::RecordTime(const char* Name, double ElapsedSeconds)
{
	auto it = Stats.find(Name);
	if (it == Stats.end())
	{
		FStatAccumulator Acc;
		Acc.Name = Name;
		Acc.MaxTime = ElapsedSeconds;
		Acc.MinTime = ElapsedSeconds;
		Acc.LastTime = ElapsedSeconds;
		Stats[Name] = std::move(Acc);
		return;
	}

	FStatAccumulator& Acc = it->second;
	Acc.LastTime = ElapsedSeconds;
	Acc.MaxTime = (std::max)(Acc.MaxTime, ElapsedSeconds);
	Acc.MinTime = (std::min)(Acc.MinTime, ElapsedSeconds);
}

void FStatManager::TakeSnapshot()
{
	Snapshot.clear();
	Snapshot.reserve(Stats.size());

	for (const auto& [Key, Acc] : Stats)
	{
		FStatEntry Entry;
		Entry.Name    = Acc.Name;
		Entry.MaxTime = Acc.MaxTime;
		Entry.MinTime = Acc.MinTime;
		Entry.LastTime = Acc.LastTime;
		Snapshot.push_back(Entry);
	}
}

void FStatManager::ResetStats()
{
	Stats.clear();
}

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
		Acc.TotalTime = ElapsedSeconds;
		Acc.Count = 1;
		Stats[Name] = std::move(Acc);
		return;
	}

	FStatAccumulator& Acc = it->second;
	Acc.LastTime = ElapsedSeconds;
	Acc.MaxTime = (std::max)(Acc.MaxTime, ElapsedSeconds);
	Acc.MinTime = (std::min)(Acc.MinTime, ElapsedSeconds);
	Acc.TotalTime += ElapsedSeconds;
	++Acc.Count;
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
		Entry.TotalTime = Acc.TotalTime;
		Entry.Count = Acc.Count;
		Snapshot.push_back(Entry);
	}
}

bool FStatManager::GetStat(const char* Name, FStatEntry& OutEntry) const
{
	auto It = Stats.find(Name);
	if (It == Stats.end())
	{
		return false;
	}

	const FStatAccumulator& Acc = It->second;
	OutEntry = {};
	OutEntry.Name = Acc.Name;
	OutEntry.MaxTime = Acc.MaxTime;
	OutEntry.MinTime = Acc.MinTime;
	OutEntry.LastTime = Acc.LastTime;
	OutEntry.TotalTime = Acc.TotalTime;
	OutEntry.Count = Acc.Count;
	return true;
}

void FStatManager::ResetStats()
{
	Stats.clear();
}

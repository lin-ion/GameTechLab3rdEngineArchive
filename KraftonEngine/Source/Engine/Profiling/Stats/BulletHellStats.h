#pragma once

#include "Core/Types/CoreTypes.h"
#include "Profiling/Stats/Stats.h"

struct FBulletHellStatsSnapshot
{
	uint32 ActiveBulletCount = 0;
	uint32 TotalSpawned = 0;
	uint32 TotalKilled = 0;
	uint32 TotalExpired = 0;
	uint32 CollisionQueryCount = 0;
	uint32 CollisionHitCount = 0;
	uint32 CollisionKilledCount = 0;
	uint32 EraseKilledCount = 0;
	uint32 BehaviorTransitionCount = 0;
	uint32 ActiveLinearCount = 0;
	uint32 ActiveHomingCount = 0;
	uint32 ActiveColdLaunchCount = 0;
	uint32 ActiveTimedVelocityChangeCount = 0;
	uint32 ActivePrimaryArchetypeCount = 0;
	uint32 ActiveSecondaryArchetypeCount = 0;
	uint32 DebugDrawSelectedCount = 0;
	uint32 DebugDrawTruncatedCount = 0;
	uint32 RenderInstanceCount = 0;
	uint32 RendererSlotCount = 0;
	uint32 RendererSlot0InstanceCount = 0;
	uint32 RendererSlot1InstanceCount = 0;
	uint32 RenderMismatchCount = 0;
};

#if STATS
struct FBulletHellStats
{
	static uint32 ComponentCount;
	static uint32 ActiveBulletCount;
	static uint32 TotalSpawned;
	static uint32 TotalKilled;
	static uint32 TotalExpired;
	static uint32 CollisionQueryCount;
	static uint32 CollisionHitCount;
	static uint32 CollisionKilledCount;
	static uint32 EraseKilledCount;
	static uint32 BehaviorTransitionCount;
	static uint32 ActiveLinearCount;
	static uint32 ActiveHomingCount;
	static uint32 ActiveColdLaunchCount;
	static uint32 ActiveTimedVelocityChangeCount;
	static uint32 ActivePrimaryArchetypeCount;
	static uint32 ActiveSecondaryArchetypeCount;
	static uint32 DebugDrawSelectedCount;
	static uint32 DebugDrawTruncatedCount;
	static uint32 RenderInstanceCount;
	static uint32 RendererSlotCount;
	static uint32 RendererSlot0InstanceCount;
	static uint32 RendererSlot1InstanceCount;
	static uint32 RenderMismatchCount;

	static void Reset();
	static void AddComponent(const FBulletHellStatsSnapshot& Snapshot);
};

#define BULLETHELL_STATS_RESET() FBulletHellStats::Reset()
#define BULLETHELL_STATS_ADD_COMPONENT(Snapshot) FBulletHellStats::AddComponent((Snapshot))
#else
#define BULLETHELL_STATS_RESET() ((void)0)
#define BULLETHELL_STATS_ADD_COMPONENT(Snapshot) ((void)0)
#endif

#include "BulletHellStats.h"

#if STATS
uint32 FBulletHellStats::ComponentCount = 0;
uint32 FBulletHellStats::ActiveBulletCount = 0;
uint32 FBulletHellStats::TotalSpawned = 0;
uint32 FBulletHellStats::TotalKilled = 0;
uint32 FBulletHellStats::TotalExpired = 0;
uint32 FBulletHellStats::CollisionQueryCount = 0;
uint32 FBulletHellStats::CollisionHitCount = 0;
uint32 FBulletHellStats::CollisionKilledCount = 0;
uint32 FBulletHellStats::EraseKilledCount = 0;
uint32 FBulletHellStats::BehaviorTransitionCount = 0;
uint32 FBulletHellStats::ActiveLinearCount = 0;
uint32 FBulletHellStats::ActiveHomingCount = 0;
uint32 FBulletHellStats::ActiveColdLaunchCount = 0;
uint32 FBulletHellStats::ActiveTimedVelocityChangeCount = 0;
uint32 FBulletHellStats::ActivePrimaryArchetypeCount = 0;
uint32 FBulletHellStats::ActiveSecondaryArchetypeCount = 0;
uint32 FBulletHellStats::DebugDrawSelectedCount = 0;
uint32 FBulletHellStats::DebugDrawTruncatedCount = 0;
uint32 FBulletHellStats::RenderInstanceCount = 0;
uint32 FBulletHellStats::RendererSlotCount = 0;
uint32 FBulletHellStats::RendererSlot0InstanceCount = 0;
uint32 FBulletHellStats::RendererSlot1InstanceCount = 0;
uint32 FBulletHellStats::RenderMismatchCount = 0;

void FBulletHellStats::Reset()
{
	ComponentCount = 0;
	ActiveBulletCount = 0;
	TotalSpawned = 0;
	TotalKilled = 0;
	TotalExpired = 0;
	CollisionQueryCount = 0;
	CollisionHitCount = 0;
	CollisionKilledCount = 0;
	EraseKilledCount = 0;
	BehaviorTransitionCount = 0;
	ActiveLinearCount = 0;
	ActiveHomingCount = 0;
	ActiveColdLaunchCount = 0;
	ActiveTimedVelocityChangeCount = 0;
	ActivePrimaryArchetypeCount = 0;
	ActiveSecondaryArchetypeCount = 0;
	DebugDrawSelectedCount = 0;
	DebugDrawTruncatedCount = 0;
	RenderInstanceCount = 0;
	RendererSlotCount = 0;
	RendererSlot0InstanceCount = 0;
	RendererSlot1InstanceCount = 0;
	RenderMismatchCount = 0;
}

void FBulletHellStats::AddComponent(const FBulletHellStatsSnapshot& Snapshot)
{
	++ComponentCount;
	ActiveBulletCount += Snapshot.ActiveBulletCount;
	TotalSpawned += Snapshot.TotalSpawned;
	TotalKilled += Snapshot.TotalKilled;
	TotalExpired += Snapshot.TotalExpired;
	CollisionQueryCount += Snapshot.CollisionQueryCount;
	CollisionHitCount += Snapshot.CollisionHitCount;
	CollisionKilledCount += Snapshot.CollisionKilledCount;
	EraseKilledCount += Snapshot.EraseKilledCount;
	BehaviorTransitionCount += Snapshot.BehaviorTransitionCount;
	ActiveLinearCount += Snapshot.ActiveLinearCount;
	ActiveHomingCount += Snapshot.ActiveHomingCount;
	ActiveColdLaunchCount += Snapshot.ActiveColdLaunchCount;
	ActiveTimedVelocityChangeCount += Snapshot.ActiveTimedVelocityChangeCount;
	ActivePrimaryArchetypeCount += Snapshot.ActivePrimaryArchetypeCount;
	ActiveSecondaryArchetypeCount += Snapshot.ActiveSecondaryArchetypeCount;
	DebugDrawSelectedCount += Snapshot.DebugDrawSelectedCount;
	DebugDrawTruncatedCount += Snapshot.DebugDrawTruncatedCount;
	RenderInstanceCount += Snapshot.RenderInstanceCount;
	RendererSlotCount += Snapshot.RendererSlotCount;
	RendererSlot0InstanceCount += Snapshot.RendererSlot0InstanceCount;
	RendererSlot1InstanceCount += Snapshot.RendererSlot1InstanceCount;
	RenderMismatchCount += Snapshot.RenderMismatchCount;
}
#endif

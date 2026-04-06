#include "Editor/Subsystem/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Profiling/Stats.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Profiling/MemoryStats.h"
#include "GameFramework/World.h"
#include "GameFramework/Scene.h"
#include "Render/Pipeline/WorldRenderProxy.h"

TArray<FOverlayStatGroup> FOverlayStatSystem::BuildGroups(const UEditorEngine& Editor) const
{
	TArray<FOverlayStatGroup> Groups;

	if (bShowFPS)
	{
		FOverlayStatGroup Group;

		const FTimer* Timer = Editor.GetTimer();
		const float FPS = Timer ? Timer->GetDisplayFPS() : 0.0f;
		const float MS = Timer ? Timer->GetFrameTimeMs() : 0.0f;

		// 발제 필수 HUD 형식: FPS : <N> (<ms> ms)
		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "FPS : %.1f (%.2f ms)", FPS, MS);
			Group.Lines.push_back(FString(Buffer));
		}

		// 발제 필수 HUD 형식: Picking Time <ms> ms : Num Attempts <N> : Accumulated Time <ms> ms
		// UX(E2E) 기준: 클릭 시점 -> 최종 선택 반영
		{
			FStatEntry ClickE2E = {};
			FStatManager::Get().GetStat("Picking.Click.E2E", ClickE2E);
			const double LastMs = ClickE2E.LastTime * 1000.0;
			const uint64 Count = ClickE2E.Count;
			const double TotalMs = ClickE2E.TotalTime * 1000.0;

			char Buffer[256] = {};
			snprintf(Buffer, sizeof(Buffer),
				"Picking Time %.8f ms : Num Attempts %llu : Accumulated Time %.8f ms",
				LastMs, static_cast<unsigned long long>(Count), TotalMs);
			Group.Lines.push_back(FString(Buffer));
		}

		// Algorithm(Core) 기준: Ray(Broad+Narrow) vs ID(Fetch)
		{
			FStatEntry RayBroad = {};
			FStatEntry RayNarrow = {};
			FStatEntry RayBroadRebuild = {};
			FStatEntry RayBroadTraversal = {};
			FStatEntry RayBroadTraversalDefer = {};
			FStatEntry RayBroadTraversalLinear = {};
			FStatEntry RayBroadTraversalBVH = {};
			FStatEntry RayBroadFilter = {};
			FStatEntry IdAlgorithm = {};
			FStatEntry IdFetchClick = {};
			FStatEntry IdWait = {};
			FStatManager::Get().GetStat("Picking.Ray.Broad", RayBroad);
			FStatManager::Get().GetStat("Picking.Ray.Narrow", RayNarrow);
			FStatManager::Get().GetStat("Picking.Ray.Broad.Rebuild", RayBroadRebuild);
			FStatManager::Get().GetStat("Picking.Ray.Broad.Traversal", RayBroadTraversal);
			FStatManager::Get().GetStat("Picking.Ray.Broad.Traversal.DeferBypass", RayBroadTraversalDefer);
			FStatManager::Get().GetStat("Picking.Ray.Broad.Traversal.VisibleLinear", RayBroadTraversalLinear);
			FStatManager::Get().GetStat("Picking.Ray.Broad.Traversal.BVH", RayBroadTraversalBVH);
			FStatManager::Get().GetStat("Picking.Ray.Broad.Filter", RayBroadFilter);
			FStatManager::Get().GetStat("Picking.ID.Algorithm", IdAlgorithm);
			FStatManager::Get().GetStat("Picking.ID.Fetch.Click", IdFetchClick);
			FStatManager::Get().GetStat("Picking.ID.Wait.Click", IdWait);

			const double RayCoreLastMs = (RayBroad.LastTime + RayNarrow.LastTime) * 1000.0;
			const double IdFetchLastMs = IdFetchClick.LastTime * 1000.0;

			char Buffer[256] = {};
			snprintf(Buffer, sizeof(Buffer),
				"Picking Core Last(ms) : Ray %.8f / ID Algo %.8f / ID Stall %.8f",
				RayCoreLastMs, IdAlgorithm.LastTime * 1000.0, IdWait.LastTime * 1000.0);
			Group.Lines.push_back(FString(Buffer));

			snprintf(Buffer, sizeof(Buffer),
				"RayBroad Split Last(ms) : Rebuild %.8f / Traversal %.8f / Filter %.8f",
				RayBroadRebuild.LastTime * 1000.0,
				RayBroadTraversal.LastTime * 1000.0,
				RayBroadFilter.LastTime * 1000.0);
			Group.Lines.push_back(FString(Buffer));

			snprintf(Buffer, sizeof(Buffer),
				"RayBroad Traversal Last(ms) : Defer %.8f / Linear %.8f / BVH %.8f",
				RayBroadTraversalDefer.LastTime * 1000.0,
				RayBroadTraversalLinear.LastTime * 1000.0,
				RayBroadTraversalBVH.LastTime * 1000.0);
			Group.Lines.push_back(FString(Buffer));

			if (UWorld* World = Editor.GetWorld())
			{
				FRayBroadDebugCounters TotalCounters = {};
				auto AccumulateRayBroadCounters = [&TotalCounters](UScene* Scene)
				{
					if (!Scene) return;
					const FRayBroadDebugCounters& C = Scene->GetRenderProxy().GetLastRayBroadDebugCounters();
					TotalCounters.AABBTests += C.AABBTests;
					TotalCounters.AABBHits += C.AABBHits;
					TotalCounters.CandidatesEmitted += C.CandidatesEmitted;
					TotalCounters.CandidatesAfterFilter += C.CandidatesAfterFilter;
				};
				AccumulateRayBroadCounters(World->GetPersistentScene());
				AccumulateRayBroadCounters(World->GetActiveScene());

				snprintf(Buffer, sizeof(Buffer),
					"RayBroad Count : Test %llu / Hit %llu / Emit %llu / PostFilter %llu",
					static_cast<unsigned long long>(TotalCounters.AABBTests),
					static_cast<unsigned long long>(TotalCounters.AABBHits),
					static_cast<unsigned long long>(TotalCounters.CandidatesEmitted),
					static_cast<unsigned long long>(TotalCounters.CandidatesAfterFilter));
				Group.Lines.push_back(FString(Buffer));
			}
		}

		Groups.push_back(std::move(Group));
	}

	if (bShowMemory)
	{
		FOverlayStatGroup Group;

		/*{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Memory Allocated : %u", MemoryStats::GetTotalAllocationBytes());
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Times Allocated : %u", MemoryStats::GetTotalAllocationCount());
			Group.Lines.push_back(FString(Buffer));
		}*/

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "PixelShader Memory : %.2f KB", static_cast<double>(MemoryStats::GetPixelShaderMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "VertexShader Memory : %.2f KB", static_cast<double>(MemoryStats::GetVertexShaderMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "VertexBuffer Memory : %.2f KB", static_cast<double>(MemoryStats::GetVertexBufferMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "IndexBuffer Memory : %.2f KB", static_cast<double>(MemoryStats::GetIndexBufferMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "StaticMesh CPU Memory : %.2f KB", static_cast<double>(MemoryStats::GetStaticMeshCPUMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Texture Memory : %.2f KB", static_cast<double>(MemoryStats::GetTextureMemory() / 1024.0f));
			Group.Lines.push_back(FString(Buffer));
		}

		Groups.push_back(std::move(Group));
	}

	return Groups;
}

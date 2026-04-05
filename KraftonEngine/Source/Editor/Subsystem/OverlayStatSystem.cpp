#include "Editor/Subsystem/OverlayStatSystem.h"

#include "Editor/Subsystem/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
#include "Profiling/Stats.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Profiling/MemoryStats.h"

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
		// UX(E2E) 기준 Ray + ID 합산
		{
			FStatEntry RayPick = {};
			FStatEntry IdPick = {};
			FStatManager::Get().GetStat("Picking.Ray.E2E", RayPick);
			FStatManager::Get().GetStat("Picking.ID.Total", IdPick);

			const double LastMs  = (RayPick.LastTime + IdPick.LastTime) * 1000.0;
			const uint64 Count   = RayPick.Count + IdPick.Count;
			const double TotalMs = (RayPick.TotalTime + IdPick.TotalTime) * 1000.0;

			char Buffer[256] = {};
			snprintf(Buffer, sizeof(Buffer),
				"Picking Time %.5f ms : Num Attempts %llu : Accumulated Time %.5f ms",
				LastMs, static_cast<unsigned long long>(Count), TotalMs);
			Group.Lines.push_back(FString(Buffer));
		}

		// Algorithm(Core) 기준: Ray(Broad+Narrow) vs ID(Fetch)
		{
			FStatEntry RayBroad = {};
			FStatEntry RayNarrow = {};
			FStatEntry IdFetch = {};
			FStatEntry IdWait = {};
			FStatManager::Get().GetStat("Picking.Ray.Broad", RayBroad);
			FStatManager::Get().GetStat("Picking.Ray.Narrow", RayNarrow);
			FStatManager::Get().GetStat("Picking.ID.Fetch", IdFetch);
			FStatManager::Get().GetStat("Picking.ID.Wait", IdWait);

			const double RayCoreLastMs = (RayBroad.LastTime + RayNarrow.LastTime) * 1000.0;
			const double IdFetchLastMs = IdFetch.LastTime * 1000.0;

			char Buffer[256] = {};
			snprintf(Buffer, sizeof(Buffer),
				"Picking Core Last(ms) : Ray %.5f / ID Fetch %.5f / ID Wait %.5f",
				RayCoreLastMs, IdFetchLastMs, IdWait.LastTime * 1000.0);
			Group.Lines.push_back(FString(Buffer));
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

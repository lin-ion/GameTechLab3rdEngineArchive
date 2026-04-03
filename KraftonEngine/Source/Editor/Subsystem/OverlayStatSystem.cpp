#include "Editor/Subsystem/OverlayStatSystem.h"

#include "Editor/EditorEngine.h"
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
		const float MS = FPS > 0.0f ? 1000.0f / FPS : 0.0f;
		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "FPS : %.1f", FPS);
			Group.Lines.push_back(FString(Buffer));
		}
		{
			char Buffer[128] = {};
			snprintf(Buffer, sizeof(Buffer), "Frame Time : %.2f ms", MS);
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

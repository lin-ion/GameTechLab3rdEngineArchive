#include "Editor/UI/EditorStatWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/PickingPerf.h"
#include "Profiling/Stats.h"
#include "Render/Pipeline/RenderStats.h"
#include "Render/Pipeline/WorldRenderProxy.h"
#include "GameFramework/World.h"
#include "GameFramework/Scene.h"
#include "ImGui/imgui.h"

#include <algorithm>
#include <sstream>

// ────────────────────────────────────────────────────────────
// 메인 Render
// ────────────────────────────────────────────────────────────
void FEditorStatWidget::Render(float DeltaTime)
{
#if STATS
	(void)DeltaTime;

	ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(700.0f, 600.0f), ImGuiCond_Once);
	ImGui::Begin("Stat Profiler");

	// Pause / Resume 버튼
	if (bPaused)
	{
		if (ImGui::Button("Resume"))
		{
			bPaused = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Copy"))
		{
			std::ostringstream oss;
			auto FormatTable = [&](const char* Title, const TArray<FStatEntry>& Entries)
			{
				oss << "=== " << Title << " ===\n";
				oss << "Name\tMax(ms)\tMin(ms)\tLast(ms)\n";
				for (const FStatEntry& E : Entries)
				{
					double MinVal = E.MinTime == DBL_MAX ? 0.0 : E.MinTime;
					oss << E.Name << "\t"
						<< E.MaxTime * 1000.0 << "\t"
						<< MinVal * 1000.0 << "\t"
						<< E.LastTime * 1000.0 << "\n";
				}
				oss << "\n";
			};
			FormatTable("CPU Stats", FrozenCPUEntries);
			ImGui::SetClipboardText(oss.str().c_str());
		}
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.0f, 1.0f), "PAUSED");
	}
	else
	{
		if (ImGui::Button("Pause"))
		{
			FrozenCPUEntries = FStatManager::Get().GetSnapshot();
			bPaused = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Stats"))
		{
			FStatManager::Get().ResetStats();
		}
	}

	ImGui::Separator();

	// ── Render API Stats ──
	RenderRenderStats();

	// ── Culling Stats ──
	RenderCullingStats();

	// ── Picking Detail ──
	RenderPickingDetail();

	// ── Raw CPU Stats ──
	const TArray<FStatEntry>& CPUSource = bPaused ? FrozenCPUEntries : FStatManager::Get().GetSnapshot();
	if (ImGui::CollapsingHeader("CPU Stats"))
	{
		RenderStatTable("CPUStatTable", CPUSource);
	}

	ImGui::End();
#endif
}

// ────────────────────────────────────────────────────────────
// Render API Stats: DrawCall / CB / State 카운터
// ────────────────────────────────────────────────────────────
void FEditorStatWidget::RenderRenderStats()
{
	if (!ImGui::CollapsingHeader("Render Stats"))
		return;

	const FRenderStats& S = GRenderStatsSnapshot;

	ImGui::Text("Draw Calls: %u (Indexed: %u, Vertex: %u)",
		S.DrawCalls, S.DrawIndexedCalls, S.DrawVertexCalls);
	ImGui::Text("Triangles: %u", S.TrianglesRendered);

	float MBUploaded = static_cast<float>(S.CBBytesUploaded) / (1024.0f * 1024.0f);
	ImGui::Text("CB Updates: %u (%.2f MB uploaded)", S.CBMapCount, MBUploaded);

	ImGui::Text("VB/IB Binds: %u (%u redundant)", S.MeshBinds, S.RedundantMeshBinds);
	ImGui::Text("Shader Binds: %u (%u redundant)", S.ShaderBinds, S.RedundantShaderBinds);
	ImGui::Text("SRV Changes: %u (%u redundant)", S.SRVChanges, S.RedundantSRVChanges);
}

// ────────────────────────────────────────────────────────────
// Culling Stats: Proxy/Octree 통계 + Culling 효율
// ────────────────────────────────────────────────────────────
void FEditorStatWidget::RenderCullingStats()
{
	if (!ImGui::CollapsingHeader("Culling Stats"))
		return;

	if (!EditorEngine) return;

	UWorld* World = EditorEngine->GetWorld();
	if (!World) return;

	FWorldProxyCullingStats Total = {};
	auto AccumulateScene = [&Total](UScene* Scene)
	{
		if (!Scene) return;
		const FWorldProxyCullingStats& S = Scene->GetRenderProxy().GetLastCullingStats();
		Total.RegisteredProxyCount          += S.RegisteredProxyCount;
		Total.InsertedProxyCount            += S.InsertedProxyCount;
		Total.CandidateProxyCount           += S.CandidateProxyCount;
		Total.RenderedProxyCount            += S.RenderedProxyCount;
		Total.OctreeTotalNodes              += S.OctreeTotalNodes;
		Total.OctreeTotalItems              += S.OctreeTotalItems;
		Total.OctreeOutsideItems            += S.OctreeOutsideItems;
		Total.OctreeFrustumIntersectedNodes += S.OctreeFrustumIntersectedNodes;
		Total.OctreeFrustumCandidateItems   += S.OctreeFrustumCandidateItems;
	};
	AccumulateScene(World->GetPersistentScene());
	AccumulateScene(World->GetActiveScene());

	int32 Culled = Total.RegisteredProxyCount - Total.RenderedProxyCount;
	float Efficiency = Total.RegisteredProxyCount > 0
		? static_cast<float>(Culled) / static_cast<float>(Total.RegisteredProxyCount) : 0.0f;

	ImGui::Text("Total: %d  Rendered: %d  Culled: %d",
		Total.RegisteredProxyCount, Total.RenderedProxyCount, Culled);

	// char EffBuf[64];
	// snprintf(EffBuf, sizeof(EffBuf), "%.1f%%", Efficiency * 100.0f);
	// ImGui::ProgressBar(Efficiency, ImVec2(-1, 0), EffBuf);

	ImGui::Text("Octree: %d nodes, %d intersected",
		Total.OctreeTotalNodes, Total.OctreeFrustumIntersectedNodes);
	ImGui::Text("Octree Items: %d total, %d outside, %d in frustum",
		Total.OctreeTotalItems, Total.OctreeOutsideItems, Total.OctreeFrustumCandidateItems);
}

// ────────────────────────────────────────────────────────────
// Picking Detail: Ray / Broad / Narrow / ID 각각
// ────────────────────────────────────────────────────────────
void FEditorStatWidget::RenderPickingDetail()
{
	if (!ImGui::CollapsingHeader("Picking Detail"))
		return;

	auto ShowBucket = [](const char* Label, const FPickingPerfBucket& B)
	{
		ImGui::Text("[%s]  Last: %.3f ms  Avg: %.3f ms  Total: %.3f ms  Count: %llu",
			Label, B.LastPickTimeMs, B.GetAverageMs(), B.TotalPickTimeMs,
			static_cast<unsigned long long>(B.TotalPickCount));
	};

	ShowBucket("Ray",          FPickingPerf::GetRay());
	ShowBucket("Ray Broad",    FPickingPerf::GetRayBroadPhase());
	ShowBucket("Ray Narrow",   FPickingPerf::GetRayNarrowPhase());
	ShowBucket("ID Buffer",    FPickingPerf::GetIdBuffer());
}

// ────────────────────────────────────────────────────────────
// Raw Stat Table (기존 구현 유지)
// ────────────────────────────────────────────────────────────
void FEditorStatWidget::RenderStatTable(const char* TableID, const TArray<FStatEntry>& Source)
{
#if STATS
	if (Source.empty())
	{
		ImGui::Text("No stats recorded.");
		return;
	}

	const char* Headers[] = { "Name", "Max(ms)", "Min(ms)", "Last(ms)" };
	constexpr int NumColumns = 4;

	if (ImGui::BeginTable(TableID, NumColumns,
		ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable,
		ImVec2(0.0f, 400.0f)))
	{
		for (int i = 0; i < NumColumns; i++)
		{
			ImGui::TableSetupColumn(Headers[i], ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthStretch);
		}
		ImGui::TableHeadersRow();

		TArray<FStatEntry> SortedSource = Source;
		if (ImGuiTableSortSpecs* sorts_specs = ImGui::TableGetSortSpecs())
		{
			if (sorts_specs->SpecsCount > 0)
			{
				const ImGuiTableColumnSortSpecs* sort_spec = &sorts_specs->Specs[0];
				std::sort(SortedSource.begin(), SortedSource.end(), [sort_spec](const FStatEntry& A, const FStatEntry& B)
				{
					int delta = 0;
					if (sort_spec->ColumnIndex == 0)
					{
						delta = strcmp(A.Name, B.Name);
					}
					else if (sort_spec->ColumnIndex == 1)
					{
						delta = (A.MaxTime > B.MaxTime) ? 1 : ((A.MaxTime < B.MaxTime) ? -1 : 0);
					}
					else if (sort_spec->ColumnIndex == 2)
					{
						double MinA = A.MinTime == DBL_MAX ? 0.0 : A.MinTime;
						double MinB = B.MinTime == DBL_MAX ? 0.0 : B.MinTime;
						delta = (MinA > MinB) ? 1 : ((MinA < MinB) ? -1 : 0);
					}
					else if (sort_spec->ColumnIndex == 3)
					{
						delta = (A.LastTime > B.LastTime) ? 1 : ((A.LastTime < B.LastTime) ? -1 : 0);
					}

					if (delta > 0)
						return sort_spec->SortDirection == ImGuiSortDirection_Descending;
					if (delta < 0)
						return sort_spec->SortDirection == ImGuiSortDirection_Ascending;
					return strcmp(A.Name, B.Name) < 0;
				});
			}
		}

		for (const FStatEntry& E : SortedSource)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::Text("%s", E.Name);
			ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", E.MaxTime * 1000.0);
			ImGui::TableSetColumnIndex(2); ImGui::Text("%.3f", E.MinTime == DBL_MAX ? 0.0 : E.MinTime * 1000.0);
			ImGui::TableSetColumnIndex(3); ImGui::Text("%.3f", E.LastTime * 1000.0);
		}

		ImGui::EndTable();
	}
#endif
}

#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Profiling/Stats.h"

class FEditorStatWidget : public FEditorWidget
{
public:
	void Render(float DeltaTime) override;

private:
	// 기존 Raw 테이블
	void RenderStatTable(const char* TableID, const TArray<FStatEntry>& Source, int& OutSortColumn, bool& OutSortDescending);

	// 통합 UI 섹션들
	void RenderRenderStats();
	void RenderCullingStats();
	void RenderPickingDetail();

	int CPUSortColumn = 1;
	bool bCPUSortDescending = true;
	bool bPaused = false;
	TArray<FStatEntry> FrozenCPUEntries;

};

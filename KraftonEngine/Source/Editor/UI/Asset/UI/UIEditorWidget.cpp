#include "UIEditorWidget.h"

#include "Object/Object.h"
#include "Object/GarbageCollection.h"
#include "Serialization/SceneSaveManager.h"
#include "UI/UIAsset.h"
#include "UI/Canvas/UICanvas.h"
#include "UI/Canvas/UICanvasActor.h"
#include "UI/Canvas/UICanvasManager.h"

#include <imgui.h>

bool FUIEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UUIAsset>();
}

void FUIEditorWidget::Open(UObject* Object)
{
	if (!CanEdit(Object))
	{
		return;
	}

	DestroyLiveTree();   // 단일 인스턴스 재사용 — 다른 에셋으로 재오픈 시 이전 트리 정리.
	EditedObject = Object;
	bOpen        = true;
	ClearDirty();
	BuildLiveTree(static_cast<UUIAsset*>(Object));
}

void FUIEditorWidget::Close()
{
	DestroyLiveTree();
	FAssetEditorWidget::Close();
}

void FUIEditorWidget::AddReferencedObjects(FReferenceCollector& Collector)
{
	FAssetEditorWidget::AddReferencedObjects(Collector);   // EditedObject(UUIAsset) keepalive
	Collector.AddReferencedObject(OwnerActor);             // 소유자 액터 → OwnedComponents(캔버스 트리) keepalive
}

void FUIEditorWidget::BuildLiveTree(UUIAsset* Asset)
{
	if (!Asset)
	{
		return;
	}

	// JSON 블롭 → 라이브 트리. 소유자는 월드 없는 에디터 전용 액터.
	OwnerActor = UObjectManager::Get().CreateObject<AUICanvasActor>();
	USceneComponent* Root = FSceneSaveManager::DeserializeUITree(Asset->GetCanvasData(), OwnerActor);
	Canvas = Cast<UUICanvas>(Root);
	if (Canvas && OwnerActor)
	{
		OwnerActor->SetRootComponent(Canvas);   // AUICanvasActor 계약(RootComponent=Canvas) 유지.
	}
}

void FUIEditorWidget::DestroyLiveTree()
{
	Canvas = nullptr;
	if (OwnerActor)
	{
		UObjectManager::Get().DestroyObject(OwnerActor);   // 컴포넌트 트리까지 정리(2-phase GC).
		OwnerActor = nullptr;
	}
}

void FUIEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	UUIAsset* UIAsset = static_cast<UUIAsset*>(EditedObject);

	bool    bWindowOpen  = true;
	FString VisibleTitle = "UI Editor";
	if (!UIAsset->GetSourcePath().empty())
	{
		VisibleTitle += " - ";
		VisibleTitle += UIAsset->GetSourcePath();
	}

	ImGui::SetNextWindowSize(ImVec2(960.0f, 600.0f), ImGuiCond_Once);

	// ### 뒤 고정 ID 로 제목이 바뀌어도 같은 창을 재사용(단일 인스턴스).
	FString WindowTitle = VisibleTitle + "###UIEditor";
	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	// 4분할: [좌상 팔레트 / 좌하 계층트리] | [중앙 캔버스 뷰포트] | [우 디테일]. child 분할(진단 B).
	const float  LeftWidth  = 200.0f;
	const float  RightWidth = 260.0f;
	const float  Spacing    = ImGui::GetStyle().ItemSpacing.x;
	const ImVec2 Avail      = ImGui::GetContentRegionAvail();
	float        CenterWidth = Avail.x - LeftWidth - RightWidth - Spacing * 2.0f;
	if (CenterWidth < 80.0f) CenterWidth = 80.0f;

	// 좌측 컬럼 — 팔레트(상) + 계층트리(하) 상하 분할.
	ImGui::BeginChild("##UILeftColumn", ImVec2(LeftWidth, 0.0f), false);
	{
		const float PaletteHeight = ImGui::GetContentRegionAvail().y * 0.4f;
		ImGui::BeginChild("##UIPalette", ImVec2(0.0f, PaletteHeight), true);
		RenderPalettePanel();
		ImGui::EndChild();

		ImGui::BeginChild("##UIHierarchy", ImVec2(0.0f, 0.0f), true);
		RenderHierarchyPanel();
		ImGui::EndChild();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// 중앙 — 캔버스 뷰포트.
	ImGui::BeginChild("##UIViewport", ImVec2(CenterWidth, 0.0f), true);
	RenderViewportPanel();
	ImGui::EndChild();

	ImGui::SameLine();

	// 우측 — 디테일(나머지 폭).
	ImGui::BeginChild("##UIDetails", ImVec2(0.0f, 0.0f), true);
	RenderDetailsPanel();
	ImGui::EndChild();

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

void FUIEditorWidget::RenderPalettePanel()
{
	// 사이클 ③에서 Canvas/Button/Image 팔레트 버튼을 채운다.
	ImGui::TextUnformatted("Palette");
	ImGui::Separator();
}

void FUIEditorWidget::RenderHierarchyPanel()
{
	// 사이클 ②에서 캔버스 트리(GetChildren 재귀)를 TreeNode 로 채운다.
	ImGui::TextUnformatted("Hierarchy");
	ImGui::Separator();
}

void FUIEditorWidget::RenderViewportPanel()
{
	// 사이클 ②에서 그리드 + 요소 쿼드(DrawList)를, 사이클 ④에서 클릭 선택을 채운다.
	ImGui::TextUnformatted("Canvas Viewport");
	ImGui::Separator();
}

void FUIEditorWidget::RenderDetailsPanel()
{
	// 사이클 ⑤에서 W/H·Offset·Pivot·Color 직접 편집 필드를 채운다.
	ImGui::TextUnformatted("Details");
	ImGui::Separator();
}

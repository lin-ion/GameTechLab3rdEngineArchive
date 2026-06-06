#include "UIEditorWidget.h"

#include "Object/Object.h"
#include "Object/GarbageCollection.h"
#include "Serialization/SceneSaveManager.h"
#include "UI/UIAsset.h"
#include "UI/Canvas/UICanvas.h"
#include "UI/Canvas/UICanvasActor.h"
#include "UI/Canvas/UICanvasManager.h"
#include "UI/Canvas/UIElement.h"
#include "UI/Canvas/UIRect.h"

#include <imgui.h>

namespace
{
	// 뷰포트 드로우 — FSimpleUIPass::CollectVisible 로직을 ImGui DrawList 로 미러(진단 §C, Option B).
	// 가시 요소의 ScreenRect(=레퍼런스*Scale, 캔버스 원점 기준)를 Origin 더해 사각형으로 그린다.
	void DrawUIElementRect(UUIElement* Element, ImDrawList* DL, const ImVec2& Origin)
	{
		if (!Element)
		{
			return;
		}
		if (Element->IsVisibleRect())
		{
			const FUIRect& R = Element->GetScreenRect();
			const FVector4 C = Element->GetColor();
			const ImVec2   Min(Origin.x + R.Pos.X, Origin.y + R.Pos.Y);
			const ImVec2   Max(Min.x + R.Size.X, Min.y + R.Size.Y);
			DL->AddRectFilled(Min, Max, ImGui::GetColorU32(ImVec4(C.R, C.G, C.B, C.A)));
			DL->AddRect(Min, Max, IM_COL32(0, 0, 0, 60));
		}
		for (USceneComponent* Child : Element->GetChildren())
		{
			if (UUIElement* ChildElement = Cast<UUIElement>(Child))
			{
				DrawUIElementRect(ChildElement, DL, Origin);
			}
		}
	}

	// 계층 트리 — 캔버스 GetChildren 재귀를 ImGui::TreeNode 로(진단 §B). 선택 동기화는 사이클 ④.
	void DrawHierarchyNode(UUIElement* Element)
	{
		if (!Element)
		{
			return;
		}

		bool bHasChild = false;
		for (USceneComponent* Child : Element->GetChildren())
		{
			if (Cast<UUIElement>(Child)) { bHasChild = true; break; }
		}

		ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen
			| ImGuiTreeNodeFlags_SpanAvailWidth;
		if (!bHasChild)
		{
			Flags |= ImGuiTreeNodeFlags_Leaf;
		}

		const bool bOpen = ImGui::TreeNodeEx((void*)Element, Flags, "%s", Element->GetClass()->GetName());
		if (bOpen)
		{
			for (USceneComponent* Child : Element->GetChildren())
			{
				if (UUIElement* ChildElement = Cast<UUIElement>(Child))
				{
					DrawHierarchyNode(ChildElement);
				}
			}
			ImGui::TreePop();
		}
	}
}

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
	ImGui::TextUnformatted("Hierarchy");
	ImGui::Separator();
	if (!Canvas)
	{
		ImGui::TextDisabled("No canvas");
		return;
	}
	DrawHierarchyNode(Canvas);
}

void FUIEditorWidget::RenderViewportPanel()
{
	ImDrawList*  DL     = ImGui::GetWindowDrawList();
	const ImVec2 Origin = ImGui::GetCursorScreenPos();
	const ImVec2 Avail  = ImGui::GetContentRegionAvail();
	const ImVec2 RegionMax(Origin.x + Avail.x, Origin.y + Avail.y);

	DL->AddRectFilled(Origin, RegionMax, IM_COL32(28, 28, 32, 255));

	if (!Canvas)
	{
		ImGui::TextDisabled("No canvas");
		return;
	}

	// 휠 줌(뷰포트 호버 시). 기본 스케일 = 뷰포트 높이를 레퍼런스 1080 에 맞춤(진단 §C).
	const ImGuiIO& IO = ImGui::GetIO();
	if (ImGui::IsWindowHovered() && IO.MouseWheel != 0.0f)
	{
		ViewportZoom *= (1.0f + IO.MouseWheel * 0.1f);
		if (ViewportZoom < 0.1f) ViewportZoom = 0.1f;
		if (ViewportZoom > 5.0f) ViewportZoom = 5.0f;
	}

	const float RefW  = 1920.0f;
	const float RefH  = 1080.0f;
	const float Scale = ((Avail.y > 0.0f) ? (Avail.y / RefH) : 1.0f) * ViewportZoom;

	// 단일 캔버스 레이아웃(전역 레지스트리 미사용 seam) → 각 요소 ScreenRect 갱신.
	FUICanvasManager::Get().LayoutCanvas(Canvas, Scale);

	// 레퍼런스 해상도(1920x1080) 경계 + 그리드.
	const ImVec2 RefMax(Origin.x + RefW * Scale, Origin.y + RefH * Scale);
	const float  Step = 120.0f * Scale;
	if (Step >= 4.0f)
	{
		for (float x = Origin.x; x <= RefMax.x && x <= RegionMax.x; x += Step)
			DL->AddLine(ImVec2(x, Origin.y), ImVec2(x, (RefMax.y < RegionMax.y ? RefMax.y : RegionMax.y)), IM_COL32(55, 55, 62, 255));
		for (float y = Origin.y; y <= RefMax.y && y <= RegionMax.y; y += Step)
			DL->AddLine(ImVec2(Origin.x, y), ImVec2((RefMax.x < RegionMax.x ? RefMax.x : RegionMax.x), y), IM_COL32(55, 55, 62, 255));
	}
	DL->AddRect(Origin, RefMax, IM_COL32(120, 120, 135, 255));

	// 요소 드로우(뷰포트 영역 클리핑).
	DL->PushClipRect(Origin, RegionMax, true);
	DrawUIElementRect(Canvas, DL, Origin);
	DL->PopClipRect();

	DL->AddText(ImVec2(Origin.x + 6.0f, Origin.y + 6.0f), IM_COL32(170, 170, 180, 255), "Canvas Viewport (wheel: zoom)");
	ImGui::Dummy(Avail);  // 레이아웃 영역 점유.
}

void FUIEditorWidget::RenderDetailsPanel()
{
	// 사이클 ⑤에서 W/H·Offset·Pivot·Color 직접 편집 필드를 채운다.
	ImGui::TextUnformatted("Details");
	ImGui::Separator();
}

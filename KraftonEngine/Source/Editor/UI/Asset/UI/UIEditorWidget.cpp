#include "UIEditorWidget.h"

#include "Object/Object.h"
#include "Object/GarbageCollection.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Serialization/SceneSaveManager.h"
#include "UI/UIAsset.h"
#include "UI/UIAssetManager.h"
#include "UI/Canvas/UICanvas.h"
#include "UI/Canvas/UICanvasActor.h"
#include "UI/Canvas/UICanvasManager.h"
#include "UI/Canvas/UIElement.h"
#include "UI/Canvas/UIButton.h"
#include "UI/Canvas/UIImage.h"
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

	// 계층 트리 — 캔버스 GetChildren 재귀를 ImGui::TreeNode 로(진단 §B). 선택 하이라이트 + 클릭 선택(사이클 ④).
	void DrawHierarchyNode(UUIElement* Element, UUIElement*& Selected)
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
		if (Element == Selected)
		{
			Flags |= ImGuiTreeNodeFlags_Selected;
		}

		const bool bOpen = ImGui::TreeNodeEx((void*)Element, Flags, "%s", Element->GetClass()->GetName());
		if (ImGui::IsItemClicked())
		{
			Selected = Element;   // 트리 클릭 → 선택(뷰포트/디테일과 공유).
		}
		if (bOpen)
		{
			for (USceneComponent* Child : Element->GetChildren())
			{
				if (UUIElement* ChildElement = Cast<UUIElement>(Child))
				{
					DrawHierarchyNode(ChildElement, Selected);
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
	Canvas   = nullptr;
	Selected = nullptr;
	if (OwnerActor)
	{
		UObjectManager::Get().DestroyObject(OwnerActor);   // 컴포넌트 트리까지 정리(2-phase GC).
		OwnerActor = nullptr;
	}
}

void FUIEditorWidget::SaveToAsset()
{
	UUIAsset* Asset = Cast<UUIAsset>(EditedObject);
	if (!Asset || !Canvas)
	{
		return;
	}

	// 편집된 라이브 트리를 다시 JSON 으로 직렬화(저장 방향은 ⓪에서 추가한 SerializeUITree 재사용)
	// → 에셋 블롭 갱신 → FAssetPackage string payload 로 .uasset 기록.
	Asset->SetCanvasData(FSceneSaveManager::SerializeUITree(Canvas));
	if (FUIAssetManager::Get().Save(Asset))
	{
		ClearDirty();
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

	// 상단 툴바 — 저장(편집 트리 → .uasset 재직렬화) + dirty 표시(사이클 ⑥).
	if (ImGui::Button("Save"))
	{
		SaveToAsset();
	}
	ImGui::SameLine();
	ImGui::TextDisabled(IsDirty() ? "(modified)" : "(saved)");
	ImGui::Separator();

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
	ImGui::TextUnformatted("Palette");
	ImGui::Separator();
	if (!Canvas)
	{
		ImGui::TextDisabled("No canvas");
		return;
	}

	const float W = ImGui::GetContentRegionAvail().x;
	if (ImGui::Button("Canvas", ImVec2(W, 0.0f))) { SpawnElement(UUICanvas::StaticClass()); }
	if (ImGui::Button("Button", ImVec2(W, 0.0f))) { SpawnElement(UUIButton::StaticClass()); }
	if (ImGui::Button("Image",  ImVec2(W, 0.0f))) { SpawnElement(UUIImage::StaticClass()); }

	ImGui::Spacing();
	ImGui::TextDisabled("Adds under selection,\nelse under Canvas.");
}

void FUIEditorWidget::SpawnElement(UClass* ElementClass)
{
	if (!ElementClass || !OwnerActor || !Canvas)
	{
		return;
	}

	// 진단 §D: AddComponentToActor 모델 — CreateObject + RegisterComponent + AttachToComponent.
	UObject*    Obj     = FObjectFactory::Get().Create(ElementClass->GetName(), OwnerActor);
	UUIElement* NewElem = Cast<UUIElement>(Obj);
	if (!NewElem)
	{
		if (Obj) UObjectManager::Get().DestroyObject(Obj);
		return;
	}
	OwnerActor->RegisterComponent(NewElem);

	// 부모 = 선택 노드(없으면 캔버스 루트). 자식 수 기반 cascade 로 겹침 방지.
	UUIElement* Parent     = Selected ? Selected : Canvas;
	int32       ChildCount = 0;
	for (USceneComponent* Child : Parent->GetChildren())
	{
		if (Cast<UUIElement>(Child)) ++ChildCount;
	}
	NewElem->SetPosition(FVector2(40.0f + ChildCount * 30.0f, 40.0f + ChildCount * 30.0f));
	NewElem->AttachToComponent(Parent);

	Selected = NewElem;   // 새로 만든 요소를 선택 상태로(디테일/다음 생성 부모).
	MarkDirty();
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
	DrawHierarchyNode(Canvas, Selected);
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

	// 입력 캡처 표면 — 뷰포트 위 좌클릭/드래그를 InvisibleButton 이 흡수해 ImGui 창 이동으로
	// 전파되지 않게 한다(사이클 ⑦). 영역 점유도 겸하므로 끝의 별도 Dummy 불필요.
	if (Avail.x > 0.0f && Avail.y > 0.0f)
	{
		ImGui::InvisibleButton("##UIViewportSurface", Avail, ImGuiButtonFlags_MouseButtonLeft);
	}
	const bool     bHovered = ImGui::IsItemHovered();
	const bool     bActive  = ImGui::IsItemActive();
	const ImGuiIO& IO       = ImGui::GetIO();

	// 휠 줌(표면 호버 시). 기본 스케일 = 뷰포트 높이를 레퍼런스 1080 에 맞춤(진단 §C).
	if (bHovered && IO.MouseWheel != 0.0f)
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

	// 프레스 순간 → 히트테스트로 선택(빈 곳은 해제). 좌표는 캔버스 원점 기준 역변환(진단 §C/E, 사이클 ④).
	if (ImGui::IsItemActivated())
	{
		const ImVec2 M = ImGui::GetMousePos();
		Selected = FUICanvasManager::Get().HitTestCanvas(Canvas, FVector2(M.x - Origin.x, M.y - Origin.y));
	}

	// 드래그 → 선택 요소 이동. 화면 델타를 Scale 로 나눠 레퍼런스 공간 델타로(사이클 7 TickEditor 로직 재사용).
	if (bActive && Selected && (IO.MouseDelta.x != 0.0f || IO.MouseDelta.y != 0.0f))
	{
		const float S = (Scale > 0.0f) ? Scale : 1.0f;
		Selected->SetPosition(Selected->GetPosition() + FVector2(IO.MouseDelta.x / S, IO.MouseDelta.y / S));
		MarkDirty();
	}

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

	// 요소 드로우(뷰포트 영역 클리핑) + 선택 강조.
	DL->PushClipRect(Origin, RegionMax, true);
	DrawUIElementRect(Canvas, DL, Origin);
	if (Selected)
	{
		const FUIRect& SR = Selected->GetScreenRect();
		const ImVec2   SMin(Origin.x + SR.Pos.X, Origin.y + SR.Pos.Y);
		const ImVec2   SMax(SMin.x + SR.Size.X, SMin.y + SR.Size.Y);
		DL->AddRect(SMin, SMax, IM_COL32(255, 180, 40, 255), 0.0f, 0, 2.0f);
	}
	DL->PopClipRect();

	DL->AddText(ImVec2(Origin.x + 6.0f, Origin.y + 6.0f), IM_COL32(170, 170, 180, 255), "Canvas Viewport (drag: move, wheel: zoom)");
}

void FUIEditorWidget::RenderDetailsPanel()
{
	ImGui::TextUnformatted("Details");
	ImGui::Separator();
	if (!Selected)
	{
		ImGui::TextDisabled("No selection");
		return;
	}

	ImGui::TextDisabled("%s", Selected->GetClass()->GetName());
	ImGui::Spacing();

	// 5필드 직접 바인딩(진단 §E). 텍스트 전용 필드 없음. 편집 즉시 RectTransform 반영 →
	// 다음 프레임 LayoutCanvas 가 ScreenRect 갱신 → 뷰포트 실시간 반영.
	FUIRectTransform& RT = Selected->GetRectTransform();

	float Size[2] = { RT.Size.X, RT.Size.Y };
	if (ImGui::DragFloat2("Size (W/H)", Size, 1.0f))
	{
		RT.Size = FVector2(Size[0], Size[1]);
		MarkDirty();
	}

	float Pos[2] = { RT.Position.X, RT.Position.Y };
	if (ImGui::DragFloat2("Offset (X/Y)", Pos, 1.0f))
	{
		RT.Position = FVector2(Pos[0], Pos[1]);
		MarkDirty();
	}

	float Pivot[2] = { RT.Pivot.X, RT.Pivot.Y };
	if (ImGui::DragFloat2("Pivot", Pivot, 0.01f))
	{
		RT.Pivot = FVector2(Pivot[0], Pivot[1]);
		MarkDirty();
	}

	FVector4 C      = Selected->GetColor();
	float    Col[4] = { C.R, C.G, C.B, C.A };
	if (ImGui::ColorEdit4("Color", Col))
	{
		Selected->SetColor(FVector4(Col[0], Col[1], Col[2], Col[3]));
		MarkDirty();
	}
}

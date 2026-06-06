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

	ImGui::SetNextWindowSize(ImVec2(480.0f, 360.0f), ImGuiCond_Once);

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

	// 사이클 2: 빈 골격. 이후 사이클에서 계층 트리 / RectTransform 속성 / 뷰포트 드래그 / 텍스트를 얹는다.
	ImGui::TextDisabled("UI Asset Editor");
	ImGui::Separator();
	ImGui::TextUnformatted(UIAsset->GetSourcePath().empty()
		? "Unsaved UI asset"
		: UIAsset->GetSourcePath().c_str());

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

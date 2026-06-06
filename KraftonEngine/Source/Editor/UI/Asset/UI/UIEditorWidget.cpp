#include "UIEditorWidget.h"

#include "Object/Object.h"
#include "UI/UIAsset.h"

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

	EditedObject = Object;
	bOpen        = true;
	ClearDirty();
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

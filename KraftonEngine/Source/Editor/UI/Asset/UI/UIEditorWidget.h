#pragma once
#include "Editor/UI/Asset/AssetEditorWidget.h"

// UI 에셋(UUIAsset) 전용 에디터 창(사이클 2).
// 이번 범위는 "더블클릭 → 빈 창" 골격까지. 내부 편집 기능(계층 트리 / RectTransform 속성 /
// 뷰포트 드래그 / 텍스트 입력)은 이후 사이클(진단 E/경계).
// CameraShake / FloatCurve 와 같이 문서 탭이 아닌 free-floating ImGui 창으로 뜬다 —
// FAssetEditorManager::Render 가 SupportsDocumentTabs()==false 위젯의 Render 를 매 프레임 호출.
class FUIEditorWidget : public FAssetEditorWidget
{
public:
	FUIEditorWidget() = default;

	bool CanEdit(UObject* Object) const override;
	void Open(UObject* Object) override;
	void Render(float DeltaTime) override;
};

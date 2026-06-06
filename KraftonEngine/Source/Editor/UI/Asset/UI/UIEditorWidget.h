#pragma once
#include "Editor/UI/Asset/AssetEditorWidget.h"

class AUICanvasActor;
class UUICanvas;
class UUIAsset;

// UI 에셋(UUIAsset) 전용 에디터 창. 4분할(팔레트/계층트리/캔버스 뷰포트/디테일) 구성.
// 진단서 UIEditor_4Panel_Layout_Diagnosis.md 기준. ImGui 즉시모드, free-floating 창.
class FUIEditorWidget : public FAssetEditorWidget
{
public:
	FUIEditorWidget() = default;

	bool CanEdit(UObject* Object) const override;
	void Open(UObject* Object) override;
	void Close() override;
	void Render(float DeltaTime) override;
	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	// .uasset JSON 블롭(UUIAsset::CanvasData) → 라이브 UUICanvas 트리 복원/파괴(사이클 ⓪).
	// 에디터 전용 소유자 액터(월드 미사용). 전역 FUICanvasManager 에는 등록하지 않는다
	// (런타임 SimpleUIPass/LayoutAll 오염 방지) — 레이아웃/히트테스트는 per-canvas seam 사용.
	void BuildLiveTree(UUIAsset* Asset);
	void DestroyLiveTree();

	AUICanvasActor* OwnerActor = nullptr;  // 복원 트리의 소유자(에디터 수명). GC keepalive 대상.
	UUICanvas*      Canvas     = nullptr;  // 복원된 루트 캔버스(편집/레이아웃/드로우 대상).
};

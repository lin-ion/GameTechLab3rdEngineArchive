#pragma once

#include "Core/Types/CoreTypes.h"
#include "Core/Singleton.h"
#include "Object/GarbageCollection.h"

class UUICanvas;

// 신규 계층형 UI 의 레지스트리 + GC 루트.
// RmlUi 의 UUIManager 와 독립인 별도 경량 시스템(진단 F1: 병존). 레이아웃(사이클3) /
// 드로우(사이클5) / 히트테스트(사이클7) 가 이 매니저를 통해 활성 Canvas 목록에 접근한다.
// UObject 가 아니므로 F 접두사(UUIManager 와 혼동 방지).
class FUICanvasManager : public TSingleton<FUICanvasManager>, public FGCObject
{
	friend class TSingleton<FUICanvasManager>;

public:
	void RegisterCanvas(UUICanvas* Canvas);
	void UnregisterCanvas(UUICanvas* Canvas);
	const TArray<UUICanvas*>& GetCanvases() const { return Canvases; }

	// 액터 없이 런타임에서 직접 Canvas 를 만들 때(테스트 / HUD 부트스트랩). 매니저가 keepalive 한다.
	UUICanvas* CreateCanvas();

	// FGCObject — 등록된 Canvas(및 그 자식 트리)를 GC sweep 으로부터 보호한다.
	// 각 노드의 자식은 USceneComponent::AddReferencedObjects 가 재귀로 보고한다(진단 A1).
	void AddReferencedObjects(FReferenceCollector& Collector) override;
	const char* GetReferencerName() const override { return "FUICanvasManager"; }

private:
	FUICanvasManager() = default;
	~FUICanvasManager() = default;

	TArray<UUICanvas*> Canvases;
};

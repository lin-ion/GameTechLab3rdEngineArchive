#pragma once

#include "UI/Canvas/UIElement.h"
#include "Object/Ptr/WeakObjectPtr.h"

#include "Source/Engine/UI/Canvas/UILabel.generated.h"

class UUserWidget;

// 텍스트 라벨 노드 — 자체 쿼드는 그리지 않고(빈 Element) RmlUi 문서를 마운트해 텍스트를
// 렌더한다(결정: 라벨 = RmlUi 마운트, 사이클 6). 레이아웃이 정한 화면 위치를 매 프레임
// 따라가며, RmlUi dp_ratio = GlobalScale(사이클 4) 이므로 dp 좌표/폰트가 신규 UI 와 1:1 로
// 스케일된다. RmlUi 의존은 이 브리지 클래스(.cpp)에만 격리한다.
UCLASS()
class UUILabel : public UUIElement
{
public:
	GENERATED_BODY()
	UUILabel() { SetVisibleRect(false); }

	void SetText(const FString& InText) { Text = InText; }
	const FString& GetText() const { return Text; }

	// 레이아웃 갱신 직후 RmlUi 위젯을 생성(최초 1회)하고 위치/텍스트를 동기화한다.
	void OnLayoutUpdated(float GlobalScale) override;

	void BeginDestroy() override;

private:
	FString Text;
	TWeakObjectPtr<UUserWidget> Widget;
	bool bMountAttempted = false;
};

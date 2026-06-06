#pragma once

#include "UI/Canvas/UITextElement.h"
#include "Object/Ptr/WeakObjectPtr.h"

#include "Source/Engine/UI/Canvas/UILabel.generated.h"

class UUserWidget;

// "배경 없는 텍스트" 팔레트 프리셋 — bVisibleRect=false 라 단색 쿼드는 안 그리고 텍스트만 보인다.
// 텍스트 데이터/속성(내용·폰트·정렬·색)은 중간 클래스 UUITextElement 에 있다(R3: 자체 Text 멤버 없음).
// 현재(사이클 ①)는 RmlUi 마운트/동기화를 이 클래스 .cpp 에 잔존시켜 외형 회귀 0 을 유지하고,
// 사이클 ②에서 그 메커니즘을 UUITextElement 로 이전해 Button/Image 와 공유한다(이후 이 클래스는 생성자 전용).
UCLASS()
class UUILabel : public UUITextElement
{
public:
	GENERATED_BODY()
	UUILabel()
	{
		SetVisibleRect(false);
		SetText("Text");   // 팔레트 스폰 직후 보이도록 기본 텍스트(빈 컨테이너와 달리 텍스트 프리셋).
	}

	// 레이아웃 갱신 직후 RmlUi 위젯을 생성(최초 1회)하고 위치/텍스트를 동기화한다. (사이클 ②에서 베이스로 이전)
	void OnLayoutUpdated(float GlobalScale) override;

	void BeginDestroy() override;

private:
	TWeakObjectPtr<UUserWidget> Widget;
	bool bMountAttempted = false;
};

#pragma once

#include "UI/Canvas/UIElement.h"

#include "Source/Engine/UI/Canvas/UIButton.generated.h"

// 단색 버튼 요소(팔레트 3종). 가시 rect 유지(베이스 기본 bVisibleRect=true) → 단색 쿼드를 그린다.
// 텍스처 참조/텍스트/타입별 특수속성 없음 — 공통 속성(W/H·Offset·Pivot·Color)만(진단 §D).
UCLASS()
class UUIButton : public UUIElement
{
public:
	GENERATED_BODY()
	UUIButton()
	{
		SetSize(FVector2(200.0f, 80.0f));
		SetColor(FVector4(0.25f, 0.55f, 0.32f, 1.0f));
	}
};

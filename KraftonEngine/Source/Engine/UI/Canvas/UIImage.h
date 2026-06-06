#pragma once

#include "UI/Canvas/UIElement.h"

#include "Source/Engine/UI/Canvas/UIImage.generated.h"

// 단색 이미지 요소(팔레트 3종). 텍스처 참조 아님 — 단색 채움만(진단 §D 범위).
// 가시 rect 유지. 공통 속성(W/H·Offset·Pivot·Color)만, 타입별 특수속성 없음.
UCLASS()
class UUIImage : public UUIElement
{
public:
	GENERATED_BODY()
	UUIImage()
	{
		SetSize(FVector2(200.0f, 200.0f));
		SetColor(FVector4(0.70f, 0.70f, 0.74f, 1.0f));
	}
};

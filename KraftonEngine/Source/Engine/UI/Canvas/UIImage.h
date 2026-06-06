#pragma once

#include "UI/Canvas/UITextElement.h"

#include "Source/Engine/UI/Canvas/UIImage.generated.h"

// 단색 이미지 요소(팔레트 3종). 텍스처 참조 아님 — 단색 채움만(텍스처는 이미지 기능 차례에 추가).
// 텍스트(내용·폰트·정렬·색)는 중간 클래스 UUITextElement 에서 상속한다(비우면 미표시). 가시 rect 유지.
UCLASS()
class UUIImage : public UUITextElement
{
public:
	GENERATED_BODY()
	UUIImage()
	{
		SetSize(FVector2(200.0f, 200.0f));
		SetColor(FVector4(0.70f, 0.70f, 0.74f, 1.0f));
	}
};

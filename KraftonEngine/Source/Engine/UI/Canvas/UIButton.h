#pragma once

#include "UI/Canvas/UITextElement.h"

#include "Source/Engine/UI/Canvas/UIButton.generated.h"

// 단색 버튼 요소(팔레트 3종). 가시 rect 유지(베이스 기본 bVisibleRect=true) → 단색 쿼드를 그린다.
// 텍스트(내용·폰트·정렬·색)는 중간 클래스 UUITextElement 에서 상속한다 — 비우면 텍스트 미표시,
// 채우면 단색 쿼드 위에 텍스트 오버레이(라벨 버튼). 텍스처 참조는 아직 없음(이미지 기능 차례).
UCLASS()
class UUIButton : public UUITextElement
{
public:
	GENERATED_BODY()
	UUIButton()
	{
		SetSize(FVector2(200.0f, 80.0f));
		SetColor(FVector4(0.25f, 0.55f, 0.32f, 1.0f));
	}
};

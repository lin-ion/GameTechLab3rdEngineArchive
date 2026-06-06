#pragma once

#include "Component/SceneComponent.h"
#include "UI/Canvas/UIRect.h"
#include "UI/Canvas/UIRectTransform.h"

#include "Source/Engine/UI/Canvas/UIElement.generated.h"

// 계층형 UI 트리의 기본 노드.
// USceneComponent 를 상속해 부모-자식 트리 / 사이클검사 / GC keepalive 를 재사용하되(진단 A2),
// 레이아웃은 3D RelativeTransform 이 아니라 FUIRectTransform 으로만 한다(진단 C5).
// 화면 사각형(ScreenRect)은 레이아웃 패스가 매 프레임 top-down 으로 채운다(진단 C3, 사이클 3).
UCLASS()
class UUIElement : public USceneComponent
{
public:
	GENERATED_BODY()
	UUIElement() { SetComponentTickEnabled(false); }

	FUIRectTransform& GetRectTransform() { return RectTransform; }
	const FUIRectTransform& GetRectTransform() const { return RectTransform; }

	void SetPosition(const FVector2& InPosition) { RectTransform.Position = InPosition; }
	void SetSize(const FVector2& InSize) { RectTransform.Size = InSize; }
	void SetAnchor(const FVector2& InAnchor) { RectTransform.Anchor = InAnchor; }
	void SetPivot(const FVector2& InPivot) { RectTransform.Pivot = InPivot; }

	FVector2 GetPosition() const { return RectTransform.Position; }
	FVector2 GetSize() const { return RectTransform.Size; }

	// 레이아웃 패스가 채우는 화면 사각형(스크린 픽셀, GlobalScale 반영). 사이클 3에서 갱신.
	const FUIRect& GetScreenRect() const { return ScreenRect; }
	void SetScreenRect(const FUIRect& InRect) { ScreenRect = InRect; }

	// 레이아웃 패스가 ScreenRect 를 갱신한 직후 호출되는 훅(기본 no-op). 화면 위치에 종속된
	// 외부 리소스(예: UUILabel 의 RmlUi 텍스트 위젯)를 동기화할 때 override 한다(사이클 6).
	virtual void OnLayoutUpdated(float GlobalScale) { (void)GlobalScale; }

	// 이 노드가 사각형을 그릴지 여부. Canvas / Group 같은 순수 컨테이너는 false.
	bool IsVisibleRect() const { return bVisibleRect; }
	void SetVisibleRect(bool bVisible) { bVisibleRect = bVisible; }

	// 단색 배경색(RGBA). 드로우 패스가 ScreenRect 를 이 색의 쿼드로 그린다(사이클 5).
	void SetColor(const FVector4& InColor) { BackgroundColor = InColor; }
	FVector4 GetColor() const { return BackgroundColor; }

protected:
	FUIRectTransform RectTransform;   // 사이클 8에서 PF_Save 직렬화 대상이 됨
	FUIRect ScreenRect;               // 레이아웃 결과 캐시(직렬화 안 함)
	bool bVisibleRect = true;
	FVector4 BackgroundColor{ 0.2f, 0.4f, 0.8f, 0.7f };
};

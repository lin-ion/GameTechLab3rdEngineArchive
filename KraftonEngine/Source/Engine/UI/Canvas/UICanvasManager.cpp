#include "UI/Canvas/UICanvasManager.h"

#include "UI/Canvas/UICanvas.h"
#include "Object/Object.h"

void FUICanvasManager::RegisterCanvas(UUICanvas* Canvas)
{
	if (!Canvas)
	{
		return;
	}
	for (UUICanvas* Existing : Canvases)
	{
		if (Existing == Canvas)
		{
			return;
		}
	}
	Canvases.push_back(Canvas);
}

void FUICanvasManager::UnregisterCanvas(UUICanvas* Canvas)
{
	for (auto It = Canvases.begin(); It != Canvases.end(); ++It)
	{
		if (*It == Canvas)
		{
			Canvases.erase(It);
			return;
		}
	}
}

UUICanvas* FUICanvasManager::CreateCanvas()
{
	UUICanvas* Canvas = UObjectManager::Get().CreateObject<UUICanvas>();
	RegisterCanvas(Canvas);
	return Canvas;
}

void FUICanvasManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	// UUICanvas* → UObject* 암시적 업캐스트. 각 Canvas 의 자식 트리는
	// USceneComponent::AddReferencedObjects 가 별도로 보고한다.
	Collector.AddReferencedObjects(Canvases, "UICanvas");
}

void FUICanvasManager::LayoutAll()
{
	for (UUICanvas* Canvas : Canvases)
	{
		if (!Canvas)
		{
			continue;
		}
		// 루트는 origin=(0,0) 에서 시작하며, 부모 크기는 Canvas 자신의 레퍼런스 크기다.
		// Canvas 의 anchor/pivot/position 은 기본 0 이라 FinalPos 도 (0,0) 이 된다.
		const FVector2 CanvasSize = Canvas->GetRectTransform().Size;
		LayoutElement(Canvas, FVector2(0.0f, 0.0f), CanvasSize, GlobalScale);
	}
}

void FUICanvasManager::LayoutElement(UUIElement* Element, const FVector2& ParentOrigin,
                                     const FVector2& ParentSize, float Scale)
{
	if (!Element)
	{
		return;
	}

	const FUIRectTransform& RT = Element->GetRectTransform();

	// 레퍼런스 좌표 공간(좌상단 원점, Y-down)에서 이 노드의 좌상단 위치(진단 C2).
	//   AnchorPx = ParentSize * anchor       (성분별)
	//   FinalPos = ParentOrigin + AnchorPx + position - (size * pivot)   (성분별)
	const FVector2 AnchorPx = ComponentMul(ParentSize, RT.Anchor);
	const FVector2 FinalPos = ParentOrigin + AnchorPx + RT.Position - ComponentMul(RT.Size, RT.Pivot);

	// 화면 사각형 = 레퍼런스 결과에 GlobalScale 적용(진단 D3). 드로우/히트테스트가 이 값을 쓴다.
	FUIRect Screen;
	Screen.Pos = FinalPos * Scale;
	Screen.Size = RT.Size * Scale;
	Element->SetScreenRect(Screen);

	// 자식은 이 노드의 레퍼런스 좌상단/크기를 부모 기준으로 받아 top-down 누적(진단 C3).
	for (USceneComponent* Child : Element->GetChildren())
	{
		if (UUIElement* ChildElement = Cast<UUIElement>(Child))
		{
			LayoutElement(ChildElement, FinalPos, RT.Size, Scale);
		}
	}
}

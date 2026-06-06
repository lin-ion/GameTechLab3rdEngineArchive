#include "UI/Canvas/UICanvasManager.h"

#include "UI/Canvas/UICanvas.h"
#include "UI/Canvas/UIElement.h"
#include "UI/Canvas/UILabel.h"
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

UUICanvas* FUICanvasManager::CreateDebugTestCanvas()
{
	UUICanvas* Canvas = CreateCanvas();

	auto MakeRect = [Canvas](const FVector2& Anchor, const FVector2& Pivot,
	                         const FVector2& Position, const FVector2& Size, const FVector4& Color)
	{
		UUIElement* Element = UObjectManager::Get().CreateObject<UUIElement>();
		Element->SetAnchor(Anchor);
		Element->SetPivot(Pivot);
		Element->SetPosition(Position);
		Element->SetSize(Size);
		Element->SetColor(Color);
		Canvas->AddChild(Element);
	};

	// 좌상단 앵커/피벗 — 화면 좌상단에서 (50,50) 떨어진 곳.
	MakeRect({ 0.0f, 0.0f }, { 0.0f, 0.0f }, { 50.0f, 50.0f }, { 300.0f, 120.0f },
	         { 0.85f, 0.2f, 0.2f, 0.85f });
	// 중앙 앵커 + 중앙 피벗 — 화면 정중앙에 정렬.
	MakeRect({ 0.5f, 0.5f }, { 0.5f, 0.5f }, { 0.0f, 0.0f }, { 400.0f, 200.0f },
	         { 0.2f, 0.7f, 0.3f, 0.85f });
	// 우하단 앵커 + 우하단 피벗 — 화면 우하단에서 (-40,-40) 안쪽.
	MakeRect({ 1.0f, 1.0f }, { 1.0f, 1.0f }, { -40.0f, -40.0f }, { 250.0f, 150.0f },
	         { 0.2f, 0.4f, 0.9f, 0.85f });

	// 텍스트 라벨(RmlUi 마운트) — 상단 중앙 빈 영역. 사이클 6 검증용.
	{
		UUILabel* Label = UObjectManager::Get().CreateObject<UUILabel>();
		Label->SetAnchor({ 0.5f, 0.0f });
		Label->SetPivot({ 0.5f, 0.0f });
		Label->SetPosition({ 0.0f, 40.0f });
		Label->SetText("SimpleUI Label");
		Canvas->AddChild(Label);
	}

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

	// 화면 위치 종속 외부 리소스 동기화 훅(예: UUILabel 의 RmlUi 텍스트). 기본 no-op.
	Element->OnLayoutUpdated(Scale);

	// 자식은 이 노드의 레퍼런스 좌상단/크기를 부모 기준으로 받아 top-down 누적(진단 C3).
	for (USceneComponent* Child : Element->GetChildren())
	{
		if (UUIElement* ChildElement = Cast<UUIElement>(Child))
		{
			LayoutElement(ChildElement, FinalPos, RT.Size, Scale);
		}
	}
}

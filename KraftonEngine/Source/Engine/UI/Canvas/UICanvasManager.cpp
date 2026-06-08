#include "UI/Canvas/UICanvasManager.h"

#include "UI/Canvas/UICanvas.h"
#include "UI/Canvas/UIElement.h"
#include "UI/Canvas/UILabel.h"
#include "Object/Object.h"
#include "Input/InputSystem.h"

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
		// 런타임 패스 → 외부(RmlUi) 동기화 허용(bSyncExternal=true).
		LayoutElement(Canvas, FVector2(0.0f, 0.0f), CanvasSize, GlobalScale, /*bSyncExternal=*/true);
	}
}

void FUICanvasManager::LayoutCanvas(UUICanvas* Canvas, float Scale, bool bSyncExternal)
{
	if (!Canvas)
	{
		return;
	}
	// LayoutAll 과 동일 규칙(origin=(0,0), parentSize=Canvas 레퍼런스 크기)으로 한 트리만 계산.
	// 에디터 호출은 bSyncExternal=false(기본) → RmlUi 마운트/동기화 스킵(R1).
	const FVector2 CanvasSize = Canvas->GetRectTransform().Size;
	LayoutElement(Canvas, FVector2(0.0f, 0.0f), CanvasSize, Scale, bSyncExternal);
}

void FUICanvasManager::LayoutElement(UUIElement* Element, const FVector2& ParentOrigin,
                                     const FVector2& ParentSize, float Scale, bool bSyncExternal)
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

	// 화면 위치 종속 외부 리소스 동기화 훅(예: UUITextElement 의 RmlUi 텍스트). bSyncExternal 게이트(R1).
	Element->OnLayoutUpdated(Scale, bSyncExternal);

	// 자식은 이 노드의 레퍼런스 좌상단/크기를 부모 기준으로 받아 top-down 누적(진단 C3).
	for (USceneComponent* Child : Element->GetChildren())
	{
		if (UUIElement* ChildElement = Cast<UUIElement>(Child))
		{
			LayoutElement(ChildElement, FinalPos, RT.Size, Scale, bSyncExternal);
		}
	}
}

void FUICanvasManager::HitTestRecursive(UUIElement* Element, const FVector2& MousePos, UUIElement*& OutTop)
{
	if (!Element)
	{
		return;
	}
	// 가시 사각형이고 마우스를 포함하면 후보. pre-order 라 나중에 그린(=위에 있는) 것이 덮어쓴다.
	if (Element->IsVisibleRect() && Element->GetScreenRect().Contains(MousePos))
	{
		OutTop = Element;
	}
	for (USceneComponent* Child : Element->GetChildren())
	{
		if (UUIElement* ChildElement = Cast<UUIElement>(Child))
		{
			HitTestRecursive(ChildElement, MousePos, OutTop);
		}
	}
}

UUIElement* FUICanvasManager::HitTest(const FVector2& MousePos) const
{
	UUIElement* Top = nullptr;
	for (UUICanvas* Canvas : Canvases)
	{
		HitTestRecursive(Canvas, MousePos, Top);
	}
	return Top;
}

UUIElement* FUICanvasManager::HitTestCanvas(UUICanvas* Canvas, const FVector2& MousePos) const
{
	UUIElement* Top = nullptr;
	HitTestRecursive(Canvas, MousePos, Top);
	return Top;
}

void FUICanvasManager::TickEditor()
{
	InputSystem& Input = InputSystem::Get();

	// 에디터 모드 토글 (F9). 토글 시 현재 잡고 있던 대상 해제.
	if (Input.GetKeyDown(VK_F9))
	{
		bEditorMode = !bEditorMode;
		GrabbedElement = nullptr;
	}

	if (!bEditorMode)
	{
		GrabbedElement = nullptr;
		return;
	}

	const POINT MP = Input.GetMouseClientPos();   // 클라이언트 px, 좌상단 원점(진단 A5/E1)
	const FVector2 MousePos(static_cast<float>(MP.x), static_cast<float>(MP.y));

	if (Input.GetKeyDown(VK_LBUTTON))
	{
		// 누른 순간 — 마우스 아래 최상위 가시 Element 를 잡는다.
		GrabbedElement = HitTest(MousePos);
	}
	else if (Input.GetKey(VK_LBUTTON))
	{
		// 드래그 중 — anchor/pivot 고정, position 만 갱신(진단 E2).
		// 마우스 델타는 스크린 px 이므로 GlobalScale 로 나눠 레퍼런스 px 로 환산해 더한다.
		if (UUIElement* Element = GrabbedElement.Get())
		{
			const float Scale = (GlobalScale > 0.0f) ? GlobalScale : 1.0f;
			const FVector2 Delta(static_cast<float>(Input.MouseDeltaX()) / Scale,
			                     static_cast<float>(Input.MouseDeltaY()) / Scale);
			Element->SetPosition(Element->GetPosition() + Delta);
		}
	}
	else
	{
		GrabbedElement = nullptr;
	}
}

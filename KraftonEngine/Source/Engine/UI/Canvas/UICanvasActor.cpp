#include "UI/Canvas/UICanvasActor.h"

#include "UI/Canvas/UICanvas.h"
#include "UI/Canvas/UICanvasManager.h"

void AUICanvasActor::InitCanvas()
{
	if (Canvas.Get())
	{
		return;
	}
	// 로드된 씬: 컴포넌트 트리 직렬화(DeserializeSceneComponentTree)가 이미 UUICanvas 를
	// RootComponent 로 복원했을 수 있다. 그 경우 새로 만들지 말고 재사용한다(중복 생성 방지).
	if (UUICanvas* Existing = Cast<UUICanvas>(GetRootComponent()))
	{
		Canvas = Existing;
		return;
	}
	UUICanvas* NewCanvas = AddComponent<UUICanvas>();
	SetRootComponent(NewCanvas);
	Canvas = NewCanvas;
}

void AUICanvasActor::BeginPlay()
{
	Super::BeginPlay();

	InitCanvas();
	if (UUICanvas* C = Canvas.Get())
	{
		FUICanvasManager::Get().RegisterCanvas(C);
	}
}

void AUICanvasActor::EndPlay()
{
	if (UUICanvas* C = Canvas.Get())
	{
		FUICanvasManager::Get().UnregisterCanvas(C);
	}
	Super::EndPlay();
}

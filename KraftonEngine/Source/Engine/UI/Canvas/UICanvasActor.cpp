#include "UI/Canvas/UICanvasActor.h"

#include "UI/Canvas/UICanvas.h"
#include "UI/Canvas/UICanvasManager.h"

void AUICanvasActor::InitCanvas()
{
	if (Canvas.Get())
	{
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

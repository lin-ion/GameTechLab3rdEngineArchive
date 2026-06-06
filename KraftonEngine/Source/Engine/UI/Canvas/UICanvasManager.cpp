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

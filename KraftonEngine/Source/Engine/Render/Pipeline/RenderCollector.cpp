#include "RenderCollector.h"

#include "GameFramework/World.h"
#include "GameFramework/Scene.h"
#include "GameFramework/AActor.h"
#include "Render/Pipeline/WorldRenderProxy.h"

#include <algorithm>

void FRenderCollector::CollectWorld(UWorld* World, const TArray<AActor*>& SelectedActors, FRenderBus& RenderBus)
{
	if (!World) return;

	if (UScene* PersistentScene = World->GetPersistentScene())
	{
		PersistentScene->GetRenderProxy().CollectWorld(RenderBus, SelectedActors, false);
	}

	if (UScene* ActiveScene = World->GetActiveScene())
	{
		ActiveScene->GetRenderProxy().CollectWorld(RenderBus, SelectedActors, true);
	}
}

void FRenderCollector::CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus)
{
	FGridEntry Entry = {};
	Entry.Grid.GridSpacing = GridSpacing;
	Entry.Grid.GridHalfLineCount = GridHalfLineCount;
	RenderBus.AddGridEntry(std::move(Entry));
}

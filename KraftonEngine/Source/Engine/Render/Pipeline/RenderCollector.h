#pragma once
#include "RenderBus.h"

class UWorld;
class AActor;
enum class ELevelViewportType : uint8;

class FRenderCollector {
public:
	void CollectWorld(UWorld* World, const TArray<AActor*>& SelectedActors, FRenderBus& RenderBus);
	void CollectGrid(float GridSpacing, int32 GridHalfLineCount, FRenderBus& RenderBus);
};

#pragma once
#include "Core/CoreTypes.h"
#include "Render/Pipeline/ViewContext.h"

class FPrimitiveProxy;
class AActor;

class FWorldRenderProxy
{
public:
	FWorldRenderProxy() = default;
	~FWorldRenderProxy();

	void AddProxy(FPrimitiveProxy* Proxy);
	void RemoveProxy(FPrimitiveProxy* Proxy);

	// World의 정보들을 바탕으로 Proxies 업데이트
	void CollectWorld(FViewContext& context, const TArray<AActor*>& SelectedActors);

private:
	TArray<FPrimitiveProxy*> Proxies;
};

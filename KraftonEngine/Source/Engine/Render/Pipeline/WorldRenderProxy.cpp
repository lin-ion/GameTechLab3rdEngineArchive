#include "WorldRenderProxy.h"
#include "PrimitiveProxy.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/GizmoComponent.h"
#include <algorithm>

void FWorldRenderProxy::AddProxy(FPrimitiveProxy* Proxy)
{
	if (!this || !Proxy) return;

	auto it = std::find(Proxies.begin(), Proxies.end(), Proxy);
	if (it == Proxies.end())
	{
		Proxies.push_back(Proxy);
	}
}

void FWorldRenderProxy::RemoveProxy(FPrimitiveProxy* Proxy)
{
	if (!this || !Proxy) return;

	auto it = std::find(Proxies.begin(), Proxies.end(), Proxy);
	if (it != Proxies.end())
	{
		Proxies.erase(it);
	}
}

void FWorldRenderProxy::CollectWorld(FRenderBus& Bus, const TArray<AActor*>& SelectedActors)
{
	if (!this) return;
	if (!Bus.GetShowFlags().bPrimitives) return;

	for (FPrimitiveProxy* Proxy : Proxies)
	{
		UPrimitiveComponent* Owner = Proxy->GetOwner();
		if (!Owner || !Owner->IsVisible()) continue;
		
		// 기즈모는 FRenderCollector::CollectGizmo에서 별도로 수집되므로 월드 순회에서는 스킵
		if (Owner->IsA<UGizmoComponent>()) continue;

		AActor* ActorOwner = Owner->GetOwner();
		if (!ActorOwner || !ActorOwner->IsVisible()) continue;

		bool bSelected = std::find(SelectedActors.begin(), SelectedActors.end(), ActorOwner) != SelectedActors.end();
		
		Proxy->CollectRender(Bus, bSelected);
	}
}

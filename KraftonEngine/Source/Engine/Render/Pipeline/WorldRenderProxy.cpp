#include "WorldRenderProxy.h"
#include "PrimitiveProxy.h"
#include "GameFramework/AActor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/GizmoComponent.h"
#include <algorithm>

FWorldRenderProxy::~FWorldRenderProxy()
{
	// Proxies are owned by UPrimitiveComponent and deleted there during OnUnregister.
	// This clear is just for safety.
	Proxies.clear();
}

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

void FWorldRenderProxy::CollectWorld(FViewContext& Context, const TArray<AActor*>& SelectedActors)
{
	if (!this) return;
	if (!Context.GetShowFlags().bPrimitives) return;

	for (FPrimitiveProxy* Proxy : Proxies)
	{
		UPrimitiveComponent* Owner = Proxy->GetOwner();
		if (!Owner || !Owner->IsVisible()) continue;

		bool bSelected = false;
		if (AActor* ActorOwner = Owner->GetOwner())
		{
			if (!ActorOwner->IsVisible()) continue;
			bSelected = std::find(SelectedActors.begin(), SelectedActors.end(), ActorOwner) != SelectedActors.end();
		}
		
		Proxy->SetSelected(bSelected);
		Proxy->OnDraw(Context);
	}
}

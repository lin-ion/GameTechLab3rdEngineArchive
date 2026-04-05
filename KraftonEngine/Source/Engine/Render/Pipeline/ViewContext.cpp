#include "ViewContext.h"
#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"
#include "Render/Pipeline/RenderSorting.h"
#include <algorithm>

void FViewContext::Clear()
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		PassQueues[i].clear();
	}

	FontEntries.clear();
	OverlayFontEntries.clear();
	SubUVEntries.clear();
	AABBEntries.clear();
	GridProxies.clear();
	CandidateProxies.clear();

	ViewportRTV = nullptr;
	ViewportDSV = nullptr;
	ViewportDepthSRV = nullptr;
	ViewportStencilSRV = nullptr;
}

void FViewContext::Reset()
{
	Clear();
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		TArray<FRenderCommand>().swap(PassQueues[i]);
	}
	TArray<FFontEntry>().swap(FontEntries);
	TArray<FFontEntry>().swap(OverlayFontEntries);
	TArray<FSubUVEntry>().swap(SubUVEntries);
	TArray<FAABBEntry>().swap(AABBEntries);
	TArray<FGridProxy>().swap(GridProxies);
	TArray<FPrimitiveProxy*>().swap(CandidateProxies);
}

void FViewContext::AddCommand(ERenderPass Pass, const FRenderCommand& InCommand)
{
	PassQueues[(uint32)Pass].push_back(InCommand);
}

void FViewContext::AddCommand(ERenderPass Pass, FRenderCommand&& InCommand)
{
	PassQueues[(uint32)Pass].push_back(std::move(InCommand));
}

void FViewContext::SortPass(ERenderPass Pass)
{
	FRenderSorting::SortCommands(PassQueues[(uint32)Pass]);
}

const TArray<FRenderCommand>& FViewContext::GetCommands(ERenderPass Pass) const
{
	return PassQueues[(uint32)Pass];
}

void FViewContext::AddFontEntry(FFontEntry&& Entry)
{
	FontEntries.push_back(std::move(Entry));
}

void FViewContext::AddOverlayFontEntry(FFontEntry&& Entry)
{
	OverlayFontEntries.push_back(std::move(Entry));
}

void FViewContext::AddSubUVEntry(FSubUVEntry&& Entry)
{
	SubUVEntries.push_back(std::move(Entry));
}

void FViewContext::AddAABBEntry(FAABBEntry&& Entry)
{
	AABBEntries.push_back(std::move(Entry));
}

void FViewContext::AddGridProxy(FGridProxy&& Proxy)
{
	GridProxies.push_back(std::move(Proxy));
}

void FViewContext::AddCandidateProxy(FPrimitiveProxy* Proxy)
{
	CandidateProxies.push_back(Proxy);
}

void FViewContext::AddCandidateProxyUnique(FPrimitiveProxy* Proxy)
{
	if (!Proxy)
	{
		return;
	}

	if (std::find(CandidateProxies.begin(), CandidateProxies.end(), Proxy) == CandidateProxies.end())
	{
		CandidateProxies.push_back(Proxy);
	}
}

void FViewContext::SetCameraInfo(const UCameraComponent* Camera)
{
	View = Camera->GetViewMatrix();
	Proj = Camera->GetProjectionMatrix();
	CameraForward = Camera->GetForwardVector();
	CameraRight = Camera->GetRightVector();
	CameraUp = Camera->GetUpVector();
	bIsOrtho = Camera->IsOrthogonal();
	OrthoWidth = Camera->GetOrthoWidth();
}

void FViewContext::SetViewportInfo(const FViewport* VP)
{
	viewportWidth = static_cast<float>(VP->GetWidth());
	viewportHeight = static_cast<float>(VP->GetHeight());
	ViewportRTV = VP->GetRTV();
	ViewportDSV = VP->GetDSV();
	ViewportDepthSRV = VP->GetDepthSRV();
	ViewportStencilSRV = VP->GetStencilSRV();
}

void FViewContext::SetRenderOptions(const FViewportRenderOptions& InOptions)
{
	Options = InOptions;
}

void FViewContext::SetViewportSize(float InWidth, float InHeight)
{
	viewportWidth = InWidth;
	viewportHeight = InHeight;
}

void FViewContext::CollectViewElements()
{
	if (Options.ShowFlags.bGrid)
	{
		FGridProxy Proxy;
		Proxy.Grid.GridSpacing = Options.GridSpacing;
		Proxy.Grid.GridHalfLineCount = Options.GridHalfLineCount;
		AddGridProxy(std::move(Proxy));
	}
}

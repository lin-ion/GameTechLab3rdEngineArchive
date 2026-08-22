#pragma once
#include "PostProcessBase.h"


class UHeightFogPostProcess : public IPostProcess
{
	EBlendState GetBlendState() const override { return EBlendState::Opaque; }

	bool IsEnabled(const FPostProcessViewDesc& ViewDesc) const override;

	void Execute(FD3DDevice* Device,
		ID3D11DeviceContext* Context,
		const FPostProcessViewDesc& ViewDesc,
		FRenderResources& Resources,
		const FRenderTargetSet& RenderTargets, // DepthStencilSRV, SelectionMaskSRV 포함
		ID3D11ShaderResourceView* SceneColorSRV, // Ping-Pong Source (Color)
		ID3D11RenderTargetView* OutputRTV      // Ping-Pong Dest
	) override;


};
#pragma once

#include "Render/PostProcess/PostProcessBase.h"

class FDepthScenePostProcess : public IPostProcess
{
  public:
	EBlendState GetBlendState() const override { return EBlendState::Opaque; }

    bool IsEnabled(const FPostProcessViewDesc& ViewDesc) const override;

    void Execute(FD3DDevice* Device, ID3D11DeviceContext* Context, const FPostProcessViewDesc& ViewDesc, FRenderResources& Resources,
                 const FRenderTargetSet& RenderTargets, ID3D11ShaderResourceView* SceneColorSRV,
                 ID3D11RenderTargetView* OutputRTV) override;
};
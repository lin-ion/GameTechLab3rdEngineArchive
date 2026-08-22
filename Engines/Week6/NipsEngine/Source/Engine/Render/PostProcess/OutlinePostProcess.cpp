#include "OutlinePostProcess.h"

bool FOutlinePostProcess::IsEnabled(const FPostProcessViewDesc& ViewDesc) const
{
	return ViewDesc.Outline.bEnabled;
}

void FOutlinePostProcess::Execute(FD3DDevice* Device, ID3D11DeviceContext* Context, const FPostProcessViewDesc& ViewDesc, FRenderResources& Resources, const FRenderTargetSet& RenderTargets, ID3D11ShaderResourceView* SceneColorSRV, ID3D11RenderTargetView* OutputRTV)
{
	(void)SceneColorSRV;

	if (Context == nullptr || OutputRTV == nullptr || RenderTargets.SelectionMaskSRV == nullptr)
	{
		return;
	}

	FOutlineConstants OutlineConstants = ViewDesc.Outline.Constants;
	OutlineConstants.ViewportSize = FVector2(RenderTargets.Width, RenderTargets.Height);

	Context->OMSetRenderTargets(1, &OutputRTV, nullptr);
	Context->OMSetDepthStencilState(nullptr, 0);

	Resources.OutlineShader.Bind(Context);
	Resources.OutlineConstantBuffer.Update(Context, &OutlineConstants, sizeof(FOutlineConstants));

	ID3D11Buffer* ConstantBuffer = Resources.OutlineConstantBuffer.GetBuffer();
	Context->VSSetConstantBuffers(5, 1, &ConstantBuffer);
	Context->PSSetConstantBuffers(5, 1, &ConstantBuffer);

	ID3D11ShaderResourceView* MaskSRV = RenderTargets.SelectionMaskSRV;
	Context->PSSetShaderResources(7, 1, &MaskSRV);

	Context->Draw(3, 0);

	ID3D11ShaderResourceView* NullSRV = nullptr;
	Context->PSSetShaderResources(7, 1, &NullSRV);
}

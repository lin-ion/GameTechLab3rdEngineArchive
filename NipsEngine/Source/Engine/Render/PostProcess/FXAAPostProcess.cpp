#include "FXAAPostProcess.h"
#include "Settings/EditorSettings.h"

bool FFXAAPostProcess::IsEnabled(const FPostProcessViewDesc& ViewDesc) const
{
	return ViewDesc.ShowFlags.bEnableFXAA;
}

void FFXAAPostProcess::Execute(FD3DDevice* Device, ID3D11DeviceContext* Context, const FPostProcessViewDesc& ViewDesc, FRenderResources& Resources, const FRenderTargetSet& RenderTargets, ID3D11ShaderResourceView* SceneColorSRV, ID3D11RenderTargetView* OutputRTV)
{
	(void)Device;
	(void)RenderTargets;

	if (ViewDesc.Width <= 0 || ViewDesc.Height <= 0)
		return;
	if (!Context || !SceneColorSRV || !OutputRTV)
		return;

	// Viewport가 1개의 texture를 공유한다.
	// 이번에 rendering할 viewport texture의 uv 정보를 constant Buffer에 담아서 보내야한다.
	// Viewport 정보는 공용 ViewportInfoBuffer(b12)에서 받는다.
	// 이번에 rendering할 viewport texture의 uv 정보는 renderer가 따로 넘겨준다.
	FFxaaConstantBuffer FXAAConstants = {};

	// RenderTarget Size의 역수 (UV 정규화 목적)
	// 해당 뷰포트의 Texture내의 SubUV 설정
	// 위 정보는 현재 공용 ViewportInfoBuffer에서 관리한다.

	// 기존에 저장된 Setting값 기반으로 상수버퍼 채우기
	const FEditorSettings& Settings = FEditorSettings::Get();
	FXAAConstants.EdgeThreshold = Settings.FXAA.EdgeThreshold;
	FXAAConstants.EdgeThresholdMin = Settings.FXAA.EdgeThresholdMin;
	FXAAConstants.Subpix = Settings.FXAA.Subpix;

	Context->OMSetRenderTargets(1, &OutputRTV, nullptr);
	Context->OMSetDepthStencilState(nullptr, 0);

	Resources.FxaaShader.Bind(Context);
	Resources.FxaaConstantBuffer.Update(Context, &FXAAConstants, sizeof(FFxaaConstantBuffer));

	// 상수버퍼 VS, PS에 연결
	ID3D11Buffer* CB = Resources.FxaaConstantBuffer.GetBuffer();
	// Constant Buffer 7번 슬롯 사용 (8번은 DephtScene, 9번은 Decal이 사용)
	Context->VSSetConstantBuffers(7, 1, &CB);
	Context->PSSetConstantBuffers(7, 1, &CB);

	// Source SRV 연결 + LInear Sampler 연결
	ID3D11SamplerState* Sampler = Resources.LinearSamplerState.Get();
	Context->PSSetShaderResources(0, 1, &SceneColorSRV);
	Context->PSSetSamplers(0, 1, &Sampler);

	// Draw 호출
	Context->Draw(3, 0);

	// NullSRV로 바인딩 해제.
	ID3D11ShaderResourceView* NullSRV = nullptr;
	Context->PSSetShaderResources(0, 1, &NullSRV);
}

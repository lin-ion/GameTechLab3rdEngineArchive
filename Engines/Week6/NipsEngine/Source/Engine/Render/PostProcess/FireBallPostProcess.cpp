#include "FireBallPostProcess.h"

#include <algorithm>

bool UFireBallPostProcess::IsEnabled(const FPostProcessViewDesc& ViewDesc) const
{
	return ViewDesc.ViewMode != EViewMode::DepthScene && !ViewDesc.FireBallInfoArray.empty();
}

void UFireBallPostProcess::Execute(
	FD3DDevice* Device,
	ID3D11DeviceContext* Context,
	const FPostProcessViewDesc& ViewDesc,
	FRenderResources& Resources,
	const FRenderTargetSet& RenderTargets,
	ID3D11ShaderResourceView* SceneColorSRV,
	ID3D11RenderTargetView* OutputRTV)
{
	(void)Device;

	// ① Output RTV 바인딩 (Depth 없이 Color만)
	Context->OMSetRenderTargets(1, &OutputRTV, nullptr);

	// ② InvViewProj 계산
	FMatrix ViewProj = ViewDesc.View * ViewDesc.Proj;
	FMatrix InvViewProj = ViewProj.GetInverse();

	// ③ FFireBallCBuffer 채우기
	FFireBallCBuffer CBufferData = {};
	CBufferData.InvViewProj = InvViewProj;

	const uint32 FireBallCount = static_cast<uint32>(std::min<size_t>(ViewDesc.FireBallInfoArray.size(), MAX_FIREBALL_COUNT));
	for (uint32 i = 0; i < FireBallCount; ++i)
	{
		const FFireBallInfo& Info = ViewDesc.FireBallInfoArray[i];
		CBufferData.FireBalls[i].WorldLocation = Info.GetWorldLocation();
		CBufferData.FireBalls[i].LinearColor = Info.GetLinearColor().ToVector4();
		CBufferData.FireBalls[i].Intensity = Info.GetIntensity();
		CBufferData.FireBalls[i].Radius = Info.GetRadius();
		CBufferData.FireBalls[i].RadiusFalloff = Info.GetRadiusFalloff();
		CBufferData.FireBalls[i]._padding = 0.0f;
	}
	CBufferData.FireBallCount = FireBallCount;

	// ④ 상수버퍼 업데이트 및 바인딩 (b10)
	Resources.FireBallConstantBuffer.Update(Context, &CBufferData, sizeof(FFireBallCBuffer));
	ID3D11Buffer* CB = Resources.FireBallConstantBuffer.GetBuffer();
	Context->PSSetConstantBuffers(10, 1, &CB);

	// ⑤ 셰이더 바인딩
	Resources.FireBallShader.Bind(Context);

	// ⑥ SRV 바인딩
	//    t0 : SceneColor (Ping-Pong Source)
	//    t1 : Depth
	ID3D11ShaderResourceView* SRVs[2] = { SceneColorSRV, RenderTargets.DepthSRV };
	Context->PSSetShaderResources(0, 2, SRVs);

	// ⑦ Sampler 바인딩 (s0 : Point)
	ID3D11SamplerState* Samplers[1] = { Resources.PointSamplerState.Get() };
	Context->PSSetSamplers(0, 1, Samplers);

	// ⑧ Draw (Vertex Buffer 없이 fullscreen triangle)
	Context->Draw(3, 0);

	// ⑨ SRV 클린업
	ID3D11ShaderResourceView* NullSRVs[2] = { nullptr, nullptr };
	Context->PSSetShaderResources(0, 2, NullSRVs);
}

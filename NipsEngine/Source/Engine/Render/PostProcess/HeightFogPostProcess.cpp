#include "HeightFogPostProcess.h"

bool UHeightFogPostProcess::IsEnabled(const FPostProcessViewDesc& ViewDesc) const
{
	return ViewDesc.ViewMode != EViewMode::DepthScene && ViewDesc.HeightFogInfo.GetFogDensity() > 0.0f and ViewDesc.HeightFogInfo.bIsFog;
}

void UHeightFogPostProcess::Execute(FD3DDevice* Device, ID3D11DeviceContext* Context, const FPostProcessViewDesc& ViewDesc, FRenderResources& Resources,
	const FRenderTargetSet& RenderTargets, ID3D11ShaderResourceView* SceneColorSRV, ID3D11RenderTargetView* OutputRTV)
{
	ID3D11RenderTargetView* RTVs = { OutputRTV };
	Context->OMSetRenderTargets(1, &RTVs, nullptr);

	// ② InvViewProj 계산
	FMatrix InvViewProj = (ViewDesc.View * ViewDesc.Proj).GetInverse();

	// ③ CameraWorldPos 추출 (View 역행렬 Translation)
	FMatrix InvView = ViewDesc.View.GetInverse();
	FVector CameraPos = FVector(InvView.M[3][0], InvView.M[3][1], InvView.M[3][2]);

	// ④ CBuffer 채우기 → b11
	const FHeightFogInfo& Info = ViewDesc.HeightFogInfo;

	FHeightFogCBuffer CBuffer = {};
	CBuffer.WorldPosition = Info.GetWorldPosition();
	CBuffer.FogDensity = Info.GetFogDensity();
	CBuffer.FogHeightFalloff = Info.GetFogHeightFalloff();
	CBuffer.StartDistance = Info.GetStartDistance();
	CBuffer.FogCutoffDistance = Info.GetFogCutoffDistance();
	CBuffer.FogMaxOpacity = Info.GetFogMaxOpacity();
	CBuffer.InscatteringColor = FVector(Info.GetInscatteringColor().r, Info.GetInscatteringColor().g, Info.GetInscatteringColor().b);
	CBuffer.CameraWorldPos = CameraPos;
	CBuffer.InvViewProj = InvViewProj;
	CBuffer.NearPlane = ViewDesc.NearPlane;
	CBuffer.FarPlane = ViewDesc.FarPlane;

	Resources.HeightFogConstantBuffer.Update(Context, &CBuffer ,sizeof(FHeightFogCBuffer));
	ID3D11Buffer* CB = Resources.HeightFogConstantBuffer.GetBuffer();
	Context->PSSetConstantBuffers(11, 1, &CB);

	// ⑤ Shader 바인딩
	Resources.HeightFogShader.Bind(Context);


	// ⑥ SRV 바인딩 (t0=SceneColor, t1=Depth)	
	ID3D11ShaderResourceView* SRVs[] = { SceneColorSRV, RenderTargets.DepthSRV };
	Context->PSSetShaderResources(0, 2, SRVs);

	// ⑦ Sampler 바인딩 (s0=PointSampler)
//    ViewportInfoBuffer(b12)는 호출자(ExecutePostProcessStack)가 이미 바인딩
//    → ClampPostProcessViewportUV() 셰이더에서 바로 사용 가능
	ID3D11SamplerState* SamplerState = Resources.PointSamplerState.Get();
	Context->PSSetSamplers(0, 1, &SamplerState);

	Context->IASetInputLayout(nullptr);
	Context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Context->Draw(3, 0);

	ID3D11ShaderResourceView* NullSRVs[] = { nullptr, nullptr };
	Context->PSSetShaderResources(0, 2, NullSRVs);
}

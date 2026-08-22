// PostProcessDepthScene.cpp
#include "Render/PostProcess/DepthScenePostProcess.h"

bool FDepthScenePostProcess::IsEnabled(const FPostProcessViewDesc& ViewDesc) const
{
    return ViewDesc.ViewMode == EViewMode::DepthScene;
}

void FDepthScenePostProcess::Execute(FD3DDevice* Device, ID3D11DeviceContext* Context, const FPostProcessViewDesc& ViewDesc, FRenderResources& Resources, const FRenderTargetSet& RenderTargets, ID3D11ShaderResourceView* SceneColorSRV, ID3D11RenderTargetView* OutputRTV)
{
	if (RenderTargets.DepthSRV == nullptr)
	{
		return;
	}

	// 1. DSV 언바인딩 + OutputRTV 설정
	Context->OMSetRenderTargets(1, &OutputRTV, nullptr);

	// 2. Depth SRV 바인딩 — t1 슬롯
	//    t0 : SceneColorSRV 예약 (포스트프로세스 컨벤션)
	//    t1 : DepthStencilSRV
	ID3D11ShaderResourceView* DepthSRV = RenderTargets.DepthSRV;
	Context->PSSetShaderResources(1, 1, &DepthSRV);

	// 3. 셰이더 바인딩
	Resources.DepthSceneShader.Bind(Context);

    // 4. Near/Far 상수 업데이트 — cb8 슬롯
    FDepthSceneConstants Constants = {};
    Constants.NearPlane = ViewDesc.NearPlane;
    Constants.FarPlane = ViewDesc.FarPlane;
    Resources.DepthSceneConstantBuffer.Update(Context, &Constants, sizeof(FDepthSceneConstants));

	ID3D11Buffer* CB = Resources.DepthSceneConstantBuffer.GetBuffer();
	Context->PSSetConstantBuffers(8, 1, &CB);

	// 5. 풀스크린 삼각형 드로우
	Context->Draw(3, 0);

	// 6. SRV 클리어 — 바인딩한 t1만 해제
	//    다음 패스에서 같은 Depth 텍스처를 DSV로 복구할 때 Read/Write 충돌 방지
	ID3D11ShaderResourceView* NullSRV = nullptr;
	Context->PSSetShaderResources(1, 1, &NullSRV);
}

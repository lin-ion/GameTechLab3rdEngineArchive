// PostProcessBase.h
#pragma once

#include "Render/Common/RenderTypes.h"
#include "Render/Resource/RenderResources.h"
#include "PostProcessTypes.h"

/*
 * IPostProcess — 포스트프로세스 패스 인터페이스
 *
 * 리소스(셰이더, 상수버퍼)는 FRenderResources가 소유하고 FRenderer::Create/Release에서 관리.
 * 이 인터페이스는 IsEnabled 조건 판단과 Execute 드로우 로직만 담당한다.
 *
 * Execute 규칙:
 *   - OMSetRenderTargets(OutputRTV, DSV=nullptr) 은 구현체가 직접 설정한다.
 *   - SRV는 Draw 이후 반드시 null로 클리어한다. (D3D11 Read/Write 충돌 방지)
 *   - Ping-Pong Swap은 호출자(ExecutePostProcessStack)가 담당한다.
 */
class IPostProcess
{
  public:
    virtual ~IPostProcess() = default;

    virtual EBlendState GetBlendState() const { return EBlendState::Opaque; }

    virtual bool IsEnabled(const FPostProcessViewDesc& ViewDesc) const = 0;

    virtual void Execute(FD3DDevice* Device,
						 ID3D11DeviceContext* Context, 
						 const FPostProcessViewDesc& ViewDesc,
                         FRenderResources&         Resources,
                         const FRenderTargetSet&   RenderTargets, // DepthStencilSRV, SelectionMaskSRV 포함
                         ID3D11ShaderResourceView* SceneColorSRV, // Ping-Pong Source (Color)
                         ID3D11RenderTargetView*   OutputRTV      // Ping-Pong Dest
                         ) = 0;
};

// DepthScene.hlsl

#include "Common.hlsl"

// ============================================================
// Constant Buffers
// ============================================================

// cb8 : DepthScene 전용 상수 (FDepthSceneConstants)
cbuffer DepthSceneConstants : register(b8)
{
    float NearPlane;
    float FarPlane;
    float2 DepthScenePadding0;
};

// ============================================================
// Textures & Samplers
// ============================================================

// t0 : SceneColorSRV — 예약 (Fog 등 컨벤션 통일, DepthScene에서는 미사용)
// t1 : DepthStencilSRV — DXGI_FORMAT_R24_UNORM_X8_TYPELESS
Texture2D<float> DepthTexture : register(t1);

SamplerState PointSampler : register(s0);

// ============================================================
// Vertex Shader
// ============================================================
// 버텍스 버퍼 없이 풀스크린 삼각형을 생성한다.
// Draw(3, 0) 호출 시 SV_VertexID 0~2로 NDC 좌표를 직접 계산.

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
};

VS_OUTPUT VS(uint VertexID : SV_VertexID)
{
    VS_OUTPUT Output;

    // 풀스크린 삼각형 — 3개의 버텍스로 화면 전체를 덮음
    // VertexID: 0 → (-1, -1), 1 → (-1, 3), 2 → (3, -1)
    float2 Position;
    Position.x = (VertexID == 2) ? 3.0f : -1.0f;
    Position.y = (VertexID == 1) ? 3.0f : -1.0f;

    Output.Position = float4(Position, 0.0f, 1.0f);

    // UV: NDC → [0, 1] 변환 (D3D11 텍스처 좌표는 Y축 반전)
    // 현재는 TEXCOORD를 따로 쓰지 않고 공용 helper에서 같은 의미의 UV를 계산한다.
    return Output;
}

// ============================================================
// Helper : NDC Depth → Linear Depth [0, 1]
// ============================================================
// D3D11의 Depth 버퍼는 비선형(NDC) 값으로 저장됨.
// 가까운 거리에서 정밀도가 집중되므로 육안으로 보기 어렵고,
// 선형화하면 Near=흰색(1.0), Far=검정(0.0)으로 거리감이 직관적으로 나타남.

float LinearizeDepth(float ndcDepth)
{
    // D3D11 투영 역변환
    // LinearDepth = Near * Far / (Far - NDCDepth * (Far - Near))
    float linearDepth = (NearPlane * FarPlane) / (FarPlane - ndcDepth * (FarPlane - NearPlane));

    // [Near, Far] 범위를 [0, 1]로 정규화
    // Near에 가까울수록 1.0 (밝음), Far에 가까울수록 0.0 (어두움)
    return frac(( saturate((linearDepth - NearPlane) / max(FarPlane - NearPlane, 1e-6f))) * 50.0f);
    }

// ============================================================
// Pixel Shader
// ============================================================

float4 PS(VS_OUTPUT Input) : SV_TARGET
{
    // UV는 별도 TEXCOORD가 아니라 공용 helper로 계산한다.
    // 멀티 뷰포트에서는 sub-viewport 안에서만 PS가 실행되지만,
    // depth sampling은 전체 render target 기준 UV로 해야 한다.
    float2 uv = ClampPostProcessViewportUV(GetPostProcessFullUV(Input.Position.xy));
    float ndcDepth = DepthTexture.Sample(PointSampler, uv).r;

    // 깊이가 기록되지 않은 픽셀(배경) 처리
    // D3D11 기본 클리어값은 1.0f
    if (ndcDepth >= 1.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float depth = LinearizeDepth(ndcDepth);

    // 흑백 깊이 시각화 — 가까울수록 밝고 멀수록 어두움
    return float4(depth, depth, depth, 1.0f);
}

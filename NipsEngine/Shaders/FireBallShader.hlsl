// FireBallShader.hlsl

#include "Common.hlsl"

// ─────────────────────────────────────────────
// FireBall 전용 상수버퍼 b10
// ─────────────────────────────────────────────
#define MAX_FIREBALL_COUNT 32

struct FFireBallConstants
{
    float4 WorldLocation; // 16 bytes
    float4 LinearColor; // 16 bytes
    float Intensity; //  4 bytes
    float Radius; //  4 bytes
    float RadiusFalloff; //  4 bytes
    float _padding; //  4 bytes
}; // = 48 bytes per entry

cbuffer FireBallBuffer : register(b10)
{
    // depth에서 world position을 복원하기 위한 역행렬
    row_major float4x4 InvViewProj;
    FFireBallConstants FireBalls[MAX_FIREBALL_COUNT];
    int FireBallCount;
    float3 _cbPadding;
};

// ─────────────────────────────────────────────
// Textures
// ─────────────────────────────────────────────
// t0 : SceneColor, t1 : Depth
Texture2D SceneColorTexture : register(t0);
Texture2D DepthTexture : register(t1);
SamplerState PointSampler : register(s0);

// ─────────────────────────────────────────────
// HSV 변환 함수
// ─────────────────────────────────────────────
float3 RGBtoHSV(float3 c)
{
    float4 K = float4(0.0f, -1.0f / 3.0f, 2.0f / 3.0f, -1.0f);
    float4 p = c.g < c.b ? float4(c.bg, K.wz) : float4(c.gb, K.xy);
    float4 q = c.r < p.x ? float4(p.xyw, c.r) : float4(c.r, p.yzx);
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10f;
    return float3(abs(q.z + (q.w - q.y) / (6.0f * d + e)), d / (q.x + e), q.x);
}

float3 HSVtoRGB(float3 c)
{
    float4 K = float4(1.0f, 2.0f / 3.0f, 1.0f / 3.0f, 3.0f);
    float3 p = abs(frac(c.xxx + K.xyz) * 6.0f - K.www);
    return c.z * lerp(K.xxx, saturate(p - K.xxx), c.y);
}

// ─────────────────────────────────────────────
// World Position 복원 함수 (Standard-Z)
// ─────────────────────────────────────────────
// viewportUV 는 현재 서브 뷰포트 기준 로컬 UV 입니다.
// 멀티 뷰포트에서는 전체 RT UV 로 NDC 를 만들면 다른 뷰포트의 오프셋이 섞이므로
// 월드 좌표 복원은 반드시 로컬 UV 를 기준으로 해야 합니다.
float3 ReconstructWorldPosition(float2 viewportUV, float depth)
{
    float4 ndcPos;
    ndcPos.x = viewportUV.x * 2.0f - 1.0f;
    ndcPos.y = viewportUV.y * -2.0f + 1.0f;
    ndcPos.z = depth;
    ndcPos.w = 1.0f;

    float4 worldPos = mul(ndcPos, InvViewProj);
    worldPos /= worldPos.w;

    return worldPos.xyz;
}

// ─────────────────────────────────────────────
// Vertex Shader
// ─────────────────────────────────────────────
struct VSOutput
{
    float4 Position : SV_Position;
};

VSOutput VS_Main(uint VertexID : SV_VertexID)
{
    VSOutput Output;

    // 버텍스 버퍼 없이 풀스크린 삼각형 생성
    float2 uv = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
   
    
}

// ─────────────────────────────────────────────
// Pixel Shader
// ─────────────────────────────────────────────
float4 PS_Main(VSOutput Input) : SV_Target
{
    
    // 멀티 뷰포트에서는 반드시 전체 render target 기준 UV로 depth/scene을 읽어야 한다.
    // scene/depth 텍스처는 전체 렌더 타겟 기준으로 저장되어 있으므로
    // 실제 샘플링은 full UV 로 해야 합니다.
    float2 uv = ClampPostProcessViewportUV(GetPostProcessFullUV(Input.Position.xy));

    // 반대로 InvViewProj 로 world position 을 복원할 때는
    // 현재 서브 뷰포트 내부의 로컬 UV 를 사용해야 좌표계가 맞습니다.
    float2 viewportUV = saturate(GetPostProcessViewportUV(Input.Position.xy));

    // 1. Depth 샘플링
    float depth = DepthTexture.Sample(PointSampler, uv).r;

    // Sky / Empty 픽셀 스킵
    if (depth >= 1.0f)
    {
        return SceneColorTexture.Sample(PointSampler, uv);
    }

    // 2. World Position 복원
    float3 worldPos = ReconstructWorldPosition(viewportUV, depth);

    // 3. SceneColor 샘플링
    float4 sceneColor = SceneColorTexture.Sample(PointSampler, uv);

    // 4. 모든 FireBall 누적
    for (int i = 0; i < FireBallCount; ++i)
    {
        float3 center = FireBalls[i].WorldLocation.xyz;
        float dist = distance(worldPos, center);
        float radius = FireBalls[i].Radius;
        float radiusFalloff = FireBalls[i].RadiusFalloff;

        if (dist > radiusFalloff)
            continue;

        float weight = 1.0f - smoothstep(radius, radiusFalloff, dist);
        weight = pow(weight, 1.5f);
        float blendWeight = saturate(weight * FireBalls[i].Intensity * 0.25f); // 강도 조절
        float3 lightColor = FireBalls[i].LinearColor.rgb;

        // HSV 공간에서 H, S만 FireBall 색으로 이동, V는 원본 유지
        float3 sceneHSV = RGBtoHSV(sceneColor.rgb);
        float3 lightHSV = RGBtoHSV(lightColor);

        float3 blendedHSV;
        blendedHSV.x = lerp(sceneHSV.x, lightHSV.x, blendWeight); // H: 색조
        blendedHSV.y = lerp(sceneHSV.y, lightHSV.y, blendWeight); // S: 채도
        blendedHSV.z = sceneHSV.z + (lightHSV.z * blendWeight);; // 밝기 누산 (현재)
       // blendedHSV.z = lerp(sceneHSV.z, lightHSV.z, weight); // 밝기 lerp
        float3 blendedRGB = HSVtoRGB(blendedHSV);


//        sceneColor.rgb = lerp(sceneColor.rgb, lightColor, blendWeight);
        sceneColor.rgb = blendedRGB; // 또는 lerp(sceneColor.rgb, blendedRGB, blendWeight)

        sceneColor.rgb = lerp(sceneColor.rgb, lightColor, blendWeight);
    }

    return sceneColor;
}

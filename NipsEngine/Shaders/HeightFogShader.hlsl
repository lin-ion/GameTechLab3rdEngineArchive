#include "Common.hlsl"  // ViewportInfoBuffer(b12), ClampPostProcessViewportUV, GetPostProcessFullUV

// ----------------------------------------------------------------
// Textures & Samplers
// t0 : SceneColor   (Ping-Pong Source)
// t1 : DepthBuffer  (RenderTargets.DepthSRV)
// ----------------------------------------------------------------
Texture2D SceneColorTex : register(t0);
Texture2D DepthTex : register(t1);
SamplerState PointSampler : register(s0);

// ----------------------------------------------------------------
// HeightFog Constant Buffer  b11
// C++ FHeightFogCBuffer 레이아웃과 1:1 매칭
//
// float3 WorldPosition    + float FogDensity        → 16 bytes
// float  FogHeightFalloff + float StartDistance
//   + float FogCutoffDistance + float FogMaxOpacity → 16 bytes
// float3 InscatteringColor + float _Pad0            → 16 bytes
// float3 CameraWorldPos    + float _Pad1            → 16 bytes
// float4x4 InvViewProj                              → 64 bytes
// Total : 128 bytes
// ---------------------------------------------------------------- 
cbuffer HeightFogBuffer : register(b11)
{
    float3 FogWorldPosition; // 컴포넌트 높이 기준점
    float FogDensity;

    float FogHeightFalloff;
    float StartDistance;
    float FogCutoffDistance;
    float FogMaxOpacity;

    float3 InscatteringColor;
    float NearPlane;

    float3 CameraWorldPos;
    float FarPlane;

    row_major float4x4 InvViewProj;
}

// ----------------------------------------------------------------
// Fullscreen Triangle  VS
// VB 없이 SV_VertexID 만으로 NDC[-1,1] 삼각형을 생성합니다.
//   ID=0 → (-1, -1)   좌하
//   ID=1 → (-1,  3)   좌상(클립 밖)
//   ID=2 → ( 3, -1)   우하(클립 밖)
// ----------------------------------------------------------------
struct FVSOutput
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

FVSOutput VS_Main(uint VertexID : SV_VertexID)
{
    FVSOutput Output;

    float2 NDC;
    NDC.x = (VertexID == 2) ? 3.0f : -1.0f;
    NDC.y = (VertexID == 1) ? 3.0f : -1.0f;

    Output.Position = float4(NDC, 0.0f, 1.0f);

    // NDC → UV  (y 반전)
    Output.UV = NDC * float2(0.5f, -0.5f) + 0.5f;

    return Output;
}

// ----------------------------------------------------------------
// World Position 복원
// NDC depth(0~1) + InvViewProj → World Space Position
// ----------------------------------------------------------------
// HeightFog 도 world reconstruction 과정은 FireBall 과 동일합니다.
// depth 는 전체 RT 에서 읽되, InvViewProj 에 넣는 UV 는 현재 서브 뷰포트 기준이어야
// 멀티 뷰포트에서 올바른 월드 좌표가 복원됩니다.
float3 ReconstructWorldPos(float2 ViewportUV, float Depth)
{
    // UV → NDC
    float2 NDC = ViewportUV * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);

    float4 ClipPos = float4(NDC, Depth, 1.0f);
    float4 WorldPos = mul(ClipPos, InvViewProj);
    return WorldPos.xyz / WorldPos.w;
}

// ----------------------------------------------------------------
// Exponential Height Fog — 해석적 경로 적분 (Analytic Path Integral)
//
// 밀도 함수:
//   density(h) = FogDensity * exp( -FogHeightFalloff * max(h - FogZ, 0) )
//
// 카메라(C)에서 픽셀(P)까지 광선 경로를 따라 밀도를 적분:
//   Integral = ∫₀ᴸ density( CamZ + DeltaZ * t/L ) dt
//
//   CamDensity = FogDensity * exp( -FogHeightFalloff * max(CamZ - FogZ, 0) )
//
//   ① DeltaHeight ≠ 0  (비수평 광선):
//      Integral = CamDensity * NormalizedLength
//                 * (1 - exp(-FogHeightFalloff * DeltaHeight))
//                 / (FogHeightFalloff * DeltaHeight)
//
//   ② DeltaHeight ≈ 0  (수평 광선, 극한 lim→1):
//      Integral = CamDensity * NormalizedLength
//
//   NormalizedLength = EffectiveLength / FarPlane
//   → 씬 스케일에 독립적으로 FogDensity 가 동작합니다.
//   → FogDensity 0.2 = "FarPlane 거리의 20% 수준에서 안개 체감"
//
//   FogFactor = clamp( 1 - exp(-Integral), 0, FogMaxOpacity )
// ----------------------------------------------------------------
float ComputeExponentialHeightFog(float3 WorldPos)
{
    float3 RayVec = WorldPos - CameraWorldPos;
    float RayLength = length(RayVec);

    // StartDistance 이전 구간 제거
    float EffectiveLength = max(RayLength - StartDistance, 0.0f);
    if (EffectiveLength <= 0.0f)
        return 0.0f;

    // FogCutoffDistance 적용
    if (FogCutoffDistance > 0.0f)
        EffectiveLength = min(EffectiveLength, FogCutoffDistance);

    // [수정 1] 로그 정규화 제거
    // 이제 씬 스케일(World Units)에 비례하여 거리가 그대로 누적됩니다.
    // (FarPlane 정규화 코드를 삭제하고 EffectiveLength를 직접 사용)

    // 카메라 위치에서의 시작 밀도
    //float CamHeightAbove = max(CameraWorldPos.z - FogWorldPosition.z, 0.0f);
    float CamHeightAbove = (CameraWorldPos.z - FogWorldPosition.z);
    float CamDensity = FogDensity * exp(-FogHeightFalloff * CamHeightAbove);

    // [수정 2] abs() 제거하여 시선 방향(위/아래)에 따른 안개 누적 부호 유지
    float LengthRatio = EffectiveLength / max(RayLength, 0.0001f);
    float DeltaHeight = RayVec.z * LengthRatio; // abs() 제거됨

    float Integral;
    // 분모 0 회피를 위한 체크는 여전히 절대값으로 확인
    if (abs(DeltaHeight) < 0.0001f)
    {
        // 수평 광선
        Integral = CamDensity * EffectiveLength;
    }
    else
    {
        // 비수평 광선 (위치 에너지가 델타 H에 따라 지수적으로 변화)
        Integral = CamDensity * EffectiveLength
                 * (1.0f - exp(-FogHeightFalloff * DeltaHeight))
                 / (FogHeightFalloff * DeltaHeight);
    }

    float Extinction = 1.0f - exp(-Integral);
    return clamp(Extinction, 0.0f, FogMaxOpacity);
}

// ----------------------------------------------------------------
// Pixel Shader
// ----------------------------------------------------------------
float4 PS_Main(FVSOutput Input) : SV_Target
{
    float2 UV = GetPostProcessFullUV(Input.Position.xy);
    UV = ClampPostProcessViewportUV(UV);
    float2 ViewportUV = saturate(GetPostProcessViewportUV(Input.Position.xy));

    float4 SceneColor = SceneColorTex.Sample(PointSampler, UV);
    float Depth = DepthTex.Sample(PointSampler, UV).r;

    // 스카이박스(Depth == 1.0) → 안개 미적용
    if (Depth >= 1.1f)
       return SceneColor;

    float3 WorldPos = ReconstructWorldPos(ViewportUV, Depth);
    float FogFactor = ComputeExponentialHeightFog(WorldPos);

    // 밝기 기반 Scattered 억제 — 밝은 픽셀이 더 밝아지는 현상 방지
    float Luminance = dot(SceneColor.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    float BrightnessSuppression = 1.0f - saturate(Luminance);

    float3 Absorbed = SceneColor.rgb * (1.0f - FogFactor);
    float3 Scattered = InscatteringColor * FogFactor * BrightnessSuppression;
    float3 FoggedColor = Absorbed + Scattered;

    return float4(FoggedColor, SceneColor.a);
}
#include "Common.hlsl"

cbuffer DecalBuffer : register(b9)
{
    row_major float4x4 DecalViewProjection;
    float3 DecalForward;
    float FadeAlpha;
};

Texture2D DecalTexture : register(t4);

SamplerState DecalSampler : register(s1);

struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION; // ClipPos
    float3 worldPos : TEXCOORD0; // WorldPos
    float3 worldNormal : TEXCOORD1; // WorldNormal
};

PSInput VS(VSInput input)
{
    PSInput output;

    output.position = ApplyMVP(input.position);
    output.worldPos = mul(float4(input.position, 1.0f), Model).xyz;
    // TODO: Normal Matrix 확인 필요
    output.worldNormal = normalize(mul(input.normal, (float3x3)Model));

    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    // 월드 좌표를 데칼의 NDC 좌표로 변환
    float4 decalNDC = mul(float4(input.worldPos, 1.0f), DecalViewProjection);
    decalNDC.xyz /= decalNDC.w;

    if (any(abs(decalNDC.xyz) > 1.0f) || decalNDC.z < 0.0f)
    {
        discard;
    }

    float2 decalUV = decalNDC.xy * 0.5f + 0.5f; // NDC를 UV로 변환
    decalUV.y = 1.0f - decalUV.y; // Texture 좌표계는 Y축이 반대

    float4 decalColor = DecalTexture.Sample(DecalSampler, decalUV);

    if (decalColor.a <= 0.01f)
    {
        discard;
    }

    decalColor.a *= FadeAlpha;

    return decalColor;
}

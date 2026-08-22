#include "Common.hlsl"

Texture2D BillboardTexture : register(t0);
SamplerState BillboardSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float4 color    : COLOR;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

PSInput VS(VSInput input)
{
    PSInput output;
    output.position = ApplyMVP(input.position);
    output.texCoord = input.texCoord;
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    float4 col = BillboardTexture.Sample(BillboardSampler, input.texCoord);
    if (col.a < 0.01f)
    {
        discard;
    }

    if (bIsWireframe < 0.5f)
    {
        return col;
    }

    return lerp(col, float4(WireframeRGB, 1.0f), bIsWireframe);
}

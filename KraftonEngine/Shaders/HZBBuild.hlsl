// HZBBuild.hlsl
Texture2D<float> InTexture : register(t0);
RWTexture2D<float> OutTexture : register(u0);

cbuffer HZBConstants : register(b0)
{
    uint2 SrcResolution;
    uint2 DstResolution;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= DstResolution.x || DTid.y >= DstResolution.y)
        return;

    uint2 srcCoord0 = DTid.xy * 2;
    uint2 srcCoord1 = min(srcCoord0 + uint2(1, 0), SrcResolution - 1);
    uint2 srcCoord2 = min(srcCoord0 + uint2(0, 1), SrcResolution - 1);
    uint2 srcCoord3 = min(srcCoord0 + uint2(1, 1), SrcResolution - 1);
    
    float d0 = InTexture.Load(uint3(srcCoord0, 0)).r;
    float d1 = InTexture.Load(uint3(srcCoord1, 0)).r;
    float d2 = InTexture.Load(uint3(srcCoord2, 0)).r;
    float d3 = InTexture.Load(uint3(srcCoord3, 0)).r;
    
    // In Reverse-Z, min is furthest. We want the furthest point of the 4 samples.
    float minDepth = min(min(d0, d1), min(d2, d3));
    
    OutTexture[DTid.xy] = minDepth;
}

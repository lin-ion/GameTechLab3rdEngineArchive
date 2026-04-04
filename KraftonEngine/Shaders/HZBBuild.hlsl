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

    uint2 srcCoord = DTid.xy * 2;
    
    float4 depths;
    depths.x = InTexture.Load(uint3(srcCoord, 0)).r;
    depths.y = InTexture.Load(uint3(srcCoord + uint2(1, 0), 0)).r;
    depths.z = InTexture.Load(uint3(srcCoord + uint2(0, 1), 0)).r;
    depths.w = InTexture.Load(uint3(srcCoord + uint2(1, 1), 0)).r;
    
    // Boundary check for non-power-of-two textures
    if (srcCoord.x + 1 >= SrcResolution.x)
    {
        depths.y = depths.x;
        depths.w = depths.z;
    }
    if (srcCoord.y + 1 >= SrcResolution.y)
    {
        depths.z = depths.x;
        depths.w = depths.y;
    }

    float minDepth = min(min(depths.x, depths.y), min(depths.z, depths.w));
    
    OutTexture[DTid.xy] = minDepth;
}

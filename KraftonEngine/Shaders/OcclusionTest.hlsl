// OcclusionTest.hlsl
Texture2D<float> HZB : register(t0);
SamplerState PointClampSampler : register(s0);

struct ProxyAABB
{
    float3 Min;
    uint Id;
    float3 Max;
    uint Padding;
};

StructuredBuffer<ProxyAABB> InProxies : register(t1);
RWStructuredBuffer<uint> OutVisibility : register(u0);

cbuffer PassConstants : register(b0)
{
    float4x4 ViewProjection;
    uint ProxyCount;
    uint HZBMipCount;
    float2 HZBSize;
};

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    if (DTid.x >= ProxyCount) return;

    ProxyAABB proxy = InProxies[DTid.x];
    
    // Project AABB to NDC
    float3 corners[8] = {
        float3(proxy.Min.x, proxy.Min.y, proxy.Min.z),
        float3(proxy.Max.x, proxy.Min.y, proxy.Min.z),
        float3(proxy.Min.x, proxy.Max.y, proxy.Min.z),
        float3(proxy.Max.x, proxy.Max.y, proxy.Min.z),
        float3(proxy.Min.x, proxy.Min.y, proxy.Max.z),
        float3(proxy.Max.x, proxy.Min.y, proxy.Max.z),
        float3(proxy.Min.x, proxy.Max.y, proxy.Max.z),
        float3(proxy.Max.x, proxy.Max.y, proxy.Max.z)
    };

    float3 minNDC = float3(1, 1, 1);
    float3 maxNDC = float3(-1, -1, -1);

    bool anyBehindNearPlane = false;

    [unroll]
    for (int i = 0; i < 8; ++i)
    {
        float4 clip = mul(float4(corners[i], 1.0), ViewProjection);
        
        // If any part is behind or very close to near plane, consider it visible
        if (clip.w <= 0.0001)
        {
            anyBehindNearPlane = true;
            break;
        }

        float3 ndc = clip.xyz / clip.w;
        minNDC = min(minNDC, ndc);
        maxNDC = max(maxNDC, ndc);
    }

    if (anyBehindNearPlane)
    {
        OutVisibility[DTid.x] = 1;
        return;
    }

    // Clipping check - if it's completely outside the view, we can cull it
    // But frustum culling already does this. Let's be conservative.
    if (maxNDC.x < -1.0 || minNDC.x > 1.0 || maxNDC.y < -1.0 || minNDC.y > 1.0 || maxNDC.z < 0.0)
    {
        OutVisibility[DTid.x] = 0;
        return;
    }

    // Clamp NDC to view range
    minNDC.xy = max(minNDC.xy, float2(-1, -1));
    maxNDC.xy = min(maxNDC.xy, float2(1, 1));

    // Convert to [0, 1] UV
    float2 minUV = minNDC.xy * float2(0.5, -0.5) + 0.5;
    float2 maxUV = maxNDC.xy * float2(0.5, -0.5) + 0.5;
    
    // Swap if needed
    if (minUV.x > maxUV.x) { float t = minUV.x; minUV.x = maxUV.x; maxUV.x = t; }
    if (minUV.y > maxUV.y) { float t = minUV.y; minUV.y = maxUV.y; maxUV.y = t; }
    
    // UV inset trick
    float2 invSize = 1.0f / HZBSize;
    float2 inset = invSize * 1.5f; // 1.5픽셀 정도만 안쪽으로
    minUV = min(minUV + inset, maxUV);
    maxUV = max(maxUV - inset, minUV);

    // Calculate footprint size in pixels (mip 0 resolution)
    float2 size = (maxUV - minUV) * HZBSize;
    float maxSide = max(size.x, size.y);
    
    // floor(log2) covers the footprint with 4 samples in that mip
    float mip = floor(log2(maxSide + 0.01f));   // 0.01 더해서 Mip 경계에서 튀는 거 방지
    mip = clamp(mip + 1.0, 0, (float)HZBMipCount - 1.0);
    
    float2 midUV = (minUV + maxUV) * 0.5f;

    // 9-tap HZB test
    // Samples the min depth (furthest point) in the HZB region.
    float d0 = HZB.SampleLevel(PointClampSampler, float2(minUV.x, minUV.y), mip).r;
    float d1 = HZB.SampleLevel(PointClampSampler, float2(midUV.x, minUV.y), mip).r;
    float d2 = HZB.SampleLevel(PointClampSampler, float2(maxUV.x, minUV.y), mip).r;
    float d3 = HZB.SampleLevel(PointClampSampler, float2(minUV.x, midUV.y), mip).r;
    float d4 = HZB.SampleLevel(PointClampSampler, float2(midUV.x, midUV.y), mip).r;
    float d5 = HZB.SampleLevel(PointClampSampler, float2(maxUV.x, midUV.y), mip).r;
    float d6 = HZB.SampleLevel(PointClampSampler, float2(minUV.x, maxUV.y), mip).r;
    float d7 = HZB.SampleLevel(PointClampSampler, float2(midUV.x, maxUV.y), mip).r;
    float d8 = HZB.SampleLevel(PointClampSampler, float2(maxUV.x, maxUV.y), mip).r;

    float minH = min(min(min(min(d0, d1), min(d2, d3)), min(min(d4, d5), min(d6, d7))), d8);
    
    float nearD = maxNDC.z;
    
    // 두 깊이가 너무 비슷하면(0.001 이내) "이전 프레임의 나"라고 판단하여 통과
    uint isVisible = 0;
    float dynamicEpsilon = 0.02f * (1.0f - nearD);
    if (abs(nearD - minH) < dynamicEpsilon) 
    {
        isVisible = 1;
    }
    else 
    {
        // 일반적인 Reverse-Z 판정
        const float EPSILON = 0.0001f; 
        isVisible = (nearD >= minH - EPSILON) ? 1 : 0;
    }
    
    OutVisibility[DTid.x] = isVisible;
}

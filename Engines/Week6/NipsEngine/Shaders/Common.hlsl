/* Constant Buffers */

cbuffer FrameBuffer : register(b0)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    float bIsWireframe;
    float3 WireframeRGB;
}

cbuffer PerObjectBuffer : register(b1)
{
    row_major float4x4 Model;
    float4 PrimitiveColor;
};

cbuffer GizmoBuffer : register(b2)
{
    float4 GizmoColorTint;
    uint bIsInnerGizmo;
    uint bClicking;
    uint SelectedAxis;
    float HoveredAxisOpacity;
};


cbuffer OverlayBuffer : register(b3)
{
    float2 OverlayCenterScreen;
    float2 ViewportSize;

    float OverlayRadius;
    float3 Padding2;

    float4 OverlayColor;
};

cbuffer EditorBuffer : register(b4)
{
    float4 CameraPosition;
    uint EditorFlag;
    float3 Padding3;
};

cbuffer OutlineConstants : register(b5)
{
    float4 OutlineColor;
    float OutlineThicknessPixels;
    float2 OutlineViewportSize;
    float OutlinePadding0;
};

// cbuffer StaticMeshBuffer : register(b6)
// cbuffer DecalBuffer : register(b9)
// cbuffer ViewportInfoBuffer : register(b12)

// 멀티 뷰포트 post process에서 공통으로 사용하는 viewport 정보.
// sampling은 전체 render target 기준으로 하되, 현재 viewport 경계를 알기 위해
// 원점/크기/역해상도를 함께 넘긴다.
cbuffer ViewportInfoBuffer : register(b12)
{
    float2 InvFullRenderTargetSize;
    float2 ViewportOriginPixels;

    float2 ViewportSizePixels;
    float2 ViewportInfoPadding0;
};

float4 ApplyMVP(float3 pos)
{
    float4 world = mul(float4(pos, 1.0f), Model);
    float4 view = mul(world, View);
    return mul(view, Projection);
}

float3x3 Inverse3x3(float3x3 m)
{
    float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1])
              - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
              + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

    float invDet = 1.0 / det;

    float3x3 result;
    result[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) * invDet;
    result[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]) * invDet;
    result[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invDet;
    result[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]) * invDet;
    result[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invDet;
    result[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]) * invDet;
    result[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) * invDet;
    result[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]) * invDet;
    result[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * invDet;
    return result;
}

float2 GetPostProcessFullUV(float2 svPosition)
{
    return svPosition * InvFullRenderTargetSize;
}

// 현재 픽셀이 "전체 렌더 타겟" 안에서 어디에 있는지가 아니라,
// "현재 서브 뷰포트 안에서" 몇 픽셀 위치인지 구합니다.
// 멀티 뷰포트에서는 SV_Position 이 항상 전체 RT 기준으로 들어오므로
// 월드 복원용 좌표를 만들 때는 뷰포트 원점을 먼저 빼줘야 합니다.
float2 GetPostProcessViewportPixelCoord(float2 svPosition)
{
    return svPosition - ViewportOriginPixels;
}

// 현재 서브 뷰포트 기준의 로컬 UV [0,1] 입니다.
// depth/scene 샘플링은 여전히 전체 RT UV 를 써야 하지만,
// InvViewProj 로 월드 좌표를 복원할 때는 이 로컬 UV 를 써야 합니다.
float2 GetPostProcessViewportUV(float2 svPosition)
{
    float2 safeViewportSize = max(ViewportSizePixels, float2(1.0f, 1.0f));
    return GetPostProcessViewportPixelCoord(svPosition) / safeViewportSize;
}

// 뷰포트 로컬 UV 를 바로 NDC 로 바꾸는 헬퍼입니다.
// post process 에서 depth 기반 world reconstruction 할 때 사용합니다.
float2 GetPostProcessViewportNDC(float2 svPosition)
{
    float2 viewportUV = GetPostProcessViewportUV(svPosition);
    return float2(viewportUV.x * 2.0f - 1.0f, viewportUV.y * -2.0f + 1.0f);
}

float2 GetPostProcessViewportMinUV()
{
    return ViewportOriginPixels * InvFullRenderTargetSize;
}

float2 GetPostProcessViewportMaxUV()
{
    return (ViewportOriginPixels + ViewportSizePixels) * InvFullRenderTargetSize;
}

float2 ClampPostProcessViewportUV(float2 uv)
{
    float2 viewportMinUV = GetPostProcessViewportMinUV();
    float2 viewportMaxUV = GetPostProcessViewportMaxUV();
    // Neighbor sample이 다른 viewport를 읽지 않도록 half pixel margin으로 clamp한다.
    float2 halfPixel = InvFullRenderTargetSize * 0.5f;
    return clamp(uv, viewportMinUV + halfPixel, viewportMaxUV - halfPixel);
}

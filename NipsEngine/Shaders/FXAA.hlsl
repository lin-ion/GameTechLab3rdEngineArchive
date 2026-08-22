/*
 *  Fast-Approximate Anti-Aliasing
 */
#include "Common.hlsl"

Texture2D Scene : register(t0); // Scene의 RTV를 SRV로 받음.
SamplerState LinearSampler : register(s0); // 선형 샘플러

cbuffer FXAACB : register(b7)
{
    float EdgeThreshold; // 예: 1.0 / 8.0
    float EdgeThresholdMin; // 예: 1.0 / 16.0
    float Subpix; // 예: 0.75
    float Padding0;
};

struct PSInput
{
    float4 position : SV_Position;
};

float Luma1(float3 rgb)
{
    // 단순 구현, 표준 계수 사용
    return dot(rgb, float3(0.299, 0.587, 0.114));
}

float Luma2(float3 rgb)
{
    // Luma 계산 최적화
    return rgb.y * (0.587 / 0.299) + rgb.x;
}

float3 SampleScene(float2 uv)
{
    // 멀티 뷰포트에서 옆 viewport를 샘플하지 않도록 현재 viewport 안으로 clamp한다.
    return Scene.Sample(LinearSampler, ClampPostProcessViewportUV(uv)).rgb;
}

PSInput VS(uint vertexId : SV_VertexID)
{
    PSInput output;

    // 버텍스 버퍼 없이 풀스크린 삼각형 생성
    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };

    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    return output;
}

float4 PS(PSInput input) : SV_TARGET
{
    // SV_Position을 전체 render target 기준 UV로 변환한다.
    float2 uv = GetPostProcessFullUV(input.position.xy);

    // UV에서 1 픽셀의 크기
    float2 px = InvFullRenderTargetSize;

    // 현재 픽셀(rgbM)과 상하좌우(N/S/E/W)를 읽는다.
    float3 rgbM = SampleScene(uv);
    float3 rgbN = SampleScene(uv + float2(0.0f, -px.y));
    float3 rgbS = SampleScene(uv + float2(0.0f, px.y));
    float3 rgbW = SampleScene(uv + float2(-px.x, 0.0f));
    float3 rgbE = SampleScene(uv + float2(px.x, 0.0f));

    //RGB -> LUma로 변경
    float lumaM = Luma2(rgbM);
    float lumaN = Luma2(rgbN);
    float lumaS = Luma2(rgbS);
    float lumaE = Luma2(rgbE);
    float lumaW = Luma2(rgbW);
    
    float rangeMin = min(lumaM, min(min(lumaN, lumaS), min(lumaE, lumaW)));
    float rangeMax = max(lumaM, max(max(lumaN, lumaS), max(lumaE, lumaW)));
    float range = rangeMax - rangeMin;

    // 1. Edge(주변 픽셀과 비교했을 때, 휘도나 색이 급격하게 변하는 구역)가 아니라면 바로 반환.
    float threshold = max(EdgeThresholdMin, rangeMax * EdgeThreshold);

    if (range < threshold)
    {
        return float4(rgbM, 1.0f);
    }

    // 2. 대각선도 읽어서 방향 판정 보강
    float3 rgbNW = SampleScene(uv + float2(-px.x, -px.y)); // 북서
    float3 rgbNE = SampleScene(uv + float2(px.x, -px.y)); // 북동
    float3 rgbSW = SampleScene(uv + float2(-px.x, px.y)); // 남서
    float3 rgbSE = SampleScene(uv + float2(px.x, px.y)); // 남동

    // 대각선 RGB -> Luma로 변경
    float lumaNW = Luma2(rgbNW);
    float lumaNE = Luma2(rgbNE);
    float lumaSW = Luma2(rgbSW);
    float lumaSE = Luma2(rgbSE);
    
    // Edge의 방향 판정 -> 경계가 가로인가, 세로인가? 
    // 픽셀 중심 통과 1픽셀 선 대응 목적 3x3 주변의 row/column별 high-pass크기 가중 합산
    // 가운데 줄/열에 더 큰 가중치를 준 이유는 중앙 row/column을 더 중요하게 보기 위해서
    float edgeVert = // 수직 Edge
        abs(0.25f * lumaNW - 0.5f * lumaN + 0.25f * lumaNE) +
        abs(0.50f * lumaW - 1.0f * lumaM + 0.50f * lumaE) +
        abs(0.25f * lumaSW - 0.5f * lumaS + 0.25f * lumaSE);

    float edgeHorz = // 수평 Edge
        abs(0.25f * lumaNW - 0.5f * lumaW + 0.25f * lumaSW) +
        abs(0.50f * lumaN - 1.0f * lumaM + 0.50f * lumaS) +
        abs(0.25f * lumaNE - 0.5f * lumaE + 0.25f * lumaSE);

    bool isHorizontal = (edgeHorz >= edgeVert);

    // --------------------------------------
    // 5. edge에 수직한 방향에서 더 강한 pair 선택
    //  더 강하다 == 이쪽에 경계가 있다
    // --------------------------------------
    float lumaNeg = isHorizontal ? lumaN : lumaW;
    float lumaPos = isHorizontal ? lumaS : lumaE;

    // Neg, Pos 방향 변화량 (Screen Space)
    float gradNeg = abs(lumaNeg - lumaM);
    float gradPos = abs(lumaPos - lumaM);

    // 차이가 더 큰 방향을 사용
    bool useNeg = (gradNeg >= gradPos);

    // edge에 수직한 방향(최종 offset 방향)
    float2 normalStep = isHorizontal ? float2(0.0f, px.y) : float2(px.x, 0.0f);
    // edge에 수평인 방향 (search 방향)
    float2 edgeStep = isHorizontal ? float2(px.x, 0.0f) : float2(0.0f, px.y);

    // 더 강한쪽으로 부호 결정
    float stepSign = useNeg ? -1.0f : 1.0f;
    float2 normal = normalStep * stepSign;

    // 선택된 두 픽셀의 평균 밝기(경계 단면의 기준선)
    float lumaLocalAverage = 0.5f * (lumaM + (useNeg ? lumaNeg : lumaPos));
    // Search 종료 기준
    float gradientScaled = 0.25f * max(gradNeg, gradPos);

    // --------------------------------------
    // 6. end-of-edge search
    // --------------------------------------
    // 선택된 pair 쪽으로 반 픽셀 이동한 위치에서 시작
    float2 uvEdge = uv + normal * 0.5f;

    float2 uv1 = uvEdge - edgeStep;
    float2 uv2 = uvEdge + edgeStep;

    float lumaEnd1 = Luma2(SampleScene(uv1)) - lumaLocalAverage;
    float lumaEnd2 = Luma2(SampleScene(uv2)) - lumaLocalAverage;

    bool reached1 = abs(lumaEnd1) >= gradientScaled;
    bool reached2 = abs(lumaEnd2) >= gradientScaled;

    // 8개 pixel만 순회
    [loop]
    for (int i = 0; i < 8 && !(reached1 && reached2); ++i)
    {
        if (!reached1)
        {
            uv1 -= edgeStep;
            lumaEnd1 = Luma2(SampleScene(uv1)) - lumaLocalAverage;
            reached1 = abs(lumaEnd1) >= gradientScaled;
        }

        if (!reached2)
        {
            uv2 += edgeStep;
            lumaEnd2 = Luma2(SampleScene(uv2)) - lumaLocalAverage;
            reached2 = abs(lumaEnd2) >= gradientScaled;
        }
    }

    // --------------------------------------
    // 7. search 결과를 offset 값으로 변환
    // --------------------------------------
    float distance1 = isHorizontal ? (uv.x - uv1.x) : (uv.y - uv1.y);
    float distance2 = isHorizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);

    // edge구간 전체 길이
    float spanLength = max(distance1 + distance2, 1e-4f);

    // 더 가까운 끝점의 휘도 선택.
    bool useSide1 = (distance1 < distance2);
    float lumaEndChosen = useSide1 ? lumaEnd1 : lumaEnd2;
    float lumaCenter = lumaM - lumaLocalAverage;

    // 부호가 맞을 때만 offset 허용
    bool correctVariation = ((lumaEndChosen < 0.0f) != (lumaCenter < 0.0f));

    // 0 ~ 0.5 정도의 sub-pixel shift
    /*
    경계 구간 한가운데에 있으면 이미 비교적 안정적
    경계 끝 근처에 있으면 톱니가 더 도드라지기 쉬움
    -> 끝점 근처일수록 더 보정해준다
    */
    float pixelOffset = -min(distance1, distance2) / spanLength + 0.5f;
    float edgeOffset = correctVariation ? pixelOffset : 0.0f;

    // --------------------------------------
    // 8. 계산된 offset 위치로 재샘플
    // --------------------------------------
    float3 rgbEdge = SampleScene(uv + normal * edgeOffset);

    // --------------------------------------
    // 9. 마지막에만 sub-pixel lowpass blend
    // --------------------------------------
    // 주변 픽셀 밝기의 평균을 낸다.
    float lumaAvg = (lumaN + lumaS + lumaW + lumaE) * 0.25f;
    // 주변 픽셀 밝기의 평균 - 현재 픽셀의 밝기 값을 정규화 한 값이 클 수록 픽셀이 튄다는 뜻이다.
    float subpix = saturate(abs(lumaAvg - lumaM) / max(range, 1e-4f));
    // 값을 그냥 쓰는게 아니라, 부드럽게 사용할 목적으로 곱해주기.
    subpix = subpix * subpix * (3.0f - 2.0f * subpix); // 부드럽게
    // sub-pixel aliasing 제거를 얼마나 할지 강도 조절 (다른 변수임)
    subpix *= Subpix;

    // lowpass 후보
    float3 rgbL = (rgbM + rgbN + rgbS + rgbW + rgbE + rgbNW + rgbNE + rgbSW + rgbSE) / 9.0f;

    // 주인공은 rgbEdge, lowpass는 보조
    // Lerp(a, b, s) = a * (1 - s) + b * s
    float3 result = lerp(rgbEdge, rgbL, subpix);
    return float4(result, 1.0f);
}

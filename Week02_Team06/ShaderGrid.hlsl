// XZ 평면에 기준 격자선을 픽셀 셰이더로 계산
cbuffer TransformData : register(b0)
{
    row_major float4x4 MVP;     
    float4             CamPos;  
};

struct VS_INPUT
{
    float4 position : POSITION;
    float4 color    : COLOR;
};

struct PS_INPUT
{
    float4 clipPos  : SV_Position;
    float3 worldPos : TEXCOORD0;
};

PS_INPUT VS_GRID(VS_INPUT input)
{
    PS_INPUT output;
    output.worldPos = input.position.xyz;
    output.clipPos  = mul(input.position, MVP);
    return output;
}

float4 PS_GRID(PS_INPUT input) : SV_Target
{
    float2 WorldXZ = input.worldPos.xz;
    
    //카메라랑 월드 기반 거리
    float dist = length(WorldXZ - CamPos.xz);
    
    float fade = 1.0 - saturate((dist - 80.0) / 120.0);
    
    float2 coordSmall = WorldXZ / 5.0;
    float2 fS = frac(coordSmall - 0.5) - 0.5;
    
    //월드 기준 옆 옆 픽셀과의 거리를 판단하여 클수록 
    float2 gridSmall = abs(fS) / fwidth(coordSmall);
    //결국 이게 알파값이 되어서 해당 픽셀을 그릴지 안그릴지 결정
    float2 smallGrid = 1.f - saturate(min(gridSmall.x, gridSmall.y));
    
    float2 coordBig = WorldXZ / 25.0;
    float2 fB = frac(coordBig - 0.5) - 0.5;
    float2 gridBig = abs(fB) / fwidth(coordBig);
    float2 bigGrid = 1.f - saturate(min(gridBig.x, gridBig.y));

    float alpha = max(smallGrid, bigGrid) * fade;
  
    float4 color = float4(0.38, 0.38, 0.38, alpha);
    
    float xAxisWidth = fwidth(input.worldPos.z)* 1.2f;
    float zAxisWidth = fwidth(input.worldPos.x)* 1.2f;
    
    if (abs(input.worldPos.x) <= zAxisWidth)
    {
        return float4(0.0, 0.0, 1.0, alpha);
    }
    if (abs(input.worldPos.z) <= xAxisWidth)
    {
        return float4(1.0, 0.0, 0.0, alpha);
    }
    
    //겹쳐서 생기는 모아레 현상 제거
    if (alpha < 0.005)
    {
        discard;
    }
    
    return color;
}

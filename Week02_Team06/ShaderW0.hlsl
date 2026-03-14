cbuffer TransformData : register(b0)
{
    row_major float4x4 MVP;
};

struct VS_INPUT
{
    float4 position : POSITION;
    float4 color : COLOR;
};


struct PS_INPUT
{
    float4 position : SV_Position;
    float4 color : COLOR;
};

PS_INPUT VS_MAIN(VS_INPUT input)
{
    PS_INPUT output;
    
    output.position = mul(input.position, MVP);

    output.color = input.color;
    
    return output;
}

float4 PS_MAIN(PS_INPUT input) : SV_Target
{
    return input.color;
}
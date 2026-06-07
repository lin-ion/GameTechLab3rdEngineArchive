// 신규 계층형 UI 의 단색 쿼드 셰이더.
// 정점 포맷/상수버퍼 레이아웃은 RmlUi.hlsl 과 동일(POSITION float2, COLOR float4,
// TEXCOORD float2 / b0: ViewportSize, Translation, Transform)하게 맞춰 입력 레이아웃을
// 그대로 재사용한다. 다만 PS 는 텍스처를 샘플하지 않고 정점 색을 그대로 출력해
// 텍스처/샘플러 바인딩 없이 단색 사각형을 그린다(진단 B1/D3, 사이클 5).
struct VSInput
{
	float2 Position : POSITION;
	float4 Color    : COLOR;
	float2 TexCoord : TEXCOORD;
};

struct VSOutput
{
	float4 Position : SV_POSITION;
	float4 Color    : COLOR;
	float2 TexCoord : TEXCOORD;
};

cbuffer SimpleUICB : register(b0)
{
	float2 ViewportSize;
	float2 Translation;
	column_major float4x4 Transform;
};

// [사이클 8] 텍스처 × 정점색. 텍스처 없는 단색 요소는 패스가 1×1 흰색 SRV 를 바인드해 곱셈 항등.
// 샘플러 s0 는 프레임 단위 시스템 샘플러(LinearClamp) — 패스가 별도 바인드하지 않음.
Texture2D    tex  : register(t0);
SamplerState samp : register(s0);

VSOutput VS(VSInput Input)
{
	VSOutput Output;
	// 픽셀(좌상단 원점) → NDC. RmlUi.hlsl 과 동일한 Y-down 변환(진단 B2).
	float4 PixelPosition = float4(Input.Position + Translation, 0.0f, 1.0f);
	PixelPosition = mul(Transform, PixelPosition);
	float2 NdcPosition = float2(
		(PixelPosition.x / ViewportSize.x) * 2.0f - 1.0f,
		1.0f - (PixelPosition.y / ViewportSize.y) * 2.0f
	);
	Output.Position = float4(NdcPosition, 0.0f, 1.0f);
	Output.Color = Input.Color;
	Output.TexCoord = Input.TexCoord;
	return Output;
}

float4 PS(VSOutput Input) : SV_Target
{
	return tex.Sample(samp, Input.TexCoord) * Input.Color;
}

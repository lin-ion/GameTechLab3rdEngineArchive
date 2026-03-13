#pragma once

using int32 = int;
using uint32 = unsigned int;
using FString = std::string;



#pragma once

struct FVertexSimple
{
	float X, Y, Z;
	float R, G, B, A;

	static constexpr int ElementNum = 2;
	static constexpr D3D11_INPUT_ELEMENT_DESC Elements[ElementNum] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
};
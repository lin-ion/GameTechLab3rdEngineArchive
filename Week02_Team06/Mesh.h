#pragma once
//추후 Mesh 컴포넌트로 바꿔야함 아직 언리얼 레퍼를 제대로 확인못해서 보류
class UMesh
{
public:
	UMesh() = default;
	~UMesh() = default;

public:
	void Load(ID3D11Device& Device, const FVertexSimple* vertices, UINT vertexCount);
	void Release();

	void Render(ID3D11DeviceContext& DeviceContext);

private:
	ID3D11Buffer* VertexBuffer = { nullptr };
	ID3D11Buffer* IndexBuffer = { nullptr };

	UINT Stride;
	UINT ByteWidth;
	UINT NumVertices;
};
#pragma once
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
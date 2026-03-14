#pragma once
//추후 Mesh 컴포넌트로 바꿔야함 아직 언리얼 레퍼를 제대로 확인못해서 보류
class UMesh
{
public:
	UMesh() = default;
	~UMesh() = default;
	//mesh 컴포넌트로 바뀌면서 수정해야함

public:
	const FVertexSimple * GetVertexData() const { return OriginData; };
	const int32 GetVertexNum() const { return NumVertices; };

public:
	void Load(ID3D11Device& Device, const FVertexSimple* vertices, UINT vertexCount);
	void Release();

	void Render(ID3D11DeviceContext& DeviceContext);

private:
	ID3D11Buffer* VertexBuffer = { nullptr };
	ID3D11Buffer* IndexBuffer = { nullptr };
	
	const FVertexSimple* OriginData = { nullptr };

	UINT Stride;
	int32 ByteWidth;
	int32 NumVertices;
};
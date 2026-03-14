#include "pch.h"
#include "Mesh.h"

void UMesh::Load(ID3D11Device& Device, const FVertexSimple* Vertices, UINT VertexCount, const uint32* Indices, UINT IndexCount)
{
	NumVertices = VertexCount;
	Stride = sizeof(FVertexSimple);
	ByteWidth = sizeof(FVertexSimple) * VertexCount;

	// 정점 버퍼 생성
	D3D11_BUFFER_DESC VertexBufferDesc = {};
	VertexBufferDesc.ByteWidth = ByteWidth;
	VertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA VertexBufferSRD = { Vertices };

	HRESULT hr = Device.CreateBuffer(&VertexBufferDesc, &VertexBufferSRD, &VertexBuffer);
	assert(SUCCEEDED(hr));

	// [수정] 인덱스 데이터가 실제로 있을 때만 버퍼를 생성하도록 보호(Guard)합니다.
	if (Indices != nullptr && IndexCount > 0)
	{
		NumIndices = IndexCount; // 개수 저장

		D3D11_BUFFER_DESC IndexBufferDesc = {};
		IndexBufferDesc.ByteWidth = sizeof(uint32) * IndexCount;
		IndexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA IndexBufferData = { Indices };
		Device.CreateBuffer(&IndexBufferDesc, &IndexBufferData, &IndexBuffer);
	}
}

void UMesh::Release()
{
	if (VertexBuffer)
	{
		VertexBuffer->Release();
		VertexBuffer = nullptr;
	}

	if (IndexBuffer)
	{
		IndexBuffer->Release();
		IndexBuffer = nullptr;
	}
}


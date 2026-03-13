#include "pch.h"
#include "UMesh.h"

void UMesh::Load(ID3D11Device& Device, const FVertexSimple* vertices, UINT vertexCount)
{
	NumVertices = vertexCount;
	Stride = sizeof(FVertexSimple);
	ByteWidth = sizeof(FVertexSimple) * vertexCount;

	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.ByteWidth = ByteWidth;
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexBufferSRD = { vertices };

	HRESULT hr = Device.CreateBuffer(&vertexBufferDesc, &vertexBufferSRD, &VertexBuffer);
	assert(SUCCEEDED(hr));
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


void UMesh::Render(ID3D11DeviceContext& DeviceContext)
{
	uint32 offset = 0;
	DeviceContext.IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &offset);
	DeviceContext.Draw(NumVertices, 0);
}


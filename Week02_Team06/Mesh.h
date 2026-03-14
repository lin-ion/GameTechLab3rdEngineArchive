#pragma once

#include <d3d11.h>
#include "Types.h"

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
	void Load(ID3D11Device& Device, const FVertexSimple* Vertices, UINT VertexCount, const uint32* Indices, UINT IndexCount);
	void Release();
	void BindBuffers(ID3D11DeviceContext& DeviceContext)
	{
		UINT offset = 0;
		DeviceContext.IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &offset);
		if (IndexBuffer)
		{
			DeviceContext.IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
		}
	}

	/** 정점 개수를 반환합니다. */
	uint32 GetVertexCount() const { return NumVertices; }

	// 추가: 인덱스 개수와 버퍼 유무를 확인하는 기능
	uint32 GetIndexCount() const { return NumIndices; }
	bool HasIndexBuffer() const { return IndexBuffer != nullptr; }

private:
	ID3D11Buffer* VertexBuffer = { nullptr };
	ID3D11Buffer* IndexBuffer = { nullptr };
	
	const FVertexSimple* OriginData = { nullptr };

	UINT Stride;
	UINT ByteWidth;
	UINT NumVertices;
	UINT NumIndices = 0; // 추가: 인덱스 개수 저장
};
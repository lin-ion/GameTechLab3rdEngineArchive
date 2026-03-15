#pragma once
#include "Mesh.h"

class UResourceManager
{
public:
	UResourceManager() = default;
	~UResourceManager() = default;

public:
	template<typename TVertex>
	void   LoadResourceData(ID3D11Device& Device, const FString& DataTypaName, const TVertex* Vertices, UINT VertexCount, const uint32* Indices, UINT IndexCount)
	{
		//이미 존재하는 리소스라면 추가X
		if (nullptr != MeshDatas.Find(DataTypaName))
		{
			return;
		}

		UMesh* Mesh = new UMesh;
		Mesh->Load<TVertex>(Device, Vertices, VertexCount, Indices, IndexCount);
		MeshDatas.Insert({ DataTypaName, Mesh });
	}

public:
	void Initialize(ID3D11Device& Device);
	void Release();

public:
	UMesh* FindMeshData(const FString& DataTypaName);

private:
	TMap<FString, UMesh*> MeshDatas;
};

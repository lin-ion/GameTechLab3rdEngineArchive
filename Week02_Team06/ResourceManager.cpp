#include "pch.h"
#include "ResourceManager.h"
#include "Mesh.h"

//외부 데이터
#include "Cube.h"
#include "Sphere.h"
#include "Triangle.h"
#include "Gizmo.h"

void UResourceManager::Initialize(ID3D11Device& Device)
{
	//인덱스 버퍼는 배제
	LoadResourceData<FVertexSimple>(Device, "Sphere", sphere_vertices, sizeof(sphere_vertices) / sizeof(FVertexSimple), nullptr, 0);
	LoadResourceData<FVertexSimple>(Device, "Cube", cube_vertices, sizeof(cube_vertices) / sizeof(FVertexSimple), nullptr, 0);
	LoadResourceData<FVertexSimple>(Device, "Triangle", triangle_vertices, sizeof(triangle_vertices) / sizeof(FVertexSimple), nullptr, 0);
	LoadResourceData<FVertexSimple>(Device, "Gizmo", gizmo_vertices, sizeof(gizmo_vertices) / sizeof(FVertexSimple), nullptr, 0);

}

void UResourceManager::Release()
{
	for (auto& iter : MeshDatas)
	{
		if (iter.second)
		{
			iter.second->Release();
			delete iter.second;
			iter.second = nullptr;
		}
	}
	MeshDatas.Clear();
}

UMesh* UResourceManager::FindMeshData(const FString& DataTypaName)
{
	if (nullptr == MeshDatas.Find(DataTypaName))
	{
		return nullptr;
	}

	return MeshDatas[DataTypaName];
}

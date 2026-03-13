#include "pch.h"
#include "PrimitiveComponent.h"
#include "Mesh.h"

void UPrimitiveComponent::Release()
{
	if (Mesh)
	{
		Mesh->Release();
		delete Mesh;
	}
}

void UPrimitiveComponent::Update(float DeltaTime)
{
	USceneComponent::Update(DeltaTime);

}

void UPrimitiveComponent::Render(ID3D11DeviceContext& DeviceContext)
{
	Mesh->Render(DeviceContext);
}

void UPrimitiveComponent::AddMesh(ID3D11Device& Device, const FVertexSimple* vertices, UINT vertexCount)
{
	Mesh = new UMesh;
	Mesh->Load(Device, vertices, vertexCount);
}

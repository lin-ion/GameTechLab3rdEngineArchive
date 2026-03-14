#include "pch.h"
#include "PrimitiveComponent.h"

#include "Mesh.h"

//테스트를 위해,,
#include "PickingComponent.h"

void UPrimitiveComponent::Release()
{
	if (Mesh)
	{
		Mesh->Release();
		delete Mesh;
	}

	if (Picking)
	{
		Picking->Release();
		delete Picking;
	}
}

void UPrimitiveComponent::TickComponent(float DeltaTime)
{
	USceneComponent::TickComponent(DeltaTime);

	if (Picking)
	{
		if (Picking->IsPicked(Mesh, GetComponentTransform()))
		{
			//색상 변경

		}
	}
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

void UPrimitiveComponent::AddPicking()
{
	//테스트용
	Picking = new UPickingComponent;
}

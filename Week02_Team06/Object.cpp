#include "pch.h"
#include "Object.h"
#include "Mesh.h"

#include "Input.h"

void UObject::Release()
{
	Mesh->Release();
	delete Mesh;
}

void UObject::Update(float DeltaTime)
{
	if (UInput::GetInstance().IsKeyHeld('A'))
	{
		Position.X -= DeltaTime;
	}
	if (UInput::GetInstance().IsKeyHeld('D'))
	{
		Position.X += DeltaTime;
	}
	if (UInput::GetInstance().IsKeyHeld('S'))
	{
		Position.Y -= DeltaTime;
	}
	if (UInput::GetInstance().IsKeyHeld('W'))
	{
		Position.Y += DeltaTime;
	}

	if (UInput::GetInstance().IsKeyHeld('Z'))
	{
		Radius += DeltaTime;
	}

	if (UInput::GetInstance().IsKeyHeld('X'))
	{
		Radius -= DeltaTime;
		Radius = max(0.02, Radius);
	}
}

void UObject::Render(ID3D11DeviceContext& DeviceContext)
{
	Mesh->Render(DeviceContext);
}

void UObject::AddMesh(ID3D11Device& Device, const FVertexSimple* vertices, UINT vertexCount)
{
	Mesh = new UMesh;
	Mesh->Load(Device, vertices, vertexCount);
}

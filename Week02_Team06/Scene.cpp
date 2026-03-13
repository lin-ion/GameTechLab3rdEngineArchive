#include "pch.h"
#include "Scene.h"

#include "Object.h"
#include "Sphere.h"

void UScene::Initialize(ID3D11Device& Device)
{
	TestObject = new UObject();
	TestObject->AddMesh(Device, sphere_vertices, sizeof(sphere_vertices) / sizeof(FVertexSimple));

}

void UScene::Update(float DeltaTime)
{
	TestObject->Update(DeltaTime);
}

void UScene::Release()
{
	if (TestObject)
	{
		TestObject->Release();
		delete TestObject;
	}
}

UObject* UScene::GetSceneObject()
{
	return TestObject;
}

#include "pch.h"
#include "Scene.h"

#include "Object.h"
#include "Sphere.h"
#include "GeometryData.h"

void UScene::Initialize(ID3D11Device& Device)
{
	TestObject = new UObject();
	UObject* TestCube = new UObject();

	TestObject->AddMesh(Device, sphere_vertices, sizeof(sphere_vertices) / sizeof(FVertexSimple));
	TestCube->AddMesh(Device, CubeVertices, sizeof(CubeVertices) / sizeof(FVertexSimple));

	AddObject(TestCube);
}

void UScene::Update(float DeltaTime)
{
	for (UObject* obj : SceneObjects)
	{
		obj->Update(DeltaTime);
	}

	if (TestObject) TestObject->Update(DeltaTime);
}

void UScene::Release()
{
	if (TestObject)
	{
		for (UObject* obj : SceneObjects)
		{
			if (obj)
			{
				obj->Release();
				delete obj;
			}
		}
		SceneObjects.clear();

		if (TestObject)
		{
			TestObject->Release();
			delete TestObject;
			TestObject = nullptr;
		}
	}
}

void UScene::AddObject(UObject* object)
{
	if (object != nullptr)
	{
		SceneObjects.push_back(object);
	}
}

UObject* UScene::GetSceneObject()
{
	return TestObject;
}

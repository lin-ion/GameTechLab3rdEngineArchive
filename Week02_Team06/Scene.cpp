#include "pch.h"
#include "Scene.h"

#include "Object.h"
#include "Cube.h"
#include "Sphere.h"
#include "Triangle.h"
#include "PrimitiveComponent.h"

#include "ObjectFactory.h"
#include "GeometryData.h"

#include "json.hpp"

void UScene::Initialize(ID3D11Device& Device)
{
	UPrimitiveComponent* Cube = UObjectFactory::NewObject<UPrimitiveComponent>();
	Cube->AddMesh(Device, cube_vertices, sizeof(cube_vertices) / sizeof(FVertexSimple));


	UPrimitiveComponent* Sphere = UObjectFactory::NewObject<UPrimitiveComponent>();
	Sphere->AddMesh(Device, sphere_vertices, sizeof(sphere_vertices) / sizeof(FVertexSimple));
}

void UScene::Update(float DeltaTime)
{
	for (size_t i = 0; i < GUObjectArray.Size(); ++i)
	{
		GUObjectArray[i]->Update(DeltaTime);
	}
}

void UScene::Release()
{
	UObjectFactory::DestroyAllObjects();
}


#include "pch.h"
#include "Scene.h"

#include "Object.h"
#include "Actor.h"
#include "ActorComponent.h"
#include "Cube.h"
#include "Sphere.h"
#include "Triangle.h"
#include "PrimitiveComponent.h"

#include "ObjectFactory.h"
#include "GeometryData.h"
#include "CameraComponent.h"

void UScene::Initialize(ID3D11Device& Device)
{
	UPrimitiveComponent* Cube = UObjectFactory::NewObject<UPrimitiveComponent>();
	Cube->AddMesh(Device, cube_vertices, sizeof(cube_vertices) / sizeof(FVertexSimple));
	Cube->SetRotation(FVector(0.0f, 45.0f, 20.0f));


	UPrimitiveComponent* Sphere = UObjectFactory::NewObject<UPrimitiveComponent>();
	Sphere->AddMesh(Device, sphere_vertices, sizeof(sphere_vertices) / sizeof(FVertexSimple));

	MainCamera = UObjectFactory::NewObject<UCameraComponent>();
	MainCamera->SetPosition({ 1.0f, 1.0f, -5.0f });
}

void UScene::Update(float DeltaTime)
{
	for (uint32 i = 0; i < GUObjectArray.Size(); ++i)
	{
		if (AActor* Actor = dynamic_cast<AActor*>(GUObjectArray[i]))
		{
			Actor->Tick(DeltaTime);
		}
		else if (UActorComponent* Comp = dynamic_cast<UActorComponent*>(GUObjectArray[i]))
		{
			Comp->TickComponent(DeltaTime);
		}
	}
}

void UScene::Release()
{
	UObjectFactory::DestroyAllObjects();
}


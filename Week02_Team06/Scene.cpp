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
	Cube->AddPicking();

	Cube->SetPosition({ 1.0f, 0.2f, 0.5f });
	Cube->SetRotation({ 0.0f, 45.0f, 20.0f });
	Cube->SetScale({ 0.2f, 0.2f, 0.2f });

	UPrimitiveComponent* Sphere = UObjectFactory::NewObject<UPrimitiveComponent>();
	Sphere->AddMesh(Device, sphere_vertices, sizeof(sphere_vertices) / sizeof(FVertexSimple));
	Sphere->SetPosition({ -1.0f, 0.2f, 0.5f });
	Sphere->SetRotation({ 0.0f, 0.0f, 0.0f });
	Sphere->SetScale({ 0.2f, 0.2f, 0.2f });

	MainCamera = UObjectFactory::NewObject<UCameraComponent>();

	MainCamera->SetPosition({ 0.0f, 0.0f, -5.0f });
	MainCamera->SetRotation({ 0.0f, 0.0f, 0.0f });

	MainCamera->SetFOV(60.0f);
	//MainCamera->AspectRatio = ViewportInfo.Width / ViewportInfo.Height; // Renderer에서 Viewport 정보 받아서 설정해야함
	MainCamera->SetNearPlane(0.1f);
	MainCamera->SetFarPlane(10.0f);
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


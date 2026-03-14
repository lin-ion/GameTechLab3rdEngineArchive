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
#include "MeshComponent.h"

#include "json.hpp"

void UScene::Initialize(ID3D11Device& Device)
{
	UMeshComponent* Cube = UObjectFactory::NewObject<UMeshComponent>();
	Cube->AddMesh(Device, cube_vertices, sizeof(cube_vertices) / sizeof(FVertexSimple));
	Cube->SetPosition({ 0.5f, 0.2f, 0.5f });
	Cube->SetRotation({ 0.0f, 45.0f, 20.0f });

	// 스피어도 마찬가지로 UMeshComponent로 생성합니다.
	UMeshComponent* Sphere = UObjectFactory::NewObject<UMeshComponent>();
	Sphere->AddMesh(Device, sphere_vertices, sizeof(sphere_vertices) / sizeof(FVertexSimple));

	MainCamera = UObjectFactory::NewObject<UCameraComponent>();

	MainCamera->SetPosition({ 0.0f, 0.0f, 5.0f });
	MainCamera->SetRotation({ 0.0f, 180.0f, 0.0f });

	MainCamera->FOV = 60.0f;
	//MainCamera->AspectRatio = ViewportInfo.Width / ViewportInfo.Height; // Renderer에서 Viewport 정보 받아서 설정해야함
	MainCamera->NearPlane = 0.1f;
	MainCamera->FarPlane = 10.0f;
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


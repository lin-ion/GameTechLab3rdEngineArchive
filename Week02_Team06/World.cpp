#include "pch.h"
#include "World.h"

#include "ResourceManager.h"
#include "ObjectFactory.h"
#include "Level.h"
#include "Actor.h"
#include "Object.h"

#include "CubeComponent.h"
#include "ImGuiDrawer.h"


void UWorld::InitWorld(UResourceManager* ResourceManager)
{
	CurrentLevel = UObjectFactory::NewObject<ULevel>();

	resourceManager = ResourceManager;

	/*
	AActor* CubeActor = SpawnActor<AActor>();

	//레벨에 엑터 추가
	UCubeComponent* CubeComponent = CubeActor->AddComponent<UCubeComponent>();
	CubeComponent->SetMesh(ResourceManager.FindMeshData("Cube"));

	CubeComponent->SetPosition({ 0.0f, 0.0f, 3.0f }); // 카메라 앞에 배치
	CubeComponent->SetScale({ 0.5f, 0.5f, 0.5f });
	*/
}

void UWorld::Tick(float DeltaTime)
{
	if (!CurrentLevel) return;

	for (uint32 i = 0; i < CurrentLevel->Actors.Size(); ++i)
	{
		CurrentLevel->Actors[i]->Tick(DeltaTime);
	}
}

void UWorld::Release()
{
	// World는 참조만 정리
	CurrentLevel = nullptr;
}

void UWorld::SpawnActorFromEditor(FSpawnParameters params)
{
	for (int i = 0; i < params.Count; i++)
	{
		AActor* actor = SpawnActor<AActor>();

		if (params.PrimitiveType == "Cube")
		{
			UCubeComponent* Cube = actor->AddComponent<UCubeComponent>();
			Cube->SetMesh(resourceManager->FindMeshData("Cube"));
			actor->RootComponent = Cube;
		}
		else
		{
			return;
		}
		// 추후 다른 preimitive들도 추가 예정

		actor->RootComponent->SetPosition(params.Location);
		actor->RootComponent->SetRotation(params.Rotation);
		actor->RootComponent->SetScale(params.Scale);

		UE_LOG("GUObject.Size : %d", GUObjectArray.Size());
	}
}

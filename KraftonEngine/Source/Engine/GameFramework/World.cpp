#include "GameFramework/World.h"
#include "GameFramework/Scene.h"
#include "Object/ObjectFactory.h"
#include <algorithm>
#include <memory>

IMPLEMENT_CLASS(UWorld, UObject)

UWorld::UWorld()
{
	InitWorld();
}

UWorld::~UWorld()
{
	if (bHasBegunPlay)
	{
		EndPlay();
	}

	if (ActiveScene)
	{
		UObjectManager::Get().DestroyObject(ActiveScene);
		ActiveScene = nullptr;
	}
	if (PersistentScene)
	{
		UObjectManager::Get().DestroyObject(PersistentScene);
		PersistentScene = nullptr;
	}
}

void UWorld::DestroyActor(AActor* Actor)
{
	if (!Actor) return;

	if (UScene* Scene = Actor->GetScene())
	{
		Scene->RemoveActor(Actor);
	}

	// Mark for garbage collection
	UObjectManager::Get().DestroyObject(Actor);
}

const TArray<AActor*>& UWorld::GetActors() const
{
	// NOTE: For compatibility, we return PersistentScene's actors.
	// In the future, we might want to return a combined list or change the API.
	return ActiveScene->GetActors();
}

void UWorld::InitWorld()
{
	if (!ActiveScene)
	{
		ActiveScene = UObjectManager::Get().CreateObject<UScene>();
		ActiveScene->SetWorld(this);
	}
	if (!PersistentScene)
	{
		PersistentScene = UObjectManager::Get().CreateObject<UScene>();
		PersistentScene->SetWorld(this);
	}
}

void UWorld::BeginPlay()
{
	bHasBegunPlay = true;

	if (ActiveScene) ActiveScene->BeginPlay();
	if (PersistentScene) PersistentScene->BeginPlay();
}

void UWorld::Tick(float DeltaTime)
{
	if (ActiveScene) ActiveScene->Tick(DeltaTime);
	if (PersistentScene) PersistentScene->Tick(DeltaTime);
}

void UWorld::EndPlay()
{
	bHasBegunPlay = false;

	if (ActiveScene) ActiveScene->EndPlay();
	if (PersistentScene) PersistentScene->EndPlay();
}

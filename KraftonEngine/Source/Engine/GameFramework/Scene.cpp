#include "Scene.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include <algorithm>

DEFINE_CLASS(UScene, UObject)

UScene::UScene()
{
	RenderProxy = std::make_unique<FWorldRenderProxy>();
}

UScene::~UScene()
{
	EndPlay();
}

void UScene::AddActor(AActor* Actor)
{
	if (Actor)
	{
		Actors.push_back(Actor);
		Actor->SetScene(this);
		Actor->RegisterAllComponents();
		if (bHasBegunPlay)
		{
			Actor->BeginPlay();
		}
	}
}

void UScene::RemoveActor(AActor* Actor)
{
	if (!Actor) return;

	Actor->UnregisterAllComponents();
	Actor->EndPlay();

	auto it = std::find(Actors.begin(), Actors.end(), Actor);
	if (it != Actors.end())
		Actors.erase(it);

	Actor->SetScene(nullptr);
}

void UScene::BeginPlay()
{
	bHasBegunPlay = true;
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->BeginPlay();
		}
	}
}

void UScene::Tick(float DeltaTime)
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->Tick(DeltaTime);
		}
	}
}

void UScene::EndPlay()
{
	bHasBegunPlay = false;

	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->UnregisterAllComponents();
			Actor->EndPlay();
			UObjectManager::Get().DestroyObject(Actor);
		}
	}

	Actors.clear();
}

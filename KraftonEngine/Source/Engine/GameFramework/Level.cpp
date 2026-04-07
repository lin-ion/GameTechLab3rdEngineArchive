#include "Level.h"
#include "GameFramework/AActor.h"
#include "Object/ObjectFactory.h"
#include <algorithm>

DEFINE_CLASS(ULevel, UObject)

ULevel::ULevel()
{
	RenderProxy = std::make_unique<FWorldRenderProxy>();
}

ULevel::~ULevel()
{
	EndPlay();
}

void ULevel::AddActor(AActor* Actor)
{
	if (Actor)
	{
		Actors.push_back(Actor);
		Actor->SetLevel(this);
		Actor->RegisterAllComponents();
		if (bHasBegunPlay)
		{
			Actor->BeginPlay();
		}
	}
}

void ULevel::RemoveActor(AActor* Actor)
{
	if (!Actor) return;

	Actor->UnregisterAllComponents();
	Actor->EndPlay();

	auto it = std::find(Actors.begin(), Actors.end(), Actor);
	if (it != Actors.end())
		Actors.erase(it);

	Actor->SetLevel(nullptr);
}

void ULevel::BeginPlay()
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

void ULevel::Tick(float DeltaTime)
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->Tick(DeltaTime);
		}
	}
}

void ULevel::EndPlay()
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

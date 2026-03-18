#include "pch.h"
#include "Actor.h"
#include "Level.h"
#include "World.h"
#include "ObjectFactory.h"

#include "SceneComponent.h"
#include "SphereComponent.h"
#include "ArrowComponent.h"
#include "ResourceManager.h"


IMPLEMENT_CLASS(AActor, UObject)

UWorld* AActor::GetWorld()
{
	if (OwningLevel)
	{
		return OwningLevel->GetWorld();
	}
	return nullptr;
}

void AActor::BeginPlay()
{
	//모든 엑터에 레이 충돌용 바운드 스피어를 생성

	BoundingSphere = AddComponent<USphereComponent>();
	BoundingSphere->SetDebugMode(true);
	BoundingSphere->SetMesh(GetWorld()->resourceManager->FindMeshData("Sphere"));
	BoundingSphere->SetRelativeScale({ 3.f, 3.f, 3.f });
}

void AActor::Tick(float DeltaTime)
{
	for (int32 i = 0; i < Components.Size(); ++i)
	{
		Components[i]->TickComponent(DeltaTime);
	}


	for (int32 i = 0; i < Components.Size(); ++i)
	{
		if (!Components[i]->IsA(USceneComponent::StaticClass())) continue;

		USceneComponent* SceneComponent = static_cast<USceneComponent*>(Components[i]);

		if (Components[i]->IsA(USceneComponent::StaticClass()))
		{
			USceneComponent* SceneComponent = static_cast<USceneComponent*>(Components[i]);
			SceneComponent->SetPosition(RootComponent->GetPosition());

			FVector RootScale = RootComponent->GetScale();
			FVector RelativeScale = SceneComponent->GetRelativeScale();

			SceneComponent->SetScale({ RootScale.X * RelativeScale.X , RootScale.Y * RelativeScale.Y , RootScale.Z * RelativeScale.Z });
		}
	}
}

void AActor::Release()
{
	// Actor 컴포넌트 또한 GUIObject가 관리한다
	Components.Clear();
}

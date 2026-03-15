#pragma once
#include "Object.h"
#include "ActorComponent.h"
#include "ObjectFactory.h"

class UWorld;
class ULevel;
class USceneComponent;

class AActor : public UObject
{
public:
	AActor() = default;
	virtual ~AActor() = default;

public:
	UWorld* GetWorld();

	virtual void BeginPlay() {}
	virtual void Tick(float DeltaTime);
	virtual void Release() override;

public:
	/** 컴포넌트를 생성하고 이 Actor에 등록 */
	template<typename T>
	T* AddComponent()
	{
		static_assert(std::is_base_of_v<UActorComponent, T>, "T must derive from UActorComponent");
		T* ActorComponent    = NewObject<T>();
		ActorComponent->Owner = this;

		Components.PushBack(ActorComponent);
		return ActorComponent;
	}

	/** 타입으로 컴포넌트 검색 */
	template<typename T>
	T* GetComponentByClass()
	{
		for (uint32 i = 0; i < Components.Size(); ++i)
		{
			if (T* Found = dynamic_cast<T*>(Components[i]))
				return Found;
		}
		return nullptr;
	}

public:
	USceneComponent* RootComponent = nullptr;
	ULevel*          OwningLevel   = nullptr;

private:
	TArray<UActorComponent*> Components;
};


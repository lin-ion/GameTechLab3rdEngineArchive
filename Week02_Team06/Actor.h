#pragma once
#include "Defines.h"
#include "Object.h"
#include "ActorComponent.h"
#include "ObjectFactory.h"
#include "Containers.h"

class UWorld;
class ULevel;
class USceneComponent;

class AActor : public UObject
{
	DECLARE_CLASS(AActor, UObject)

public:
	AActor() = default;
	virtual ~AActor() = default;

public:
	UWorld* GetWorld();

	//사실상 Actor 준비는 안함
	virtual void BeginPlay();

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
		ActorComponent->InitializeComponent();

		Components.PushBack(ActorComponent);
		return ActorComponent;
	}

	void AddComponent(UActorComponent* InComponent)
	{
		if (!InComponent) return;
		InComponent->Owner = this;
		Components.PushBack(InComponent);
	}

	template<typename T>
	bool RemoveComponent()
	{
		static_assert(std::is_base_of_v<UActorComponent, T>, "T must derive from UActorComponent");

		uint64 currentSize = Components.Size();
		for (uint64 i = 0; i < currentSize; ++i)
		{
			// 다형성을 이용해 해당 타입의 부품을 찾습니다.
			T* Target = dynamic_cast<T*>(Components[i]);
			if (Target)
			{
				uint64 lastIndex = currentSize - 1;

				// [RemoveSwap] 삭제할 위치에 마지막 요소를 덮어씌웁니다.
				if (i != lastIndex)
				{
					Components[i] = Components[lastIndex];
				}

				// 마지막 칸을 비워 공정을 마무리합니다.
				Components.PopBack();

				// 소유권 해제 (퇴거 처리)
				Target->Owner = nullptr;
				return true; // 성공적으로 분리됨
			}
		}
		return false; // 해당 타입의 부품이 명단에 없음
	}

	void RemoveComponent(UActorComponent* InComponent)
	{
		if (!InComponent) return;

			uint64 currentSize = Components.Size();
		for (uint64 i = 0; i < currentSize; ++i)
		{
			// 정확한 메모리 주소를 대조하여 식별합니다.
			if (Components[i] == InComponent)
			{
				uint64 lastIndex = currentSize - 1;

				// [RemoveSwap] 순서보다 속도가 중요한 에디터 환경의 최적화 공정입니다.
				if (i != lastIndex)
				{
					Components[i] = Components[lastIndex];
				}

				Components.PopBack();
				InComponent->Owner = nullptr;
				return;
			}
		}
	}

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

	/** 타입으로 컴포넌트 전부 검색 */
	template<typename T>
	TArray<T*> GetComponentArrayByClass()
	{
		TArray<T*> Result;
		for (uint32 i = 0; i < Components.Size(); ++i)
		{
			if (T* Found = dynamic_cast<T*>(Components[i]))
				Result.PushBack(Found);
		}
		return Result;
	}

public:
	USceneComponent* RootComponent = nullptr;
	ULevel*          OwningLevel   = nullptr;

private:
	TArray<UActorComponent*> Components;
};


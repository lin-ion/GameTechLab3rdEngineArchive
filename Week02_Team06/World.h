#pragma once
#include "Object.h"
#include "Level.h"
#include "Actor.h"
#include "ObjectFactory.h"

class UWorld : public UObject
{
public:
	UWorld() = default;
	virtual ~UWorld() = default;

public:
	/** Actor를 생성하고 PersistentLevel에 등록한 뒤 BeginPlay 호출 */
	template<typename T>
	T* SpawnActor()
	{
		static_assert(std::is_base_of_v<AActor, T>, "T must derive from AActor");

		T* Actor           = NewObject<T>();
		Actor->OwningLevel = PersistentLevel;

		if (PersistentLevel)
		{
			PersistentLevel->Actors.PushBack(Actor);
		}

		Actor->BeginPlay();
		return Actor;
	}

	virtual void Release() override;

public:
	// 월드가 시작할 때 초기 레벨
	ULevel* PersistentLevel = nullptr;
};


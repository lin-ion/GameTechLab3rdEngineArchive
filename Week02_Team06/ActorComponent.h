#pragma once
#include "Object.h"

class AActor;

class UActorComponent : public UObject
{
	friend class AActor;

public:
	UActorComponent() = default;
	virtual ~UActorComponent() = default;

public:
	AActor* GetOwner() const { return Owner; }

	/** Templated version of GetOwner(), will return nullptr if cast fails */
	template<class T>
	T* GetOwner() const
	{
		return dynamic_cast<T*>(GetOwner());
	}

	virtual void BeginPlay() {}
	virtual void TickComponent(float DeltaTime) {}

private:
	AActor* Owner = nullptr;
};


#include "pch.h"
#include "Actor.h"
#include "Level.h"
#include "World.h"
#include "ObjectFactory.h"

UWorld* AActor::GetWorld()
{
	if (OwningLevel)
	{
		return OwningLevel->OwningWorld;
	}
	return nullptr;
}

void AActor::Release()
{
	for (int32 i = 0; i < Components.Size(); ++i)
	{
		UObjectFactory::DestroyObject(Components[i]);
	}

	Components.Clear();
	RootComponent = nullptr;
}

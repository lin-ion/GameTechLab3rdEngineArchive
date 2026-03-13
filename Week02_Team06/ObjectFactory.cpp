#include "pch.h"
#include "ObjectFactory.h"
#include "EngineStatics.h"

void UObjectFactory::DestroyObject(UObject* Obj)
{
	if (!Obj) return;

	uint32 Idx     = Obj->internalIndex;
	uint32 LastIdx = GUObjectArray.Size() - 1;

	if (Idx != LastIdx)
	{
		GUObjectArray[Idx] = GUObjectArray[LastIdx];
		GUObjectArray[Idx]->internalIndex = Idx;
	}
	GUObjectArray.PopBack();

	Obj->Release();
	delete Obj;
}

void UObjectFactory::DestroyAllObjects()
{
	for (uint32 i = 0; i < GUObjectArray.Size(); i++)
	{
		GUObjectArray[i]->Release();
		delete GUObjectArray[i];
	}

	GUObjectArray.Clear();
}

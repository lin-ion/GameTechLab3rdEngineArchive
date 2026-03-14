#pragma once

class UObject
{
	friend class UObjectFactory;
	friend class UImGuiDrawer;

public:
	UObject() = default;
	virtual ~UObject() = default;

public:
	uint32 UUID = { 0 };
	uint32 internalIndex = { }; // Index of GUObjectArray

public:
	virtual void Release() {}
};

extern TArray<UObject*> GUObjectArray;

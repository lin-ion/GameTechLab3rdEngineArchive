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
	virtual void Release() = 0;
	virtual void Update(float DeltaTime) = 0;
	virtual void Render(ID3D11DeviceContext& DeviceContext) = 0;
};

extern TArray<UObject*> GUObjectArray;

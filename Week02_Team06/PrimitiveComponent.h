#pragma once
#include "Defines.h"
#include "SceneComponent.h"

class UMesh;

class UPrimitiveComponent : public USceneComponent
{
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

public:
	UPrimitiveComponent() = default;

public:
	void Release() override;
	void TickComponent(float DeltaTime) override;

	virtual void Render(ID3D11DeviceContext& DevcieContext) = 0;
};

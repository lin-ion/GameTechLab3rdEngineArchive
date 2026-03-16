#pragma once
#include "SceneComponent.h"

class UMesh;

class UPrimitiveComponent : public USceneComponent
{
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

public:
	UPrimitiveComponent() = default;
	virtual ~UPrimitiveComponent() = default;

public:
	void Release() override;
	void TickComponent(float DeltaTime) override;


	virtual void RenderOutline(ID3D11DeviceContext& DevcieContext) {};
	virtual void Render(ID3D11DeviceContext& DevcieContext) = 0;


};

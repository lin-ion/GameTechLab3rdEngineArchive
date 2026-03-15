#pragma once
#include "SceneComponent.h"

class UMesh;

class UPrimitiveComponent : public USceneComponent
{
public:
	UPrimitiveComponent() = default;
	virtual ~UPrimitiveComponent() = default;
  
public:
	void Release() override;
	void TickComponent(float DeltaTime) override;
	
	virtual void Render(ID3D11DeviceContext& DevcieContext) = 0;


};

#pragma once
#include "SceneComponent.h"

struct ID3D11DeviceContext;
struct ID3D11Buffer;

class UPrimitiveComponent : public USceneComponent
{
public:
	UPrimitiveComponent() = default;
	virtual ~UPrimitiveComponent() = default;

public:
	void Release() override;
	void TickComponent(float DeltaTime) override;
	virtual void Render(ID3D11DeviceContext* DeviceContext, const FMatrix& ViewProjection, ID3D11Buffer* ConstantBuffer) = 0;
};
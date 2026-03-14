#pragma once
#include "SceneComponent.h"

class UMesh;
class UPickingComponent;

class UPrimitiveComponent : public USceneComponent
{
public:
	UPrimitiveComponent() = default;
	virtual ~UPrimitiveComponent() = default;

public:
	//추후 수정예정
	const UMesh* GetMesh() const { return Mesh; };

public:
	void Release() override;
	void TickComponent(float DeltaTime) override;
	virtual void Render(ID3D11DeviceContext* DeviceContext, const FMatrix& ViewProjection, ID3D11Buffer* ConstantBuffer) = 0;
};

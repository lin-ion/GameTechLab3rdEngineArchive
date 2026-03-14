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
	void Render(ID3D11DeviceContext& DeviceContext) override;

public:
	//추후 컴포넌트로 수정
	void AddMesh(ID3D11Device& Device, const FVertexSimple* vertices, UINT vertexCount);
	void AddPicking();
private:

	//VertexBuffer를 소유
	UMesh* Mesh = { nullptr };
	UPickingComponent* Picking = { nullptr };

};


#pragma once
#include "Defines.h"
#include "SceneComponent.h"

class UMesh;

enum class EPrimitiveType
{
	Cube,
	Sphere,
	Triangle
};

class UPrimitiveComponent : public USceneComponent
{
	DECLARE_CLASS(UPrimitiveComponent, USceneComponent)

public:
	UPrimitiveComponent() = default;

public:
	const UMesh* GetMesh() const { return MeshData; }
	void  SetMesh(UMesh* _MeshData) { MeshData = _MeshData; }

	bool  IsSelected() { return IsHovering; }
	void  SetHovering(bool bFlag) { IsHovering = bFlag; };

public:
	void TickComponent(float DeltaTime) override;
	virtual void Render(ID3D11DeviceContext* DeviceContext, const FMatrix& ViewProjection, ID3D11Buffer* ConstantBuffer) override = 0;
	void Release() override;

protected:
	bool IsHovering = { false };
	UMesh* MeshData = nullptr;
};

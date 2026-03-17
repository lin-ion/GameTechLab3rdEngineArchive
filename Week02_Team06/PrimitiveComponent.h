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

	const FVector4& GetColor() const { return Color; }
	void SetColor(const FVector4& InColor) { Color = InColor; }

	bool  IsSelected() { return IsHovering; }
	void  SetHovering(bool bFlag) { IsHovering = bFlag; };

public:
	void TickComponent(float DeltaTime) override;
	virtual void Render(ID3D11DeviceContext& DeviceContext) override = 0;
	void Release() override;

protected:
	FVector4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
	
	bool IsHovering = { false };
	UMesh* MeshData = nullptr;
};

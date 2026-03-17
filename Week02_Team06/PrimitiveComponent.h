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
	const UMesh* GetMesh() const { return MeshData; }
	void  SetMesh(UMesh* _MeshData) { MeshData = _MeshData; }

	const FVector4& GetColor() { return Color; };
	void  SetColor(const FVector4& _color) { Color = _color; };

	bool  IsSelected() { return IsHovering; }
	void  SetHovering(bool bFlag) { IsHovering = bFlag; };

	bool  IsDebugMode() { return IsDebugUse; };
	void  SetDebugMode(bool isDebug) { IsDebugUse = isDebug; };

public:
	void TickComponent(float DeltaTime) override;
	virtual void Render(ID3D11DeviceContext& DeviceContext) override = 0;
	void Release() override;

protected:
	bool IsHovering = { false };
	bool IsDebugUse = { false };

	D3D11_CULL_MODE CullMode = D3D11_CULL_NONE;
	FVector4 Color = { };

	UMesh* MeshData = nullptr;
};

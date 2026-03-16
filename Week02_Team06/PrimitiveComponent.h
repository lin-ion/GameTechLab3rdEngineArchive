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
	const UMesh* GetMesh() { return MeshData; } const
	void   SetMesh(UMesh* _MeshData) { MeshData = _MeshData; }

	bool  IsSelected() { return IsHovering; }
	void  SetHovering(bool bFlag) { IsHovering = bFlag;};

public:
	void TickComponent(float DeltaTime) override;

	virtual void Render(ID3D11DeviceContext& DevcieContext) = 0;
	void Release() override;

protected :
	bool IsHovering = { false };
	UMesh* MeshData = nullptr;

};

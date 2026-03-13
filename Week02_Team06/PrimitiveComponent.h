#pragma once

#include "SceneComponent.h"

class UMesh;

class UPrimitiveComponent : public USceneComponent
{
public:
	UPrimitiveComponent() = default;
	virtual ~UPrimitiveComponent() = default;

public:
	// UObject을(를) 통해 상속됨
	void Release() override;

	void Update(float DeltaTime) override;

	void Render(ID3D11DeviceContext& DeviceContext) override;

public:
	//추후 컴포넌트로 수정
	void AddMesh(ID3D11Device& Device, const FVertexSimple* vertices, UINT vertexCount);

private:
	//VertexBuffer를 소유
	UMesh* Mesh = { nullptr };

};


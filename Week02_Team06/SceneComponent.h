#pragma once
#include "Object.h"

class UMesh;

class USceneComponent : public UObject
{
public:
	USceneComponent() = default;
	virtual ~USceneComponent() = default;

public:
	FVector& GetPosition() { return Position; };
	float	 GetRadius()   { return Radius; }

public:
	// UObject을(를) 통해 상속됨
	void Release() override;

	void Update(float DeltaTime) override;
	void Render(ID3D11DeviceContext& DeviceContext) override;

private:
	//나중에 컴포넌트로 바꿀 예정
	//추후 Matrix로 스케일 / 회전 / 위치 정보를 저장
	FVector Position = {};
	float   Radius = { 0.1f };
};


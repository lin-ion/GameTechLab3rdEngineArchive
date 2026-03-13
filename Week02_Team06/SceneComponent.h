#pragma once
#include "Object.h"

class UMesh;

class USceneComponent : public UObject
{
public:
	USceneComponent() = default;
	virtual ~USceneComponent() = default;

public:
	const FVector& GetPosition() { return Position; };
	void SetPosition(const FVector& InPosition) { Position = InPosition; };
	const FVector& GetRotation() { return Rotation; };
	void SetRotation(const FVector& InRotation) { Rotation = InRotation; };
	const FVector& GetScale() { return Scale; };
	void SetScale(const FVector& InScale) { Scale = InScale; };

public:
	// UObject을(를) 통해 상속됨
	void Release() override;

	void Update(float DeltaTime) override;
	void Render(ID3D11DeviceContext& DeviceContext) override;

private:
	//나중에 컴포넌트로 바꿀 예정
	//추후 Matrix로 스케일 / 회전 / 위치 정보를 저장
	FVector Position = { 0.0f, 0.0f, 0.0f };
	FVector Rotation = { 0.0f, 0.0f, 0.0f };
	FVector Scale = { 1.0f, 1.0f, 1.0f };
};


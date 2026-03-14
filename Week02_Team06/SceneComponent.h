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
	void SetPosition(const FVector& InPosition);

	const FVector& GetRotation() { return Rotation; };
	void SetRotation(const FVector& InRotation);

	const FVector& GetScale();
	void SetScale(const FVector& InScale);

	FVector GetComponentLocation() const;
	FVector GetForwardVector() const;
	FVector GetUpVector() const;
	FVector GetRightVector() const;
	FMatrix GetComponentTransform() const { return ComponentToWorld; }

public:
	// UObject을(를) 통해 상속됨
	void Release() override;

	void Update(float DeltaTime) override;
	void Render(ID3D11DeviceContext& DeviceContext) override;

protected:
	// TODO: Dirty Flag 추가 고려
	virtual void UpdateTransform();

private:
	// 나중에 컴포넌트로 바꿀 예정
	// TODO: FTransform 사용
	FVector Position = { 0.0f, 0.0f, 0.0f };
	// TODO: Euler Angle이 아닌 Quaternion으로 회전 표현 고려
	FVector Rotation = { 0.0f, 0.0f, 0.0f };
	FVector Scale = { 1.0f, 1.0f, 1.0f };

	// MVP 행렬의 Model 행렬에 해당
	FMatrix ComponentToWorld = FMatrix::Identity;
};

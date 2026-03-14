#pragma once
#include "ActorComponent.h"

struct ID3D11DeviceContext;

class USceneComponent : public UActorComponent
{
public:
	USceneComponent() = default;
	virtual ~USceneComponent() = default;

public:

	const FVector& GetPosition() const { return Position; }
	void SetPosition(const FVector& InPosition) { Position = InPosition; UpdateTransform(); }

	const FVector& GetRotation() const { return Rotation; }
	void SetRotation(const FVector& InRotation) { Rotation = InRotation;  UpdateTransform();}

	const FVector& GetScale() const { return Scale; }
	void SetScale(const FVector& InScale) { Scale = InScale;  UpdateTransform();}

	FVector GetComponentLocation() const;
	FVector GetForwardVector() const;
	FVector GetUpVector() const;
	FVector GetRightVector() const;

	FMatrix GetComponentTransform() const { return ComponentToWorld; }


public:

	// 렌더는 PrimitiveComponent만 수행 — 기본 구현은 비어있음
	virtual void Render(ID3D11DeviceContext* DeviceContext, const FMatrix& ViewProjection, ID3D11Buffer* ConstantBuffer) {}

protected:
	// TODO: Dirty Flag 추가 고려
	virtual void UpdateTransform();

protected:

	// 나중에 컴포넌트로 바꿀 예정
	// TODO: FTransform 사용
	FVector Position = { 0.0f, 0.0f, 0.0f };
	// TODO: Euler Angle이 아닌 Quaternion으로 회전 표현 고려
	FVector Rotation = { 0.0f, 0.0f, 0.0f };
	FVector Scale    = { 1.0f, 1.0f, 1.0f };

	// MVP 행렬의 Model 행렬에 해당
	FMatrix ComponentToWorld = FMatrix::Identity;
};

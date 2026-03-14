#pragma once
#include "ActorComponent.h"

class USceneComponent : public UActorComponent
{
public:
	USceneComponent() = default;
	virtual ~USceneComponent() = default;

public:
	const FVector& GetPosition() const { return Position; }
	void SetPosition(const FVector& InPosition) { Position = InPosition; }

	const FVector& GetRotation() const { return Rotation; }
	void SetRotation(const FVector& InRotation) { Rotation = InRotation; }

	const FVector& GetScale() const { return Scale; }
	void SetScale(const FVector& InScale) { Scale = InScale; }

public:
	virtual void TickComponent(float DeltaTime) override;

	// 렌더는 PrimitiveComponent만 수행 — 기본 구현은 비어있음
	virtual void Render(ID3D11DeviceContext& DeviceContext) {}

private:
	FVector Position = { 0.0f, 0.0f, 0.0f };
	FVector Rotation = { 0.0f, 0.0f, 0.0f };
	FVector Scale    = { 1.0f, 1.0f, 1.0f };
};


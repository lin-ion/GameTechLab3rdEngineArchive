#pragma once

#include "SceneComponent.h"

class UCameraComponent : public USceneComponent
{
public:
	UCameraComponent() = default;
	virtual ~UCameraComponent() = default;

public:
	float FOV = 60.0f;
	float AspectRatio = 1.0f;
	float NearPlane = 0.1f;
	float FarPlane = 100.0f;

public:
	// 매 프레임마다 한번만 호출
	FMatrix GetViewMatrix() const; // WorldToView
	FMatrix GetProjectionMatrix() const; // ViewToClip

	FVector GetCameraRayDirection();

	// TODO: Viewport 상호작용 추가


public:
	virtual void TickComponent(float Deltatime) override;
};


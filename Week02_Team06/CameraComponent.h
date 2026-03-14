#pragma once

#include "SceneComponent.h"

class UCameraComponent : public USceneComponent
{
public:
	UCameraComponent() = default;
	~UCameraComponent() = default;
public:
	float FOV = 60.0f;
	float AspectRatio = 1.0f;
	float NearPlane = 0.1f;
	float FarPlane = 100.0f;

public:
	// 매 프레임마다 한번만 호출
	FMatrix GetViewMatrix() const; // WorldToView
	FMatrix GetProjectionMatrix() const; // ViewToClip
	void SetAspectRatio(float InAspectRatio) { AspectRatio = InAspectRatio; };

	// TODO: Viewport 상호작용 추가
};


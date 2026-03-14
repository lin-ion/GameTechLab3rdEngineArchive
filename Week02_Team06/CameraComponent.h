#pragma once

#include "SceneComponent.h"

class UCameraComponent : public USceneComponent
{
public:
	UCameraComponent() = default;
	virtual ~UCameraComponent() = default;

private:
	float FOV = 60.0f;
	float AspectRatio = 1.0f;
	float NearPlane = 0.1f;
	float FarPlane = 1000.0f;
	bool bIsOrthogonal = false;

	bool bIsDragging = false;
	POINT PreviousMousePosition = { 0, 0 };

public:
	void TickComponent(float DeltaTime);
	// 매 프레임마다 한번만 호출
	FMatrix GetViewMatrix() const; // WorldToView
	FMatrix GetProjectionMatrix() const; // ViewToClip
	void SetAspectRatio(float InAspectRatio) { AspectRatio = InAspectRatio; };

	float GetFOV() const { return FOV; }
	void SetFOV(float InFOV) { FOV = InFOV; }
	float GetAspectRatio() const { return AspectRatio; }
	void SetAspectRatio(float InAspectRatio) { AspectRatio = InAspectRatio; }
	float GetNearPlane() const { return NearPlane; }
	void SetNearPlane(float InNearPlane) { NearPlane = InNearPlane; }
	float GetFarPlane() const { return FarPlane; }
	void SetFarPlane(float InFarPlane) { FarPlane = InFarPlane; }
	bool IsOrthogonal() const { return bIsOrthogonal; }
	void SetOrthogonal(bool bInIsOrthogonal) { bIsOrthogonal = bInIsOrthogonal; }

	FVector GetCameraRayDirection();

	// TODO: Viewport 상호작용 추가

public:
	virtual void TickComponent(float Deltatime) override;
};


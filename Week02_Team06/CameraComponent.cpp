#include "pch.h"
#include "CameraComponent.h"

FMatrix UCameraComponent::GetViewMatrix() const {
	FVector Eye = GetComponentLocation();
	FVector Forward = GetForwardVector();
	FVector Up = GetUpVector();

	FVector At = Eye + Forward;

	return FMatrix::MakeLookAt(Eye, At, Up);
}

FMatrix UCameraComponent::GetProjectionMatrix() const {
	return FMatrix::MakePerspective(FOV, AspectRatio, NearPlane, FarPlane);
}
#include "pch.h"
#include "CameraComponent.h"
#include "SceneComponent.h"
#include "Input.h"

void UCameraComponent::TickComponent(float DeltaTime)
{
	bool bMoved = false;

	if (UInput::GetInstance().IsKeyPressing('A')) { Position.X -= DeltaTime; UpdateTransform(); }
	if (UInput::GetInstance().IsKeyPressing('D')) { Position.X += DeltaTime; UpdateTransform(); }
	if (UInput::GetInstance().IsKeyPressing('S')) { Position.Y -= DeltaTime; UpdateTransform(); }
	if (UInput::GetInstance().IsKeyPressing('W')) { Position.Y += DeltaTime; UpdateTransform(); }

	// 값이 변했을 때만 행렬 갱신 공정 가동!
	//if (bMoved)
	//{
		//UpdateTransform();
	//}
}

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
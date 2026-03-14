#include "pch.h"
#include "CameraComponent.h"
#include "SceneComponent.h"
#include "Input.h"

FMatrix UCameraComponent::GetViewMatrix() const {
	FVector Eye = GetComponentLocation();
	FVector Forward = GetForwardVector();
	FVector Up = GetUpVector();

	FVector At = Eye + Forward;

	return FMatrix::MakeLookAt(Eye, At, Up);
}

FMatrix UCameraComponent::GetProjectionMatrix() const {
	if (bIsOrthogonal)
	{
		float OrthoWidth = 10.0f;
		float OrthoHeight = AspectRatio * OrthoWidth;
		return FMatrix::MakeOrthographic(OrthoWidth, OrthoHeight, NearPlane, FarPlane);
	}
	else {
		return FMatrix::MakePerspective(FOV, AspectRatio, NearPlane, FarPlane);
	}
}

/*월드 디렉션을 반환*/
FVector UCameraComponent::GetCameraRayDirection()
{
	POINT MousePostion = UInput::GetInstance().GetMousePosition();

	//far를 바라봐야함
	FVector NDC = {0.f ,0.f, 1.f};

	NDC.X = MousePostion.x * 2.f / WindowSizeWidth  - 1;
	NDC.Y = -MousePostion.y * 2.f / WindowSizeHeight + 1;

	//NDC 역투영 곱하기
	FMatrix ProjectionInverse = FMatrix::MakePerspective(FOV, AspectRatio, NearPlane, FarPlane).Inverse();

	FVector Eye = GetComponentLocation();
	FVector Forward = GetForwardVector();
	FVector Up = GetUpVector();
	FVector At = Eye + Forward;

	FMatrix ViewInverse = FMatrix::MakeLookAt(Eye, At, Up).Inverse();

	FVector ViewDirection = FMatrix::TransformNormal(NDC, ProjectionInverse);
	ViewDirection.Z = 1.f;

	FVector WorldDirection = FMatrix::TransformNormal(ViewDirection, ViewInverse);

	WorldDirection.Normalize();

	return WorldDirection;
}


void UCameraComponent::TickComponent(float Deltatime)
{
	//GetCameraRayDirection();

	if (UInput::GetInstance().IsKeyPressing('A'))
	{
		SetPosition(GetPosition() - GetRightVector() * Deltatime);
	}
	if (UInput::GetInstance().IsKeyPressing('D'))
	{
		SetPosition(GetPosition() + GetRightVector() * Deltatime);
	}
	if (UInput::GetInstance().IsKeyPressing('S'))
	{
		SetPosition(GetPosition() - GetForwardVector() * Deltatime);
	}
	if (UInput::GetInstance().IsKeyPressing('W'))
	{
		SetPosition(GetPosition() + GetForwardVector() * Deltatime);
	}
	if (UInput::GetInstance().IsKeyPressing(VK_CONTROL))
	{
		SetPosition(GetPosition() - GetUpVector() * Deltatime);
	}
	if (UInput::GetInstance().IsKeyPressing(VK_SPACE))
	{
		SetPosition(GetPosition() + GetUpVector() * Deltatime);
	}

	// Mouse Drag
	if (UInput::GetInstance().IsKeyDown(VK_RBUTTON))
	{
		PreviousMousePosition = UInput::GetInstance().GetMousePosition();
	}
	//if (UInput::GetInstance().IsKeyUp(VK_RBUTTON))
	if (UInput::GetInstance().IsKeyPressing(VK_RBUTTON))
	{
		POINT CurrentMousePosition = UInput::GetInstance().GetMousePosition();
		float DeltaMouseY = static_cast<float>(CurrentMousePosition.y - PreviousMousePosition.y);
		float DeltaMouseX = static_cast<float>(CurrentMousePosition.x - PreviousMousePosition.x);
		PreviousMousePosition = CurrentMousePosition;

		float RotationX = DeltaMouseY * 0.3f;
		float RotationY = DeltaMouseX * 0.3f;
		SetRotation(GetRotation()+FVector(RotationX, RotationY, 0.0f));
	}
}

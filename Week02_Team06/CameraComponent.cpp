#include "pch.h"
#include "CameraComponent.h"
#include "Input.h"

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
	GetCameraRayDirection();
}

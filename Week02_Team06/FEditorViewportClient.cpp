#include "pch.h"
#include "FEditorViewportClient.h"
#include "Input.h"

FEditorViewportClient::FEditorViewportClient()
{
	ViewTransform.SetLocation({ 10.f, 10.f ,10.f });
	ViewTransform.SetRotation({ 37.f, -121.f, 0.f });
}

FMatrix FEditorViewportClient::GetViewMatrix() const
{
	FVector Eye = ViewTransform.GetLocation();
	FVector Forward = ViewTransform.GetForwardVector();
	FVector Up = ViewTransform.GetUpVector();

	FVector At = Eye + Forward;

	return FMatrix::MakeLookAt(Eye, At, Up);
}

FMatrix FEditorViewportClient::GetProjectionMatrix() const
{
	if (isPerspective)
	{
		return FMatrix::MakePerspective(FOVAngle, AspectRatio, NearPlane, FarPlane);
	}
	else
	{
		float OrthoWidth = 10.0f;
		float OrthoHeight = AspectRatio * OrthoWidth;
		return FMatrix::MakeOrthographic(OrthoWidth, OrthoHeight, NearPlane, FarPlane);
	}
}

FVector FEditorViewportClient::GetCameraRayDirection()
{
	POINT MousePostion = UInput::GetInstance().GetMousePosition();

	//far를 바라봐야함
	FVector NDC = { 0.f ,0.f, 1.f };

	NDC.X = MousePostion.x * 2.f / WindowSizeWidth - 1;
	NDC.Y = -MousePostion.y * 2.f / WindowSizeHeight + 1;

	FVector Eye = ViewTransform.GetLocation();
	FVector Forward = ViewTransform.GetForwardVector();
	FVector Up = ViewTransform.GetUpVector();

	FVector At = Eye + Forward;

	//NDC 역투영 곱하기
	FMatrix ProjectionInverse = FMatrix::MakePerspective(FOVAngle, AspectRatio, NearPlane, FarPlane).Inverse();

	FMatrix ViewInverse = FMatrix::MakeLookAt(Eye, At, Up).Inverse();

	FVector ViewDirection = FMatrix::TransformNormal(NDC, ProjectionInverse);
	ViewDirection.Z = 1.f;

	FVector WorldDirection = FMatrix::TransformNormal(ViewDirection, ViewInverse);

	WorldDirection.Normalize();

	return WorldDirection;
}

void FEditorViewportClient::Tick(float DeltaTime) {
	FViewportCameraTransform& Transform = GetViewTransform();
	FVector CurrentLocation = Transform.GetLocation();

	if (UInput::GetInstance().IsKeyPressing('A'))
	{
		CurrentLocation = CurrentLocation - Transform.GetRightVector() * DeltaTime * 10.f;
	}
	if (UInput::GetInstance().IsKeyPressing('D'))
	{
		CurrentLocation = CurrentLocation + Transform.GetRightVector() * DeltaTime * 10.f;
	}
	if (UInput::GetInstance().IsKeyPressing('S'))
	{
		CurrentLocation = CurrentLocation - Transform.GetForwardVector() * DeltaTime * 10.f;
	}
	if (UInput::GetInstance().IsKeyPressing('W'))
	{
		CurrentLocation = CurrentLocation + Transform.GetForwardVector() * DeltaTime * 10.f;
	}
	ViewTransform.SetLocation(CurrentLocation);

	// Mouse Drag
	if (UInput::GetInstance().IsKeyDown(VK_RBUTTON))
	{
		PreviousMousePosition = UInput::GetInstance().GetMousePosition();
	}
	//if (UInput::GetInstance().IsKeyUp(VK_RBUTTON))
	if (UInput::GetInstance().IsKeyPressing(VK_RBUTTON))
	{
		POINT CurrentMousePosition = UInput::GetInstance().GetMousePosition();
		float DeltaMouseY = static_cast<float>(CurrentMousePosition.y - PreviousMousePosition.y); // Pitch
		float DeltaMouseX = static_cast<float>(CurrentMousePosition.x - PreviousMousePosition.x); // Yaw
		PreviousMousePosition = CurrentMousePosition;

		float Sensitivity = 0.2f;
		FVector Rotation = GetViewRotation();
		Rotation.X = std::clamp(Rotation.X + (DeltaMouseY * Sensitivity), -89.0f, 89.0f);
		Rotation.Y = Rotation.Y + (DeltaMouseX * Sensitivity);
		SetViewRotation(Rotation);
	}

}

FVector FViewportCameraTransform::GetRightVector() const
{
	FMatrix RotationMatrix = FMatrix::MakeRotation(ViewRotation);
	FVector Right = { RotationMatrix.M[0][0], RotationMatrix.M[0][1], RotationMatrix.M[0][2] };
	// TODO: Normalize가 실제로 필요한지 확인 필요
	Right.Normalize();
	return Right;
}

FVector FViewportCameraTransform::GetUpVector() const
{
	FMatrix RotationMatrix = FMatrix::MakeRotation(ViewRotation);
	FVector Up = { RotationMatrix.M[1][0], RotationMatrix.M[1][1], RotationMatrix.M[1][2] };
	Up.Normalize();
	return Up;
}

FVector FViewportCameraTransform::GetForwardVector() const
{
	FMatrix RotationMatrix = FMatrix::MakeRotation(ViewRotation);
	FVector Forward = { RotationMatrix.M[2][0], RotationMatrix.M[2][1], RotationMatrix.M[2][2] };
	Forward.Normalize();
	return Forward;
}
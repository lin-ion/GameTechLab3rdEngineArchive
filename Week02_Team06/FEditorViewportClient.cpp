#include "pch.h"
#include "FEditorViewportClient.h"
#include "Input.h"

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
	if (bIsPerspective)
	{
		return FMatrix::MakePerspective(FOVAngle, AspectRatio, NearPlane, FarPlane);
	}
	else
	{
		float OrthoHeight = ViewTransform.GetOrthoSize();
		float OrthoWidth = OrthoHeight * AspectRatio;
		return FMatrix::MakeOrthographic(OrthoWidth, OrthoHeight, NearPlane, FarPlane);
	}
}

void FEditorViewportClient::SetPerspective(bool bInIsPerspective)
{
	if (bIsPerspective == bInIsPerspective)
	{
		return;
	}

	// Orthographic to Perspective
	if (bInIsPerspective)
	{
		// D = S / (2 * tan(FOV / 2))
		float HalfFOVRadian = Math::ToRadians(FOVAngle) / 2.0f;
		float NewDistance = ViewTransform.GetOrthoSize() / (2.0f * tanf(HalfFOVRadian));
		FVector NewViewLocation = ViewTransform.GetPivotLocation() - ViewTransform.GetForwardVector() * NewDistance;
		ViewTransform.SetLocation(NewViewLocation);
		ViewTransform.SetDistance(NewDistance);
	}
	// Perspective to Orthographic
	else
	{
		// S = 2 * D * tan(FOV / 2)
		float HalfFOVRadian = Math::ToRadians(FOVAngle) / 2.0f;
		float NewOrthoSize = 2.0f * ViewTransform.GetDistance() * tanf(HalfFOVRadian);
		ViewTransform.SetOrthoSize(NewOrthoSize);
	}

	bIsPerspective = bInIsPerspective;
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
	HandleKeyboardMovement(DeltaTime);
	HandleMouseRightDrag();
	HandleMouseWheel();
}

void FEditorViewportClient::HandleKeyboardMovement(float DeltaTime)
{
	UInput& Input = UInput::GetInstance();

	FVector MovementDirection = { 0.0f, 0.0f, 0.0f };
	if (Input.IsKeyPressing('A'))
	{
		MovementDirection = MovementDirection - ViewTransform.GetRightVector();
	}
	if (Input.IsKeyPressing('D'))
	{
		MovementDirection = MovementDirection + ViewTransform.GetRightVector();
	}
	if (Input.IsKeyPressing('S'))
	{
		MovementDirection = MovementDirection - ViewTransform.GetForwardVector();
	}
	if (Input.IsKeyPressing('W'))
	{
		MovementDirection = MovementDirection + ViewTransform.GetForwardVector();
	}
	MovementDirection.Normalize();

	constexpr float MovementSpeed = 5.0f;
	FVector MovementLocation = ViewTransform.GetLocation() + MovementDirection * DeltaTime * MovementSpeed;
	ViewTransform.SetLocation(MovementLocation);
}

void FEditorViewportClient::HandleMouseRightDrag()
{
	UInput& Input = UInput::GetInstance();

	if (!Input.IsKeyPressing(VK_RBUTTON))
	{
		return;
	}

	POINT MouseDelta = Input.GetMousePositionDelta();
	bool bIsOrbiting = Input.IsKeyPressing(VK_LMENU);

	float DeltaMouseY = static_cast<float>(MouseDelta.y); // Pitch
	float DeltaMouseX = static_cast<float>(MouseDelta.x); // Yaw

	float RotationSpeed = 0.2f;
	FVector Rotation = ViewTransform.GetRotation();
	Rotation.X = std::clamp(Rotation.X + (DeltaMouseY * RotationSpeed), -89.0f, 89.0f);
	Rotation.Y = Rotation.Y + (DeltaMouseX * RotationSpeed);

	if (bIsOrbiting)
	{
		FVector OldPivotLocation = ViewTransform.GetPivotLocation();
		ViewTransform.SetRotation(Rotation);
		FVector NewPivotLocation = ViewTransform.GetPivotLocation();
		ViewTransform.SetLocation(ViewTransform.GetLocation() + (OldPivotLocation - NewPivotLocation));
	}
	else
	{
		ViewTransform.SetRotation(Rotation);
	}
}

void FEditorViewportClient::HandleMouseWheel()
{
	UInput& Input = UInput::GetInstance();
	float MouseWheelDelta = Input.GetMouseWheelDelta();

	if (MouseWheelDelta == 0.0f)
	{
		return;
	}

	if (bIsPerspective)
	{
		constexpr float PerspectiveZoomSpeed = 1.0f;
		float NewDistance = ViewTransform.GetDistance() - MouseWheelDelta * PerspectiveZoomSpeed;
		NewDistance = (std::max)(NewDistance, 1.0f);
		FVector NewViewLocation = ViewTransform.GetPivotLocation() - ViewTransform.GetForwardVector() * NewDistance;
		ViewTransform.SetLocation(NewViewLocation);
		ViewTransform.SetDistance(NewDistance);
		UE_LOG(("Distance: " + std::to_string(ViewTransform.GetDistance())).c_str());
	}
	else
	{
		constexpr float OrthoZoomSpeed = 1.0f;
		float NewOrthoSize = ViewTransform.GetOrthoSize() - MouseWheelDelta * OrthoZoomSpeed;
		NewOrthoSize = (std::max)(NewOrthoSize, 1.0f);
		ViewTransform.SetOrthoSize(NewOrthoSize);
		UE_LOG(("OrthoSize: " + std::to_string(ViewTransform.GetOrthoSize())).c_str());
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

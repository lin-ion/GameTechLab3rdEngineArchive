#include "pch.h"
#include "FEditorViewportClient.h"
#include "PickingComponent.h"
#include "GizmoComponent.h"
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
	POINT MousePos = UInput::GetInstance().GetMousePosition();

	// 1. Client 좌표로 변환 (이미 검증됨)
	HWND hWnd = GetActiveWindow();
	ScreenToClient(hWnd, &MousePos);

	// 2. NDC 좌표 계산 (정중앙이 0,0이 되도록)
	float NDCX = (2.0f * MousePos.x) / WindowSizeWidth - 1.0f;
	float NDCY = 1.0f - (2.0f * MousePos.y) / WindowSizeHeight;

	// 3. 역행렬 준비 (View * Proj의 역행렬 하나로 통합 가능)
	FMatrix InvViewProj = (GetViewMatrix() * GetProjectionMatrix()).Inverse();

	// 4. [핵심] 근평면과 원평면의 두 점을 월드 공간으로 복원
	// TransformCoord는 W 성분을 1로 취급하고 결과에서 W로 나누어 투영을 해제합니다.
	FVector NearNDC = { NDCX, NDCY, 0.0f };
	FVector FarNDC = { NDCX, NDCY, 1.0f };

	FVector WorldNear = FMatrix::TransformCoord(NearNDC, InvViewProj);
	FVector WorldFar = FMatrix::TransformCoord(FarNDC, InvViewProj);

	// 5. 방향 벡터 산출 및 정규화
	FVector WorldDirection = WorldFar - WorldNear;
	WorldDirection.Normalize();

	return WorldDirection;
}

void FEditorViewportClient::Tick(float DeltaTime) {
	UInput& Input = UInput::GetInstance();

	FVector MovementDirection = { 0.f, 0.f, 0.f };
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

	constexpr float MovementSpeed = 5.f;
	FVector MovementLocation = ViewTransform.GetLocation() + MovementDirection * DeltaTime * MovementSpeed;
	ViewTransform.SetLocation(MovementLocation);

	// Mouse Drag (Right Click)
	if (Input.IsKeyPressing(VK_RBUTTON))
	{
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
		else {
			ViewTransform.SetRotation(Rotation);
		}
	}

	// Mouse Wheel Zoom
	float MouseWheelDelta = Input.GetMouseWheelDelta();
	if (MouseWheelDelta != 0.0f)
	{
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
		else {
			constexpr float OrthoZoomSpeed = 1.0f;
			float NewOrthoSize = ViewTransform.GetOrthoSize() - MouseWheelDelta * OrthoZoomSpeed;
			NewOrthoSize = (std::max)(NewOrthoSize, 1.0f);
			ViewTransform.SetOrthoSize(NewOrthoSize);
			UE_LOG(("OrthoSize: " + std::to_string(ViewTransform.GetOrthoSize())).c_str());
		}
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
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
	FVector MovementDirection = { 0.f, 0.f, 0.f };
	if (UInput::GetInstance().IsKeyPressing('A'))
	{
		MovementDirection = MovementDirection - ViewTransform.GetRightVector();
	}
	if (UInput::GetInstance().IsKeyPressing('D'))
	{
		MovementDirection = MovementDirection + ViewTransform.GetRightVector();
	}
	if (UInput::GetInstance().IsKeyPressing('S'))
	{
		MovementDirection = MovementDirection - ViewTransform.GetForwardVector();
	}
	if (UInput::GetInstance().IsKeyPressing('W'))
	{
		MovementDirection = MovementDirection + ViewTransform.GetForwardVector();
	}
	MovementDirection.Normalize();

	constexpr float MovementSpeed = 5.f;
	FVector MovementLocation = ViewTransform.GetLocation() + MovementDirection * DeltaTime * MovementSpeed;
	ViewTransform.SetLocation(MovementLocation);

	// Gizmo Picking Test용
	if (UInput::GetInstance().IsKeyDown(VK_LBUTTON))
	{
		// 임시 피킹 부품을 생성하여 검수를 의뢰합니다.
		static UPickingComponent TestPicker;

		if (MainGizmo)
		{
			EGizmoAxis Picked = MainGizmo->CheckGizmoPicking(&TestPicker);

			// 결과에 따른 로그 출력 (Visual Studio 출력창에서 확인 가능)
			if (Picked != EGizmoAxis::None)
			{
				std::string AxisName[] = { "None", "Center", "X", "Y", "Z" };
				std::string Msg = "Gizmo Picked: " + AxisName[(int)Picked] + "\n";
				OutputDebugStringA(Msg.c_str());
			}
		}
	}
	// 여기까지 Test용

	// Mouse Drag
	if (UInput::GetInstance().IsKeyDown(VK_RBUTTON))
	{
		PreviousMousePosition = UInput::GetInstance().GetMousePosition();
	}
	if (UInput::GetInstance().IsKeyPressing(VK_RBUTTON))
	{
		POINT CurrentMousePosition = UInput::GetInstance().GetMousePosition();
		float DeltaMouseY = static_cast<float>(CurrentMousePosition.y - PreviousMousePosition.y); // Pitch
		float DeltaMouseX = static_cast<float>(CurrentMousePosition.x - PreviousMousePosition.x); // Yaw
		PreviousMousePosition = CurrentMousePosition;

		// TODO: DeltaMouse는 해상도에 비례하므로 다양한 해상도에서 감도 실험 필요
		float RotationSpeed = 0.2f;
		FVector Rotation = ViewTransform.GetRotation();
		Rotation.X = std::clamp(Rotation.X + (DeltaMouseY * RotationSpeed), -89.0f, 89.0f);
		Rotation.Y = Rotation.Y + (DeltaMouseX * RotationSpeed);
		ViewTransform.SetRotation(Rotation);
	}

	// Mouse Wheel Zoom
	float MouseWheelDelta = UInput::GetInstance().GetMouseWheelDelta();
	if (UInput::GetInstance().GetMouseWheelDelta() != 0.0f)
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
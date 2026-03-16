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
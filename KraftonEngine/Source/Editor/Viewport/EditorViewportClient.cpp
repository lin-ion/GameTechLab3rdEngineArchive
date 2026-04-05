#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/EditorEngine.h"
#include "Editor/Selection/PickingService.h"
#include "Editor/Selection/PickingPerf.h"
#include "Editor/Settings/EditorSettings.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "Component/CameraComponent.h"
#include "Viewport/Viewport.h"
#include "GameFramework/World.h"
#include "Component/GizmoComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Collision/RayUtils.h"
#include "Object/Object.h"
#include "Editor/Selection/SelectionManager.h"
#include "ImGui/imgui.h"

#include <cmath>

void FEditorViewportClient::Initialize(FWindowsWindow* InWindow)
{
	Window = InWindow;
}

void FEditorViewportClient::SetWorld(UWorld* InWorld)
{
	if (World == InWorld)
	{
		return;
	}

	World = InWorld;
	ResetIdPickingState();
}

void FEditorViewportClient::CreateCamera()
{
	DestroyCamera();
	Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
}

void FEditorViewportClient::DestroyCamera()
{
	if (Camera)
	{
		UObjectManager::Get().DestroyObject(Camera);
		Camera = nullptr;
	}
}

void FEditorViewportClient::ResetCamera()
{
	if (!Camera || !Settings) return;
	Camera->SetWorldLocation(Settings->InitViewPos);
	Camera->LookAt(Settings->InitLookAt);
}

void FEditorViewportClient::SetViewportType(ELevelViewportType NewType)
{
	if (!Camera) return;

	RenderOptions.ViewportType = NewType;

	if (NewType == ELevelViewportType::Perspective)
	{
		Camera->SetOrthographic(false);
		return;
	}

	// FreeOrthographic: 현재 카메라 위치/회전 유지, 투영만 Ortho로 전환
	if (NewType == ELevelViewportType::FreeOrthographic)
	{
		Camera->SetOrthographic(true);
		return;
	}

	// 고정 방향 Orthographic: 카메라를 프리셋 방향으로 설정
	Camera->SetOrthographic(true);

	constexpr float OrthoDistance = 50.0f;
	FVector Position = FVector(0, 0, 0);
	FVector Rotation = FVector(0, 0, 0); // (Roll, Pitch, Yaw)

	switch (NewType)
	{
	case ELevelViewportType::Top:
		Position = FVector(0, 0, OrthoDistance);
		Rotation = FVector(0, 90.0f, 0);	// Pitch down (positive pitch = look -Z)
		break;
	case ELevelViewportType::Bottom:
		Position = FVector(0, 0, -OrthoDistance);
		Rotation = FVector(0, -90.0f, 0);	// Pitch up (negative pitch = look +Z)
		break;
	case ELevelViewportType::Front:
		Position = FVector(OrthoDistance, 0, 0);
		Rotation = FVector(0, 0, 180.0f);	// Yaw to look -X
		break;
	case ELevelViewportType::Back:
		Position = FVector(-OrthoDistance, 0, 0);
		Rotation = FVector(0, 0, 0.0f);		// Yaw to look +X
		break;
	case ELevelViewportType::Left:
		Position = FVector(0, -OrthoDistance, 0);
		Rotation = FVector(0, 0, 90.0f);	// Yaw to look +Y
		break;
	case ELevelViewportType::Right:
		Position = FVector(0, OrthoDistance, 0);
		Rotation = FVector(0, 0, -90.0f);	// Yaw to look -Y
		break;
	default:
		break;
	}

	Camera->SetRelativeLocation(Position);
	Camera->SetRelativeRotation(Rotation);
}

void FEditorViewportClient::SetViewportSize(float InWidth, float InHeight)
{
	if (InWidth > 0.0f)
	{
		WindowWidth = InWidth;
	}

	if (InHeight > 0.0f)
	{
		WindowHeight = InHeight;
	}

	if (Camera)
	{
		Camera->OnResize(static_cast<int32>(WindowWidth), static_cast<int32>(WindowHeight));
	}
}

void FEditorViewportClient::Tick(float DeltaTime)
{
	if (!bIsActive) return;

	TickInput(DeltaTime);
	TickInteraction(DeltaTime);
}

void FEditorViewportClient::TickInput(float DeltaTime)
{
	if (!Camera)
	{
		return;
	}

	if (InputSystem::Get().GetGuiInputState().bUsingKeyboard == true)
	{
		return;
	}

	const FCameraState& CameraState = Camera->GetCameraState();
	const bool bIsOrtho = CameraState.bIsOrthogonal;

	const float MoveSensitivity = RenderOptions.CameraMoveSensitivity;
	const float CameraSpeed = (Settings ? Settings->CameraSpeed : 10.f) * MoveSensitivity;

	if (!bIsOrtho)
	{
		// ── Perspective: 기존 WASDQE 이동 ──
		FVector Move = FVector(0, 0, 0);

		if (InputSystem::Get().GetKey('W'))
			Move.X += CameraSpeed;
		if (InputSystem::Get().GetKey('A'))
			Move.Y -= CameraSpeed;
		if (InputSystem::Get().GetKey('S'))
			Move.X -= CameraSpeed;
		if (InputSystem::Get().GetKey('D'))
			Move.Y += CameraSpeed;
		if (InputSystem::Get().GetKey('Q'))
			Move.Z -= CameraSpeed;
		if (InputSystem::Get().GetKey('E'))
			Move.Z += CameraSpeed;

		Move *= DeltaTime;
		Camera->MoveLocal(Move);

		// ── Perspective: 키보드 회전 ──
		FVector Rotation = FVector(0, 0, 0);

		const float RotateSensitivity = RenderOptions.CameraRotateSensitivity;
		const float AngleVelocity = (Settings ? Settings->CameraRotationSpeed : 60.f) * RotateSensitivity;
		if (InputSystem::Get().GetKey(VK_UP))
			Rotation.Z -= AngleVelocity;
		if (InputSystem::Get().GetKey(VK_LEFT))
			Rotation.Y -= AngleVelocity;
		if (InputSystem::Get().GetKey(VK_DOWN))
			Rotation.Z += AngleVelocity;
		if (InputSystem::Get().GetKey(VK_RIGHT))
			Rotation.Y += AngleVelocity;

		// ── Perspective: 마우스 우클릭 → 회전 ──
		FVector MouseRotation = FVector(0, 0, 0);
		float MouseRotationSpeed = 0.15f * RotateSensitivity;

		if (InputSystem::Get().GetKey(VK_RBUTTON))
		{
			float DeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
			float DeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());

			MouseRotation.Y += DeltaX * MouseRotationSpeed;
			MouseRotation.Z += DeltaY * MouseRotationSpeed;

			MouseRotation.Y = Clamp(MouseRotation.Y, -89.0f, 89.0f);
			MouseRotation.Z = Clamp(MouseRotation.Z, -89.0f, 89.0f);
		}

		Rotation *= DeltaTime;
		Camera->Rotate(Rotation.Y + MouseRotation.Y, Rotation.Z + MouseRotation.Z);
	}
	else
	{
		// ── Orthographic: 마우스 우클릭 드래그 → 평행이동 (Pan) ──
		if (InputSystem::Get().GetKey(VK_RBUTTON))
		{
			float DeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
			float DeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());

			// OrthoWidth 기준으로 감도 스케일 (줌 레벨에 비례)
			float PanScale = CameraState.OrthoWidth * 0.002f * MoveSensitivity;

			// 카메라 로컬 Right/Up 방향으로 이동
			Camera->MoveLocal(FVector(0, -DeltaX * PanScale, DeltaY * PanScale));
		}
	}

	if (InputSystem::Get().GetKeyUp(VK_SPACE))
		Gizmo->SetNextMode();
}

void FEditorViewportClient::TickInteraction(float DeltaTime)
{
	(void)DeltaTime;
	ProcessPendingIdPickResult();

	if (!Camera || !Gizmo || !World)
	{
		return;
	}

	Gizmo->ApplyScreenSpaceScaling(Camera->GetWorldLocation(),
		Camera->IsOrthogonal(), Camera->GetOrthoWidth());

	Gizmo->UpdateAxisMask(RenderOptions.ViewportType);

	// 기즈모 드래그 중에는 마우스가 뷰포트 밖으로 나가도 드래그 종료를 처리해야 함
	if (InputSystem::Get().GetGuiInputState().bUsingMouse && !Gizmo->IsHolding())
	{
		return;
	}

	const float ZoomSpeed = Settings ? Settings->CameraZoomSpeed : 300.f;

	float ScrollNotches = InputSystem::Get().GetScrollNotches();
	if (ScrollNotches != 0.0f) {
		if (Camera->IsOrthogonal()) {
			float NewWidth = Camera->GetOrthoWidth() - ScrollNotches * ZoomSpeed * DeltaTime;
			Camera->SetOrthoWidth(Clamp(NewWidth, 0.1f, 1000.0f));
		}
		else {
			constexpr float FovStep = 2.0f * DEG_TO_RAD; // 노치당 2도
			float NewFOV = Camera->GetFOV() - ScrollNotches * FovStep;
			Camera->SetFOV(Clamp(NewFOV, 1.f * DEG_TO_RAD, 90.0f * DEG_TO_RAD));
		}
	}

	// 마우스 좌표를 뷰포트 슬롯 로컬 좌표로 변환
	// (ImGui screen space = 윈도우 클라이언트 좌표)
	POINT MousePoint = InputSystem::Get().GetMousePos();
	MousePoint = Window->ScreenToClientPoint(MousePoint);

	float LocalMouseX = static_cast<float>(MousePoint.x) - ViewportScreenRect.X;
	float LocalMouseY = static_cast<float>(MousePoint.y) - ViewportScreenRect.Y;

	// 커서 숨김 제거: ShowCursor는 전역 레퍼런스 카운터라 멀티 뷰포트에서
	// active 전환 시 GetKeyUp이 처리되지 않아 커서가 영구 숨김될 수 있음

	// FViewport 크기 기준으로 디프로젝션 (슬롯 크기와 동기화됨)
	float VPWidth = Viewport ? static_cast<float>(Viewport->GetWidth()) : WindowWidth;
	float VPHeight = Viewport ? static_cast<float>(Viewport->GetHeight()) : WindowHeight;
	FRay Ray = Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
	FHitResult HitResult;

	//Gizmo Hover
	FRayUtils::RaycastComponent(Gizmo, Ray, HitResult);

	if (InputSystem::Get().GetKeyDown(VK_LBUTTON))
	{
		HandleDragStart(Ray, LocalMouseX, LocalMouseY);
	}
	else if (InputSystem::Get().GetLeftDragging())
	{
		//	눌려있고, Holding되지 않았다면 다음 Loop부터 드래그 업데이트 시작
		if (Gizmo->IsPressedOnHandle() && !Gizmo->IsHolding())
		{
			Gizmo->SetHolding(true);
		}

		if (Gizmo->IsHolding())
		{
			Gizmo->UpdateDrag(Ray);
			InvalidateIdBufferCache();
		}
	}
	else if (InputSystem::Get().GetLeftDragEnd())
	{
		Gizmo->DragEnd();
	}
	else if (InputSystem::Get().GetKeyUp(VK_LBUTTON))
	{
		// 드래그 threshold 미달로 DragEnd가 호출되지 않는 경우 처리
		Gizmo->SetPressedOnHandle(false);
	}
}

void FEditorViewportClient::HandleDragStart(const FRay& Ray, float LocalMouseX, float LocalMouseY)
{
	EPickingMode PickingMode = EPickingMode::RayTriangleBVH;
	if (UEditorEngine* Editor = Cast<UEditorEngine>(GEngine))
	{
		PickingMode = Editor->GetPickingMode();
	}

	if (PickingMode == EPickingMode::IDBuffer)
	{
		CancelPendingIdPickReadback();
		bPendingIdPickCtrlHeld = InputSystem::Get().GetKey(VK_CONTROL);
		const float MaxX = (Viewport && Viewport->GetWidth() > 0) ? static_cast<float>(Viewport->GetWidth() - 1) : 0.0f;
		const float MaxY = (Viewport && Viewport->GetHeight() > 0) ? static_cast<float>(Viewport->GetHeight() - 1) : 0.0f;
		PendingIdPickX = static_cast<uint32>(Clamp(LocalMouseX, 0.0f, MaxX));
		PendingIdPickY = static_cast<uint32>(Clamp(LocalMouseY, 0.0f, MaxY));

		FHitResult GizmoHitResult{};
		if (FRayUtils::RaycastComponent(Gizmo, Ray, GizmoHitResult))
		{
			Gizmo->SetPressedOnHandle(true);
			return;
		}

		bPendingIdPickRequest = true;
		return;
	}

	const bool bMeasureRayPick = (PickingMode == EPickingMode::RayTriangleBVH);
	const uint64 RayPickStartCycles = bMeasureRayPick ? FPickingPlatformTime::Cycles64() : 0;

	auto RecordRayPickIfNeeded = [&]()
	{
		if (!bMeasureRayPick) return;
		const uint64 EndCycles = FPickingPlatformTime::Cycles64();
		FPickingPerf::Record(EPickingMode::RayTriangleBVH, EndCycles - RayPickStartCycles);
	};

	FHitResult HitResult{};
	if (FPickingService::PickGizmo(Gizmo, Ray, PickingMode, HitResult))
	{
		Gizmo->SetPressedOnHandle(true);
		RecordRayPickIfNeeded();
	}
	else
	{
		float ClosestDistance = FLT_MAX;
		AActor* BestActor = FPickingService::PickActor(World, Ray, PickingMode, ClosestDistance);

		bool bCtrlHeld = InputSystem::Get().GetKey(VK_CONTROL);

		if (BestActor == nullptr)
		{
			if (!bCtrlHeld)
			{
				SelectionManager->ClearSelection();
			}
		}
		else
		{
			if (bCtrlHeld)
			{
				SelectionManager->ToggleSelect(BestActor);
			}
			else
			{
				SelectionManager->Select(BestActor);
			}
		}

		RecordRayPickIfNeeded();
	}
}

void FEditorViewportClient::ProcessPendingIdPickResult()
{
	if (!bHasPendingIdPickResult || !SelectionManager)
	{
		return;
	}

	bHasPendingIdPickResult = false;

	UObject* PickedObject = FPickingService::ResolveObjectFromPickingId(PendingPickedObjectId);
	if (PickedObject == Gizmo)
	{
		Gizmo->SetPressedOnHandle(true);
		return;
	}

	AActor* PickedActor = Cast<AActor>(PickedObject);

	if (!PickedActor)
	{
		if (UPrimitiveComponent* PickedComp = Cast<UPrimitiveComponent>(PickedObject))
		{
			PickedActor = PickedComp->GetOwner();
		}
	}

	if (!PickedActor)
	{
		if (!bPendingIdPickCtrlHeld)
		{
			SelectionManager->ClearSelection();
		}
		return;
	}

	if (bPendingIdPickCtrlHeld)
	{
		SelectionManager->ToggleSelect(PickedActor);
	}
	else
	{
		SelectionManager->Select(PickedActor);
	}
}

void FEditorViewportClient::CancelPendingIdPickReadback()
{
	if (Viewport && PendingIdPickReadbackRequestId != 0u)
	{
		Viewport->CancelPickingIdReadback(PendingIdPickReadbackRequestId);
	}

	bPendingIdPickReadback = false;
	PendingIdPickReadbackRequestId = 0u;
}

void FEditorViewportClient::ResetIdPickingState()
{
	CancelPendingIdPickReadback();
	bPendingIdPickRequest = false;
	bHasPendingIdPickResult = false;
	PendingPickedObjectId = 0u;
	bPendingIdPickCtrlHeld = false;
	PendingIdPickX = 0u;
	PendingIdPickY = 0u;
	LastIdBufferRenderCycles = 0u;
	InvalidateIdBufferCache();
}

bool FEditorViewportClient::IsIdBufferCacheValidForCurrentCamera() const
{
	if (!bHasCachedIdPickResult || !Camera)
	{
		return false;
	}

	const bool bSameProjection = (Camera->IsOrthogonal() == bCachedIdPickCameraOrtho)
		&& (std::abs(Camera->GetFOV() - CachedIdPickCameraFOV) <= 1e-4f)
		&& (std::abs(Camera->GetOrthoWidth() - CachedIdPickCameraOrthoWidth) <= 1e-4f);
	const bool bSameView = FVector::Distance(Camera->GetWorldLocation(), CachedIdPickCameraLocation) <= 1e-4f
		&& FVector::Distance(Camera->GetForwardVector(), CachedIdPickCameraForward) <= 1e-4f;

	return bSameProjection && bSameView;
}

bool FEditorViewportClient::ShouldRenderPendingIdPick() const
{
	if (bPendingIdPickRequest)
	{
		return true;
	}

	if (!bIdBufferDirty)
	{
		return false;
	}

	if (LastIdBufferRenderCycles == 0)
	{
		return true;
	}

	const uint64 NowCycles = FPickingPlatformTime::Cycles64();
	const double ElapsedMs = FPickingPlatformTime::ToMilliseconds(NowCycles - LastIdBufferRenderCycles);
	return ElapsedMs >= static_cast<double>(IdBufferUpdateIntervalMs);
}

void FEditorViewportClient::RefreshIdBufferDirtyStateFromCamera()
{
	UpdateIdBufferDirtyFromCamera();
}

void FEditorViewportClient::UpdateIdBufferDirtyFromCamera()
{
	if (!Camera)
	{
		bIdBufferDirty = true;
		return;
	}

	bIdBufferDirty = !IsIdBufferCacheValidForCurrentCamera();
}

void FEditorViewportClient::UpdateIdBufferCacheCameraState()
{
	if (!Camera)
	{
		bHasCachedIdPickResult = false;
		bIdBufferDirty = true;
		return;
	}

	bHasCachedIdPickResult = true;
	CachedIdPickCameraLocation = Camera->GetWorldLocation();
	CachedIdPickCameraForward = Camera->GetForwardVector();
	bCachedIdPickCameraOrtho = Camera->IsOrthogonal();
	CachedIdPickCameraFOV = Camera->GetFOV();
	CachedIdPickCameraOrthoWidth = Camera->GetOrthoWidth();
	LastIdBufferRenderCycles = FPickingPlatformTime::Cycles64();
	bIdBufferDirty = false;
}

void FEditorViewportClient::UpdateLayoutRect()
{
	if (!LayoutWindow) return;

	const FRect& R = LayoutWindow->GetRect();
	ViewportScreenRect = R;

	// FViewport 리사이즈 요청 (슬롯 크기와 RT 크기 동기화)
	if (Viewport)
	{
		uint32 SlotW = static_cast<uint32>(R.Width);
		uint32 SlotH = static_cast<uint32>(R.Height);
		if (SlotW > 0 && SlotH > 0 && (SlotW != Viewport->GetWidth() || SlotH != Viewport->GetHeight()))
		{
			Viewport->RequestResize(SlotW, SlotH);
		}
	}
}

void FEditorViewportClient::RenderViewportImage(bool bIsActiveViewport)
{
	if (!Viewport || !Viewport->GetSRV()) return;

	const FRect& R = ViewportScreenRect;
	if (R.Width <= 0 || R.Height <= 0) return;

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 Min(R.X, R.Y);
	ImVec2 Max(R.X + R.Width, R.Y + R.Height);

	DrawList->AddImage((ImTextureID)Viewport->GetSRV(), Min, Max);

	// 활성 뷰포트 테두리 강조
	if (bIsActiveViewport)
	{
		DrawList->AddRect(Min, Max, IM_COL32(255, 200, 0, 200), 0.0f, 0, 2.0f);
	}
}

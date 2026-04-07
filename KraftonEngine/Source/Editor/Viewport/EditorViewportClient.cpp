#include "Editor/Viewport/EditorViewportClient.h"

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/EditorEngine.h"
#include "Editor/Selection/PickingService.h"
#include "Editor/Settings/EditorSettings.h"
#include "Profiling/PlatformTime.h"
#include "Profiling/Stats.h"
#include "Engine/Input/InputSystem.h"
#include "Engine/Runtime/Engine.h"
#include "Engine/Runtime/WindowsWindow.h"

#include "Viewport/Viewport.h"
#include "GameFramework/World.h"
#include "GameFramework/Level.h"
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

	EndDeferredSpatialIndexInvalidation();

	World = InWorld;
	if (World)
	{
		if (ULevel* ActiveLevel = World->GetActiveLevel())
		{
			ActiveLevel->GetRenderProxy().WarmupSpatialIndices();
		}
		if (ULevel* PersistentLevel = World->GetPersistentLevel())
		{
			if (PersistentLevel != World->GetActiveLevel())
			{
				PersistentLevel->GetRenderProxy().WarmupSpatialIndices();
			}
		}
	}
	ResetIdPickingState();
}

void FEditorViewportClient::CreateCamera()
{
	DestroyCamera();
	Camera = std::make_unique<FViewportCamera>();
}

void FEditorViewportClient::DestroyCamera()
{
	Camera.reset();
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

	const FViewportCameraState& CameraState = Camera->GetCameraState();
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
	UpdateIdPickingAdaptivePolicy();
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
	UpdateLatestMouseLocalForIdProbe(LocalMouseX, LocalMouseY);

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
			// 트랜스폼 드래그 중에는 Octree dirty를 모아서 드래그 종료 시점에 1회만 반영한다.
			BeginDeferredSpatialIndexInvalidation();
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
		EndDeferredSpatialIndexInvalidation();
		Gizmo->DragEnd();
	}
	else if (InputSystem::Get().GetKeyUp(VK_LBUTTON))
	{
		EndDeferredSpatialIndexInvalidation();
		// 드래그 threshold 미달로 DragEnd가 호출되지 않는 경우 처리
		Gizmo->SetPressedOnHandle(false);
	}
}

void FEditorViewportClient::HandleDragStart(const FRay& Ray, float LocalMouseX, float LocalMouseY)
{
	BeginClickE2ETiming();

	EPickingMode PickingMode = EPickingMode::RayTriangleBVH;
	if (UEditorEngine* Editor = Cast<UEditorEngine>(GEngine))
	{
		PickingMode = Editor->GetPickingMode();
	}

	if (PickingMode == EPickingMode::IDBuffer)
	{
		BeginPendingIdPickTiming();
		CancelPendingIdPickReadback();
		PendingIdPickRetryCount = 0;
		bPendingIdPickCtrlHeld = InputSystem::Get().GetKey(VK_CONTROL);
		const float MaxX = (Viewport && Viewport->GetWidth() > 0) ? static_cast<float>(Viewport->GetWidth() - 1) : 0.0f;
		const float MaxY = (Viewport && Viewport->GetHeight() > 0) ? static_cast<float>(Viewport->GetHeight() - 1) : 0.0f;
		PendingIdPickX = static_cast<uint32>(Clamp(LocalMouseX, 0.0f, MaxX));
		PendingIdPickY = static_cast<uint32>(Clamp(LocalMouseY, 0.0f, MaxY));

		FHitResult GizmoHitResult{};
		if (FRayUtils::RaycastComponent(Gizmo, Ray, GizmoHitResult))
		{
			Gizmo->SetPressedOnHandle(true);
			EndPendingIdPickTiming();
			EndClickE2ETiming();
			return;
		}

		uint32 CachedProbeId = 0u;
		if (TryConsumeCachedIdProbeResult(PendingIdPickX, PendingIdPickY, CachedProbeId))
		{
			SetIdPickResult(CachedProbeId);
			ApplyIdPickResultNow();
			return;
		}

		bPendingIdPickRequest = true;
		return;
	}

	const bool bCtrlHeld = InputSystem::Get().GetKey(VK_CONTROL);
	SCOPE_STAT("Picking.Ray.E2E");
	const float MaxX = (Viewport && Viewport->GetWidth() > 0) ? static_cast<float>(Viewport->GetWidth() - 1) : 0.0f;
	const float MaxY = (Viewport && Viewport->GetHeight() > 0) ? static_cast<float>(Viewport->GetHeight() - 1) : 0.0f;
	const uint32 ClickX = static_cast<uint32>(Clamp(LocalMouseX, 0.0f, MaxX));
	const uint32 ClickY = static_cast<uint32>(Clamp(LocalMouseY, 0.0f, MaxY));

	if (!bCtrlHeld
		&& IsRayPickCacheValidForCurrentCamera()
		&& ClickX == CachedRayPickX
		&& ClickY == CachedRayPickY)
	{
		AActor* CachedActor = Cast<AActor>(FPickingService::ResolveObjectFromPickingId(CachedRayPickedActorId));
		if (CachedActor && CachedActor->IsVisible())
		{
			SelectionManager->Select(CachedActor);
			EndClickE2ETiming();
			return;
		}
		if (CachedRayPickedActorId == 0u)
		{
			SelectionManager->ClearSelection();
			EndClickE2ETiming();
			return;
		}
	}

	FHitResult HitResult{};
	if (FPickingService::PickGizmo(Gizmo, Ray, PickingMode, HitResult))
	{
		Gizmo->SetPressedOnHandle(true);
	}
	else
	{
		float ClosestDistance = FLT_MAX;
		AActor* BestActor = FPickingService::PickActor(World, Ray, PickingMode, ClosestDistance);

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

		UpdateRayPickCache(ClickX, ClickY, BestActor);
	}

	EndClickE2ETiming();
}

void FEditorViewportClient::ProcessPendingIdPickResult()
{
	if (!bHasPendingIdPickResult || !SelectionManager)
	{
		return;
	}

	bHasPendingIdPickResult = false;

	if (PendingPickedObjectId == 0u)
	{
		if (!bPendingIdPickCtrlHeld)
		{
			SelectionManager->ClearSelection();
		}
		EndPendingIdPickTiming();
		return;
	}

	if (Gizmo && PendingPickedObjectId == Gizmo->GetUUID())
	{
		PendingIdPickRetryCount = 0;
		Gizmo->SetPressedOnHandle(true);
		EndPendingIdPickTiming();
		return;
	}

	UObject* PickedObject = FPickingService::ResolveObjectFromPickingId(PendingPickedObjectId);
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
		EndPendingIdPickTiming();
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

	PendingIdPickRetryCount = 0;
	EndPendingIdPickTiming();
}

void FEditorViewportClient::ApplyIdPickResultNow()
{
	ProcessPendingIdPickResult();
}

void FEditorViewportClient::BeginClickE2ETiming()
{
	bPendingClickE2ETiming = true;
	PendingClickE2EStartCycles = FPlatformTime::Cycles64();
	LastClickE2EStartCycles = PendingClickE2EStartCycles;
}

void FEditorViewportClient::EndClickE2ETiming()
{
	if (!bPendingClickE2ETiming)
	{
		return;
	}

	const uint64 EndCycles = FPlatformTime::Cycles64();
	FStatManager::Get().RecordTime("Picking.Click.E2E", FPlatformTime::ToSeconds(EndCycles - PendingClickE2EStartCycles));
	bPendingClickE2ETiming = false;
	PendingClickE2EStartCycles = 0;
}

void FEditorViewportClient::AbortClickE2ETiming()
{
	bPendingClickE2ETiming = false;
	PendingClickE2EStartCycles = 0;
}

void FEditorViewportClient::UpdateIdPickingAdaptivePolicy()
{
	const uint64 NowCycles = FPlatformTime::Cycles64();

	const bool bCameraInputActive = IsCameraInputActiveNow();

	if (bCameraInputActive)
	{
		LastCameraInteractCycles = NowCycles;
	}

	const double SinceClickMs =
		(LastClickE2EStartCycles == 0u) ? DBL_MAX : FPlatformTime::ToMilliseconds(NowCycles - LastClickE2EStartCycles);
	const double SinceInteractMs =
		(LastCameraInteractCycles == 0u) ? DBL_MAX : FPlatformTime::ToMilliseconds(NowCycles - LastCameraInteractCycles);

	if (bPendingIdPickRequest || bPendingIdPickReadback || bPendingClickE2ETiming || SinceClickMs < 150.0)
	{
		ActiveIdBufferUpdateIntervalMs = 0.0f;
		IdProbePrefetchFrameStride = 1u;
		return;
	}

	// During active camera interaction, keep ID updates sparse and disable probe prefetch.
	if (bCameraInputActive || SinceInteractMs < 200.0)
	{
		ActiveIdBufferUpdateIntervalMs = 120.0f;
		IdProbePrefetchFrameStride = 0u;
		return;
	}

	ActiveIdBufferUpdateIntervalMs = IdBufferUpdateIntervalMs;
	IdProbePrefetchFrameStride = 4u;
}

void FEditorViewportClient::BeginPendingIdPickTiming()
{
	bPendingIdPickTiming = true;
	PendingIdPickStartCycles = FPlatformTime::Cycles64();
	PendingIdPickFetchCycles = 0;
	PendingIdPickWaitCycles = 0;
}

void FEditorViewportClient::EndPendingIdPickTiming()
{
	if (!bPendingIdPickTiming)
	{
		return;
	}

	const uint64 EndCycles = FPlatformTime::Cycles64();
	const uint64 TotalCycles = EndCycles - PendingIdPickStartCycles;
	const uint64 FetchCycles = PendingIdPickFetchCycles;
	const uint64 WaitCycles = PendingIdPickWaitCycles;
	FStatManager::Get().RecordTime("Picking.ID.Total", FPlatformTime::ToSeconds(TotalCycles));
	FStatManager::Get().RecordTime("Picking.ID.Algorithm", FPlatformTime::ToSeconds(FetchCycles));
	FStatManager::Get().RecordTime("Picking.ID.Fetch.Click", FPlatformTime::ToSeconds(FetchCycles));
	if (WaitCycles > 0u)
	{
		FStatManager::Get().RecordTime("Picking.ID.Wait.Click", FPlatformTime::ToSeconds(WaitCycles));
	}
	bPendingIdPickTiming = false;
	PendingIdPickStartCycles = 0;
	PendingIdPickFetchCycles = 0;
	PendingIdPickWaitCycles = 0;
	EndClickE2ETiming();
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

void FEditorViewportClient::BeginPendingIdProbeReadback(uint32 InRequestId, uint32 InX, uint32 InY)
{
	bPendingIdProbeReadback = (InRequestId != 0u);
	PendingIdProbeReadbackRequestId = InRequestId;
	PendingIdProbeX = InX;
	PendingIdProbeY = InY;
}

void FEditorViewportClient::CancelPendingIdProbeReadback()
{
	if (Viewport && PendingIdProbeReadbackRequestId != 0u)
	{
		Viewport->CancelPickingIdReadback(PendingIdProbeReadbackRequestId);
	}

	bPendingIdProbeReadback = false;
	PendingIdProbeReadbackRequestId = 0u;
}

void FEditorViewportClient::OnIdProbeSampleReady(uint32 InId)
{
	bPendingIdProbeReadback = false;
	PendingIdProbeReadbackRequestId = 0u;
	bHasCachedIdProbeResult = true;

	for (FCachedIdProbeSample& Sample : CachedIdProbeSamples)
	{
		if (Sample.X == PendingIdProbeX && Sample.Y == PendingIdProbeY)
		{
			Sample.Id = InId;
			return;
		}
	}

	if (CachedIdProbeSamples.size() >= 16)
	{
		CachedIdProbeSamples.erase(CachedIdProbeSamples.begin());
	}
	CachedIdProbeSamples.push_back({ PendingIdProbeX, PendingIdProbeY, InId });
}

bool FEditorViewportClient::TryConsumeCachedIdProbeResult(uint32 InX, uint32 InY, uint32& OutId) const
{
	OutId = 0u;
	if (!bHasCachedIdProbeResult || bIdBufferDirty || !IsIdBufferCacheValidForCurrentCamera())
	{
		return false;
	}

	int32 BestDistSq = 10;
	uint32 BestId = 0u;
	for (const FCachedIdProbeSample& Sample : CachedIdProbeSamples)
	{
		const int32 DX = static_cast<int32>(Sample.X) - static_cast<int32>(InX);
		const int32 DY = static_cast<int32>(Sample.Y) - static_cast<int32>(InY);
		const int32 DistSq = DX * DX + DY * DY;
		if (DistSq <= 9 && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestId = Sample.Id;
		}
	}

	if (BestDistSq > 9)
	{
		return false;
	}

	OutId = BestId;
	return true;
}

bool FEditorViewportClient::TryPromotePendingIdProbeToPick(uint32 InX, uint32 InY, uint32& OutRequestId)
{
	OutRequestId = 0u;
	if (!bPendingIdProbeReadback || PendingIdProbeReadbackRequestId == 0u)
	{
		return false;
	}

	const int32 DX = static_cast<int32>(PendingIdProbeX) - static_cast<int32>(InX);
	const int32 DY = static_cast<int32>(PendingIdProbeY) - static_cast<int32>(InY);
	if ((DX * DX + DY * DY) > 9)
	{
		return false;
	}

	OutRequestId = PendingIdProbeReadbackRequestId;
	bPendingIdProbeReadback = false;
	PendingIdProbeReadbackRequestId = 0u;
	return true;
}

bool FEditorViewportClient::GetIdProbeCoordForPrefetch(uint32& OutX, uint32& OutY)
{
	OutX = 0u;
	OutY = 0u;

	if (IdProbePrefetchFrameStride == 0u)
	{
		return false;
	}

	if (!bHasLatestMouseLocalForIdProbe || !Viewport || Viewport->GetWidth() == 0 || Viewport->GetHeight() == 0)
	{
		return false;
	}

	++IdProbePrefetchFrameCounter;
	if (IdProbePrefetchFrameStride > 1u &&
		(IdProbePrefetchFrameCounter % IdProbePrefetchFrameStride) != 0u)
	{
		return false;
	}

	constexpr int32 Pattern[9][2] = {
		{ 0, 0 }, { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 }
	};

	const int32 PatternIndex = static_cast<int32>(NextIdProbePatternIndex % 9u);
	const float ProbeX = LatestMouseLocalXForIdProbe + static_cast<float>(Pattern[PatternIndex][0]);
	const float ProbeY = LatestMouseLocalYForIdProbe + static_cast<float>(Pattern[PatternIndex][1]);

	const float MaxX = static_cast<float>(Viewport->GetWidth() - 1);
	const float MaxY = static_cast<float>(Viewport->GetHeight() - 1);
	OutX = static_cast<uint32>(Clamp(ProbeX, 0.0f, MaxX));
	OutY = static_cast<uint32>(Clamp(ProbeY, 0.0f, MaxY));
	NextIdProbePatternIndex = (NextIdProbePatternIndex + 1u) % 9u;
	return true;
}

void FEditorViewportClient::UpdateLatestMouseLocalForIdProbe(float LocalMouseX, float LocalMouseY)
{
	bHasLatestMouseLocalForIdProbe = true;
	LatestMouseLocalXForIdProbe = LocalMouseX;
	LatestMouseLocalYForIdProbe = LocalMouseY;
}

void FEditorViewportClient::ResetIdPickingState()
{
	EndDeferredSpatialIndexInvalidation();
	AbortClickE2ETiming();
	CancelPendingIdPickReadback();
	CancelPendingIdProbeReadback();
	bForceIdBufferPrewarm = true;
	bPendingIdPickRequest = false;
	bHasPendingIdPickResult = false;
	PendingPickedObjectId = 0u;
	PendingIdPickRetryCount = 0;
	bPendingIdPickTiming = false;
	PendingIdPickStartCycles = 0;
	PendingIdPickFetchCycles = 0;
	PendingIdPickWaitCycles = 0;
	bPendingIdPickCtrlHeld = false;
	PendingIdPickX = 0u;
	PendingIdPickY = 0u;
	PendingIdProbeX = 0u;
	PendingIdProbeY = 0u;
	bHasCachedIdProbeResult = false;
	CachedIdProbeSamples.clear();
	NextIdProbePatternIndex = 0u;
	IdProbePrefetchFrameCounter = 0u;
	IdProbePrefetchFrameStride = 1u;
	bHasLatestMouseLocalForIdProbe = false;
	LatestMouseLocalXForIdProbe = 0.0f;
	LatestMouseLocalYForIdProbe = 0.0f;
	LastIdBufferRenderCycles = 0u;
	ActiveIdBufferUpdateIntervalMs = 0.0f;
	LastClickE2EStartCycles = 0u;
	LastCameraInteractCycles = 0u;
	CachedActiveLevelSpatialChangeSerial = 0u;
	CachedPersistentLevelSpatialChangeSerial = 0u;
	InvalidateIdBufferCache();
}

void FEditorViewportClient::BeginDeferredSpatialIndexInvalidation()
{
	if (bDeferredSpatialIndexInvalidation || !World)
	{
		return;
	}

	if (ULevel* ActiveLevel = World->GetActiveLevel())
	{
		ActiveLevel->GetRenderProxy().BeginDeferSpatialIndexInvalidation();
	}

	if (ULevel* PersistentLevel = World->GetPersistentLevel())
	{
		if (PersistentLevel != World->GetActiveLevel())
		{
			PersistentLevel->GetRenderProxy().BeginDeferSpatialIndexInvalidation();
		}
	}

	bDeferredSpatialIndexInvalidation = true;
}

void FEditorViewportClient::EndDeferredSpatialIndexInvalidation()
{
	if (!bDeferredSpatialIndexInvalidation || !World)
	{
		return;
	}

	if (ULevel* ActiveLevel = World->GetActiveLevel())
	{
		ActiveLevel->GetRenderProxy().EndDeferSpatialIndexInvalidation();
		ActiveLevel->GetRenderProxy().WarmupSpatialIndices();
	}

	if (ULevel* PersistentLevel = World->GetPersistentLevel())
	{
		if (PersistentLevel != World->GetActiveLevel())
		{
			PersistentLevel->GetRenderProxy().EndDeferSpatialIndexInvalidation();
			PersistentLevel->GetRenderProxy().WarmupSpatialIndices();
		}
	}

	bDeferredSpatialIndexInvalidation = false;
}

bool FEditorViewportClient::IsIdBufferCacheValidForCurrentCamera() const
{
	if (!bHasCachedIdPickResult || !Camera)
	{
		return false;
	}

	constexpr float FOVEpsilon = 0.25f * DEG_TO_RAD;
	constexpr float OrthoWidthEpsilon = 0.5f;
	constexpr float PositionEpsilon = 0.02f;
	constexpr float ForwardEpsilon = 0.001f;

	const bool bSameProjection = (Camera->IsOrthogonal() == bCachedIdPickCameraOrtho)
		&& (std::abs(Camera->GetFOV() - CachedIdPickCameraFOV) <= FOVEpsilon)
		&& (std::abs(Camera->GetOrthoWidth() - CachedIdPickCameraOrthoWidth) <= OrthoWidthEpsilon);
	const bool bSameView = FVector::Distance(Camera->GetWorldLocation(), CachedIdPickCameraLocation) <= PositionEpsilon
		&& FVector::Distance(Camera->GetForwardVector(), CachedIdPickCameraForward) <= ForwardEpsilon;

	return bSameProjection && bSameView;
}

bool FEditorViewportClient::IsRayPickCacheValidForCurrentCamera() const
{
	if (!bHasCachedRayPickResult || !Camera)
	{
		return false;
	}

	constexpr float FOVEpsilon = 0.10f * DEG_TO_RAD;
	constexpr float OrthoWidthEpsilon = 0.25f;
	constexpr float PositionEpsilon = 0.01f;
	constexpr float ForwardEpsilon = 0.0005f;

	const bool bSameProjection = (Camera->IsOrthogonal() == bCachedRayPickCameraOrtho)
		&& (std::abs(Camera->GetFOV() - CachedRayPickCameraFOV) <= FOVEpsilon)
		&& (std::abs(Camera->GetOrthoWidth() - CachedRayPickCameraOrthoWidth) <= OrthoWidthEpsilon);
	const bool bSameView = FVector::Distance(Camera->GetWorldLocation(), CachedRayPickCameraLocation) <= PositionEpsilon
		&& FVector::Distance(Camera->GetForwardVector(), CachedRayPickCameraForward) <= ForwardEpsilon;

	return bSameProjection && bSameView;
}

void FEditorViewportClient::UpdateRayPickCache(uint32 InX, uint32 InY, AActor* InActor)
{
	if (!Camera)
	{
		InvalidateRayPickCache();
		return;
	}

	bHasCachedRayPickResult = true;
	CachedRayPickX = InX;
	CachedRayPickY = InY;
	CachedRayPickedActorId = InActor ? InActor->GetUUID() : 0u;
	CachedRayPickCameraLocation = Camera->GetWorldLocation();
	CachedRayPickCameraForward = Camera->GetForwardVector();
	bCachedRayPickCameraOrtho = Camera->IsOrthogonal();
	CachedRayPickCameraFOV = Camera->GetFOV();
	CachedRayPickCameraOrthoWidth = Camera->GetOrthoWidth();
}

bool FEditorViewportClient::ShouldRenderPendingIdPick() const
{
	if (bPendingIdPickReadback)
	{
		return false;
	}

	if (bForceIdBufferPrewarm)
	{
		return true;
	}

	if (!bIdBufferDirty)
	{
		return false;
	}

	// Keep camera interaction smooth: skip background ID refresh unless there is an urgent click path.
	if (!bPendingIdPickRequest && !bForceIdBufferPrewarm && IsCameraInputActiveNow())
	{
		return false;
	}

	if (LastIdBufferRenderCycles == 0)
	{
		return true;
	}

	const uint64 NowCycles = FPlatformTime::Cycles64();
	const double ElapsedMs = FPlatformTime::ToMilliseconds(NowCycles - LastIdBufferRenderCycles);
	return ElapsedMs >= static_cast<double>(ActiveIdBufferUpdateIntervalMs);
}

void FEditorViewportClient::RefreshIdBufferDirtyStateFromCamera()
{
	HandleIdPickingLevelMutation();
	UpdateIdBufferDirtyFromCamera();
}

void FEditorViewportClient::UpdateIdBufferDirtyFromCamera()
{
	if (!Camera)
	{
		bIdBufferDirty = true;
		return;
	}

	const bool bCameraDirty = !IsIdBufferCacheValidForCurrentCamera();
	const bool bSceneDirty = IsIdPickingLevelStateDirty();
	bIdBufferDirty = bCameraDirty || bSceneDirty;
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
	if (World)
	{
		if (ULevel* ActiveLevel = World->GetActiveLevel())
		{
			CachedActiveLevelSpatialChangeSerial = ActiveLevel->GetRenderProxy().GetSpatialChangeSerial();
		}
		if (ULevel* PersistentLevel = World->GetPersistentLevel())
		{
			CachedPersistentLevelSpatialChangeSerial = PersistentLevel->GetRenderProxy().GetSpatialChangeSerial();
		}
	}
	LastIdBufferRenderCycles = FPlatformTime::Cycles64();
	bForceIdBufferPrewarm = false;
	bIdBufferDirty = false;
}

bool FEditorViewportClient::IsIdPickingLevelStateDirty() const
{
	if (!World)
	{
		return false;
	}

	if (ULevel* ActiveLevel = World->GetActiveLevel())
	{
		const FWorldRenderProxy& RP = ActiveLevel->GetRenderProxy();
		if (RP.IsSpatialIndexDirtyForQueries() || RP.GetSpatialChangeSerial() != CachedActiveLevelSpatialChangeSerial)
		{
			return true;
		}
	}

	if (ULevel* PersistentLevel = World->GetPersistentLevel())
	{
		const FWorldRenderProxy& RP = PersistentLevel->GetRenderProxy();
		if (RP.IsSpatialIndexDirtyForQueries() || RP.GetSpatialChangeSerial() != CachedPersistentLevelSpatialChangeSerial)
		{
			return true;
		}
	}

	return false;
}

void FEditorViewportClient::HandleIdPickingLevelMutation()
{
	if (!IsIdPickingLevelStateDirty())
	{
		return;
	}

	// Level/object transform mutation invalidates stale ID results and probe samples.
	InvalidateIdBufferCache();

	// Pending probe readback is speculative; discard on mutation.
	CancelPendingIdProbeReadback();

	// If a click readback is already in flight, retry against the refreshed ID buffer.
	if (bPendingIdPickReadback)
	{
		CancelPendingIdPickReadback();
		bPendingIdPickRequest = true;
	}
}

bool FEditorViewportClient::IsCameraInputActiveNow() const
{
	return InputSystem::Get().GetKey(VK_RBUTTON) ||
		InputSystem::Get().GetLeftDragging() ||
		InputSystem::Get().GetKey('W') || InputSystem::Get().GetKey('A') ||
		InputSystem::Get().GetKey('S') || InputSystem::Get().GetKey('D') ||
		InputSystem::Get().GetKey('Q') || InputSystem::Get().GetKey('E') ||
		InputSystem::Get().GetKey(VK_UP) || InputSystem::Get().GetKey(VK_DOWN) ||
		InputSystem::Get().GetKey(VK_LEFT) || InputSystem::Get().GetKey(VK_RIGHT) ||
		(InputSystem::Get().GetScrollNotches() != 0.0f);
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

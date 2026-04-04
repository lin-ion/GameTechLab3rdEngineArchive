#pragma once

#include "Viewport/ViewportClient.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/ViewTypes.h"

#include "UI/SWindow.h"
#include <string>
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
class UWorld;
class UCameraComponent;
class UGizmoComponent;
class FEditorSettings;
class FWindowsWindow;
class FSelectionManager;
class FViewport;

class FEditorViewportClient : public FViewportClient
{
public:
	void Initialize(FWindowsWindow* InWindow);
	void SetWorld(UWorld* InWorld) { World = InWorld; }
	void SetGizmo(UGizmoComponent* InGizmo) { Gizmo = InGizmo; }
	void SetSettings(const FEditorSettings* InSettings) { Settings = InSettings; }
	void SetSelectionManager(FSelectionManager* InSelectionManager) { SelectionManager = InSelectionManager; }
	UGizmoComponent* GetGizmo() { return Gizmo; }

	// 뷰포트별 렌더 옵션
	FViewportRenderOptions& GetRenderOptions() { return RenderOptions; }
	const FViewportRenderOptions& GetRenderOptions() const { return RenderOptions; }

	// 뷰포트 타입 전환 (Perspective / Ortho 방향)
	void SetViewportType(ELevelViewportType NewType);
	void SetViewportSize(float InWidth, float InHeight);

	// Camera lifecycle
	void CreateCamera();
	void DestroyCamera();
	void ResetCamera();
	UCameraComponent* GetCamera() const { return Camera; }

	void Tick(float DeltaTime);

	// 활성 상태 — 활성 뷰포트만 입력 처리
	void SetActive(bool bInActive) { bIsActive = bInActive; }
	bool IsActive() const { return bIsActive; }

	// FViewport 소유
	void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
	FViewport* GetViewport() const { return Viewport; }

	// SWindow 레이아웃 연결 — SSplitter 리프 노드
	void SetLayoutWindow(SWindow* InWindow) { LayoutWindow = InWindow; }
	SWindow* GetLayoutWindow() const { return LayoutWindow; }

	// SWindow Rect → ViewportScreenRect 갱신 + FViewport 리사이즈 요청
	void UpdateLayoutRect();

	// ImDrawList에 자신의 SRV를 SWindow Rect 위치에 렌더 (활성 테두리 포함)
	void RenderViewportImage(bool bIsActiveViewport);

private:
	void TickInput(float DeltaTime);
	void TickInteraction(float DeltaTime);
	void HandleDragStart(const FRay& Ray, float LocalMouseX, float LocalMouseY);
	void ProcessPendingIdPickResult();
	void UpdateIdBufferDirtyFromCamera();

public:
	bool HasPendingIdPickRequest() const { return bPendingIdPickRequest; }
	bool ShouldRenderPendingIdPick() const;
	void GetPendingIdPickCoord(uint32& OutX, uint32& OutY) const { OutX = PendingIdPickX; OutY = PendingIdPickY; }
	void SetIdPickResult(uint32 InId) { PendingPickedObjectId = InId; bHasPendingIdPickResult = true; bPendingIdPickRequest = false; bPendingIdPickReadback = false; PendingIdPickReadbackRequestId = 0u; }
	bool HasPendingIdPickReadback() const { return bPendingIdPickReadback; }
	uint32 GetPendingIdPickReadbackRequestId() const { return PendingIdPickReadbackRequestId; }
	void BeginPendingIdPickReadback(uint32 InRequestId) { bPendingIdPickRequest = false; bPendingIdPickReadback = true; PendingIdPickReadbackRequestId = InRequestId; }
	void CancelPendingIdPickReadback() { bPendingIdPickReadback = false; PendingIdPickReadbackRequestId = 0u; }
	void RefreshIdBufferDirtyStateFromCamera();
	void UpdateIdBufferCacheCameraState();
	void InvalidateIdBufferCache() { bHasCachedIdPickResult = false; bIdBufferDirty = true; }

private:
	FViewport* Viewport = nullptr;
	SWindow* LayoutWindow = nullptr;
	FWindowsWindow* Window = nullptr;
	UWorld* World = nullptr;
	UCameraComponent* Camera = nullptr;
	UGizmoComponent* Gizmo = nullptr;
	const FEditorSettings* Settings = nullptr;
	FSelectionManager* SelectionManager = nullptr;
	FViewportRenderOptions RenderOptions;

	float WindowWidth = 1920.f;
	float WindowHeight = 1080.f;

	bool bIsActive = false;
	bool bPendingIdPickRequest = false;
	bool bPendingIdPickReadback = false;
	bool bIdBufferDirty = true;
	bool bHasPendingIdPickResult = false;
	bool bPendingIdPickCtrlHeld = false;
	uint32 PendingIdPickX = 0;
	uint32 PendingIdPickY = 0;
	uint32 PendingIdPickReadbackRequestId = 0;
	uint32 PendingPickedObjectId = 0;

	bool bHasCachedIdPickResult = false;
	FVector CachedIdPickCameraLocation = FVector(0.0f, 0.0f, 0.0f);
	FVector CachedIdPickCameraForward = FVector(0.0f, 0.0f, 0.0f);
	bool bCachedIdPickCameraOrtho = false;
	float CachedIdPickCameraFOV = 0.0f;
	float CachedIdPickCameraOrthoWidth = 0.0f;
	float IdBufferUpdateIntervalMs = 33.0f;	//	ID 버퍼 렌더 주기 (자주 렌더링 하지 않도록 방지)
	uint64 LastIdBufferRenderCycles = 0;
	bool IsIdBufferCacheValidForCurrentCamera() const;
	// 뷰포트 슬롯의 스크린 좌표 (ImGui screen space = 윈도우 클라이언트 좌표)
	FRect ViewportScreenRect;
};

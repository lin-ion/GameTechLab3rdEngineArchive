#pragma once

#include "Viewport/ViewportClient.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/ViewTypes.h"
#include "Editor/Viewport/ViewportCamera.h"

#include "UI/SWindow.h"
#include <string>
#include <memory>
#include "Core/RayTypes.h"
#include "Core/CollisionTypes.h"
class UWorld;
class UGizmoComponent;
class FEditorSettings;
class FWindowsWindow;
class FSelectionManager;
class FViewport;
class AActor;

class FEditorViewportClient : public FViewportClient
{
	struct FCachedIdProbeSample
	{
		uint32 X = 0u;
		uint32 Y = 0u;
		uint32 Id = 0u;
	};

public:
	void Initialize(FWindowsWindow* InWindow);
	void SetWorld(UWorld* InWorld);
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
	FViewportCamera* GetCamera() const { return Camera.get(); }

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
	void BeginClickE2ETiming();
	void EndClickE2ETiming();
	void AbortClickE2ETiming();
	void UpdateIdPickingAdaptivePolicy();
	void BeginPendingIdPickTiming();
	void EndPendingIdPickTiming();
	void UpdateLatestMouseLocalForIdProbe(float LocalMouseX, float LocalMouseY);
	void BeginDeferredSpatialIndexInvalidation();
	void EndDeferredSpatialIndexInvalidation();
	void UpdateIdBufferDirtyFromCamera();
	bool IsCameraInputActiveNow() const;
	bool IsIdPickingLevelStateDirty() const;
	void HandleIdPickingLevelMutation();
	bool IsRayPickCacheValidForCurrentCamera() const;
	void UpdateRayPickCache(uint32 InX, uint32 InY, AActor* InActor);
	void InvalidateRayPickCache() { bHasCachedRayPickResult = false; CachedRayPickedActorId = 0u; }

public:
	void ApplyIdPickResultNow();
	void AddPendingIdPickFetchCycles(uint64 InCycles) { PendingIdPickFetchCycles += InCycles; }
	void AddPendingIdPickWaitCycles(uint64 InCycles) { PendingIdPickWaitCycles += InCycles; }
	bool HasPendingIdPickRequest() const { return bPendingIdPickRequest; }
	bool HasPendingIdPickReadbackOrRequest() const { return bPendingIdPickRequest || bPendingIdPickReadback; }
	bool ShouldRenderPendingIdPick() const;
	void GetPendingIdPickCoord(uint32& OutX, uint32& OutY) const { OutX = PendingIdPickX; OutY = PendingIdPickY; }
	void SetIdPickResult(uint32 InId) { PendingPickedObjectId = InId; bHasPendingIdPickResult = true; bPendingIdPickRequest = false; bPendingIdPickReadback = false; PendingIdPickReadbackRequestId = 0u; }
	bool HasPendingIdPickReadback() const { return bPendingIdPickReadback; }
	uint32 GetPendingIdPickReadbackRequestId() const { return PendingIdPickReadbackRequestId; }
	void BeginPendingIdPickReadback(uint32 InRequestId) { bPendingIdPickRequest = false; bPendingIdPickReadback = true; PendingIdPickReadbackRequestId = InRequestId; }
	void CancelPendingIdPickReadback();
	bool HasPendingIdProbeReadback() const { return bPendingIdProbeReadback; }
	uint32 GetPendingIdProbeReadbackRequestId() const { return PendingIdProbeReadbackRequestId; }
	void BeginPendingIdProbeReadback(uint32 InRequestId, uint32 InX, uint32 InY);
	void CancelPendingIdProbeReadback();
	bool TryPromotePendingIdProbeToPick(uint32 InX, uint32 InY, uint32& OutRequestId);
	void OnIdProbeSampleReady(uint32 InId);
	bool TryConsumeCachedIdProbeResult(uint32 InX, uint32 InY, uint32& OutId) const;
	bool GetIdProbeCoordForPrefetch(uint32& OutX, uint32& OutY);
	void ResetIdPickingState();
	void RefreshIdBufferDirtyStateFromCamera();
	void UpdateIdBufferCacheCameraState();
	void InvalidateIdBufferCache()
	{
		bHasCachedIdPickResult = false;
		bIdBufferDirty = true;
		bHasCachedIdProbeResult = false;
		CachedIdProbeSamples.clear();
	}

private:
	FViewport* Viewport = nullptr;
	SWindow* LayoutWindow = nullptr;
	FWindowsWindow* Window = nullptr;
	UWorld* World = nullptr;
	std::unique_ptr<FViewportCamera> Camera;
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
	bool bForceIdBufferPrewarm = true;
	bool bDeferredSpatialIndexInvalidation = false;
	bool bHasPendingIdPickResult = false;
	bool bPendingIdPickCtrlHeld = false;
	uint32 PendingIdPickX = 0;
	uint32 PendingIdPickY = 0;
	uint32 PendingIdPickReadbackRequestId = 0;
	uint32 PendingPickedObjectId = 0;
	uint8 PendingIdPickRetryCount = 0;
	bool bPendingClickE2ETiming = false;
	uint64 PendingClickE2EStartCycles = 0;
	uint64 LastClickE2EStartCycles = 0;
	uint64 LastCameraInteractCycles = 0;
	bool bPendingIdPickTiming = false;
	uint64 PendingIdPickStartCycles = 0;
	uint64 PendingIdPickFetchCycles = 0;
	uint64 PendingIdPickWaitCycles = 0;
	bool bPendingIdProbeReadback = false;
	uint32 PendingIdProbeReadbackRequestId = 0;
	uint32 PendingIdProbeX = 0;
	uint32 PendingIdProbeY = 0;
	bool bHasCachedIdProbeResult = false;
	TArray<FCachedIdProbeSample> CachedIdProbeSamples;
	uint32 NextIdProbePatternIndex = 0u;
	bool bHasLatestMouseLocalForIdProbe = false;
	float LatestMouseLocalXForIdProbe = 0.0f;
	float LatestMouseLocalYForIdProbe = 0.0f;

	bool bHasCachedIdPickResult = false;
	FVector CachedIdPickCameraLocation = FVector(0.0f, 0.0f, 0.0f);
	FVector CachedIdPickCameraForward = FVector(0.0f, 0.0f, 0.0f);
	bool bCachedIdPickCameraOrtho = false;
	float CachedIdPickCameraFOV = 0.0f;
	float CachedIdPickCameraOrthoWidth = 0.0f;
	float IdBufferUpdateIntervalMs = 33.0f;	//	유휴 상태 기본 갱신 주기
	float ActiveIdBufferUpdateIntervalMs = 0.0f;	//	적응형 정책으로 갱신되는 실제 주기
	uint32 IdProbePrefetchFrameStride = 1u;	//	1이면 매 프레임, 2면 격프레임, ...
	uint32 IdProbePrefetchFrameCounter = 0u;
	uint64 LastIdBufferRenderCycles = 0;
	bool IsIdBufferCacheValidForCurrentCamera() const;
	uint64 CachedActiveLevelSpatialChangeSerial = 0u;
	uint64 CachedPersistentLevelSpatialChangeSerial = 0u;

	bool bHasCachedRayPickResult = false;
	uint32 CachedRayPickX = 0;
	uint32 CachedRayPickY = 0;
	uint32 CachedRayPickedActorId = 0;
	FVector CachedRayPickCameraLocation = FVector(0.0f, 0.0f, 0.0f);
	FVector CachedRayPickCameraForward = FVector(0.0f, 0.0f, 0.0f);
	bool bCachedRayPickCameraOrtho = false;
	float CachedRayPickCameraFOV = 0.0f;
	float CachedRayPickCameraOrthoWidth = 0.0f;

	// 뷰포트 슬롯의 스크린 좌표 (ImGui screen space = 윈도우 클라이언트 좌표)
	FRect ViewportScreenRect;
};

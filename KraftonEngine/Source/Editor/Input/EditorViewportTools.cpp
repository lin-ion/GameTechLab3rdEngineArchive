#include "Editor/Input/EditorViewportInputUtils.h"
#include "Editor/Input/EditorViewportTools.h"

#include "Component/PrimitiveComponent.h"
#include "Editor/EditorEngine.h"
#include "Editor/Gizmo/TransformGizmo.h"
#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/Selection/PickingService.h"
#include "Editor/Selection/SelectionManager.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/Object.h"
#include "Viewport/Viewport.h"

#include <cfloat>
#include <cmath>

FEditorGizmoTool::FEditorGizmoTool(FEditorViewportClient* InOwner)
	: Owner(InOwner)
{
}

bool FEditorGizmoTool::HandleInput(float DeltaTime)
{
	if (!Owner)
	{
		return false;
	}

	bool bConsumed = false;

	if (!Owner->Camera || !Owner->Gizmo || !Owner->World)
	{
		return false;
	}

	Owner->Gizmo->ApplyScreenSpaceScaling(
		Owner->Camera->GetWorldLocation(),
		Owner->Camera->IsOrthogonal(),
		Owner->Camera->GetOrthoWidth());
	Owner->Gizmo->UpdateAxisMask(Owner->RenderOptions.ViewportType);

	if (Owner->InputContext.bImGuiCapturedMouse && !Owner->InputContext.bCaptured && !Owner->Gizmo->IsHolding())
	{
		return true;
	}

	const float ZoomSpeed = Owner->Settings ? Owner->Settings->CameraZoomSpeed : 300.f;
	const float ScrollNotches = Owner->InputContext.Frame.WheelNotches;
	if (ScrollNotches != 0.0f)
	{
		bConsumed = true;
		if (Owner->Camera->IsOrthogonal())
		{
			const float NewWidth = Owner->Camera->GetOrthoWidth() - ScrollNotches * ZoomSpeed * DeltaTime;
			Owner->Camera->SetOrthoWidth(Clamp(NewWidth, 0.1f, 1000.0f));
		}
		else
		{
			constexpr float FovStep = 2.0f * DEG_TO_RAD;
			const float NewFOV = Owner->Camera->GetFOV() - ScrollNotches * FovStep;
			Owner->Camera->SetFOV(Clamp(NewFOV, 1.f * DEG_TO_RAD, 90.0f * DEG_TO_RAD));
		}
	}

	const float LocalMouseX = static_cast<float>(Owner->InputContext.MouseLocalPos.x);
	const float LocalMouseY = static_cast<float>(Owner->InputContext.MouseLocalPos.y);

	const float VPWidth = Owner->Viewport ? static_cast<float>(Owner->Viewport->GetWidth()) : Owner->WindowWidth;
	const float VPHeight = Owner->Viewport ? static_cast<float>(Owner->Viewport->GetHeight()) : Owner->WindowHeight;
	const FRay Ray = Owner->Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);

	FHitResult HoverHit{};
	Owner->Gizmo->Raycast(Ray, HoverHit);

	if (Owner->InputContext.Frame.IsPressed(VK_LBUTTON))
	{
		EPickingMode PickingMode = EPickingMode::RayTriangleBVH;
		if (UEditorEngine* Editor = Cast<UEditorEngine>(GEngine))
		{
			PickingMode = Editor->GetPickingMode();
		}

		FHitResult GizmoHit{};
		bool bGizmoHit = false;
		if (PickingMode == EPickingMode::IDBuffer)
		{
			bGizmoHit = Owner->Gizmo->Raycast(Ray, GizmoHit);
		}
		else
		{
			bGizmoHit = Owner->Gizmo->Raycast(Ray, GizmoHit);
		}

		if (bGizmoHit)
		{
			Owner->Gizmo->SetPressedOnHandle(true);
			if (!Owner->Gizmo->IsHolding())
			{
				Owner->BeginDeferredSpatialIndexInvalidation();
				Owner->Gizmo->SetHolding(true);
			}
			Owner->Gizmo->UpdateDrag(Ray);
			bConsumed = true;
		}
	}
	else if (Owner->InputContext.Frame.IsDown(VK_LBUTTON))
	{
		const bool bGizmoDragActive = Owner->Gizmo->IsPressedOnHandle() || Owner->Gizmo->IsHolding();
		if (bGizmoDragActive)
		{
			bConsumed = true;
			if (Owner->Gizmo->IsPressedOnHandle() && !Owner->Gizmo->IsHolding())
			{
				Owner->BeginDeferredSpatialIndexInvalidation();
				Owner->Gizmo->SetHolding(true);
			}

			if (Owner->Gizmo->IsHolding())
			{
				Owner->Gizmo->UpdateDrag(Ray);
			}
		}
	}
	else if (Owner->InputContext.Frame.IsReleased(VK_LBUTTON))
	{
		if (Owner->Gizmo->IsPressedOnHandle() || Owner->Gizmo->IsHolding())
		{
			bConsumed = true;
			Owner->EndDeferredSpatialIndexInvalidation();
			Owner->Gizmo->DragEnd();
		}
	}

	if (Owner->Gizmo->IsHolding())
	{
		bConsumed = true;
	}

	return bConsumed;
}
FEditorSelectionTool::FEditorSelectionTool(FEditorViewportClient* InOwner)
	: Owner(InOwner)
{
}

bool FEditorSelectionTool::HandleInput(float DeltaTime)
{
	if (!Owner)
	{
		return false;
	}

	(void)DeltaTime;
	const bool bConsumedByPending = ProcessPendingIdPickResult();
	if ((Owner->InputContext.bImGuiCapturedMouse && !Owner->InputContext.bCaptured) || !Owner->Camera || !Owner->Viewport)
	{
		return bConsumedByPending;
	}

	// Click-selection is resolved on button release only when drag did not start.
	// If drag threshold was crossed, navigation path handles the interaction.
	if (!Owner->InputContext.Frame.IsReleased(VK_LBUTTON))
	{
		return bConsumedByPending;
	}
	if (Owner->InputContext.Frame.bLeftDragEnded)
	{
		return true;
	}

	const float LocalMouseX = static_cast<float>(Owner->InputContext.MouseLocalPos.x);
	const float LocalMouseY = static_cast<float>(Owner->InputContext.MouseLocalPos.y);
	const float VPWidth = static_cast<float>(Owner->Viewport->GetWidth());
	const float VPHeight = static_cast<float>(Owner->Viewport->GetHeight());
	const FRay Ray = Owner->Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);
	HandleSelectionClick(Ray, LocalMouseX, LocalMouseY);
	return true;
}

bool FEditorSelectionTool::ProcessPendingIdPickResult()
{
	if (!IdPickState.bHasResult || !Owner->SelectionManager)
	{
		return false;
	}

	IdPickState.bHasResult = false;

	if (IdPickState.PickedObjectId == 0u)
	{
		if (!IdPickState.bCtrlHeld)
		{
			Owner->SelectionManager->ClearSelection();
		}
		return true;
	}

	if (Owner->Gizmo && IdPickState.PickedObjectId == Owner->Gizmo->GetUUID())
	{
		Owner->Gizmo->SetPressedOnHandle(true);
		return true;
	}

	UObject* PickedObject = FPickingService::ResolveObjectFromPickingId(IdPickState.PickedObjectId);
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
		if (!IdPickState.bCtrlHeld)
		{
			Owner->SelectionManager->ClearSelection();
		}
		return true;
	}

	if (IdPickState.bCtrlHeld)
	{
		Owner->SelectionManager->ToggleSelect(PickedActor);
	}
	else
	{
		Owner->SelectionManager->Select(PickedActor);
	}

	return true;
}

void FEditorSelectionTool::HandleSelectionClick(const FRay& Ray, float LocalMouseX, float LocalMouseY)
{
	if (!Owner)
	{
		return;
	}

	EPickingMode PickingMode = EPickingMode::RayTriangleBVH;
	if (UEditorEngine* Editor = Cast<UEditorEngine>(GEngine))
	{
		PickingMode = Editor->GetPickingMode();
	}

	if (PickingMode == EPickingMode::IDBuffer)
	{
		CancelPendingIdPickReadback();
		IdPickState.bCtrlHeld = Owner->InputContext.Frame.IsDown(VK_CONTROL);
		const float MaxX = (Owner->Viewport && Owner->Viewport->GetWidth() > 0)
			? static_cast<float>(Owner->Viewport->GetWidth() - 1) : 0.0f;
		const float MaxY = (Owner->Viewport && Owner->Viewport->GetHeight() > 0)
			? static_cast<float>(Owner->Viewport->GetHeight() - 1) : 0.0f;
		const uint32 PickX = static_cast<uint32>(Clamp(LocalMouseX, 0.0f, MaxX));
		const uint32 PickY = static_cast<uint32>(Clamp(LocalMouseY, 0.0f, MaxY));
		IdPickState.PickX = PickX;
		IdPickState.PickY = PickY;

		IdPickState.bPendingRequest = true;
		return;
	}

	const bool bCtrlHeld = Owner->InputContext.Frame.IsDown(VK_CONTROL);
	const float MaxX = (Owner->Viewport && Owner->Viewport->GetWidth() > 0)
		? static_cast<float>(Owner->Viewport->GetWidth() - 1) : 0.0f;
	const float MaxY = (Owner->Viewport && Owner->Viewport->GetHeight() > 0)
		? static_cast<float>(Owner->Viewport->GetHeight() - 1) : 0.0f;
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
			Owner->SelectionManager->Select(CachedActor);
			return;
		}
		if (CachedRayPickedActorId == 0u)
		{
			Owner->SelectionManager->ClearSelection();
			return;
		}
	}

	float ClosestDistance = FLT_MAX;
	AActor* BestActor = FPickingService::PickActor(Owner->World, Ray, PickingMode, ClosestDistance);
	if (!BestActor)
	{
		if (!bCtrlHeld)
		{
			Owner->SelectionManager->ClearSelection();
		}
	}
	else
	{
		if (bCtrlHeld)
		{
			Owner->SelectionManager->ToggleSelect(BestActor);
		}
		else
		{
			Owner->SelectionManager->Select(BestActor);
		}
	}

	UpdateRayPickCache(ClickX, ClickY, BestActor);
}

void FEditorSelectionTool::ResetInputState()
{
	InvalidateRayPickCache();
}

bool FEditorSelectionTool::HasPendingIdPickRequest() const
{
	return IdPickState.bPendingRequest;
}

void FEditorSelectionTool::GetPendingIdPickCoord(uint32& OutX, uint32& OutY) const
{
	OutX = IdPickState.PickX;
	OutY = IdPickState.PickY;
}

bool FEditorSelectionTool::HasPendingIdPickReadback() const
{
	return IdPickState.bPendingReadback;
}

uint32 FEditorSelectionTool::GetPendingIdPickReadbackRequestId() const
{
	return IdPickState.PendingReadbackRequestId;
}

void FEditorSelectionTool::BeginPendingIdPickReadback(uint32 InRequestId)
{
	IdPickState.bPendingRequest = false;
	IdPickState.bPendingReadback = true;
	IdPickState.PendingReadbackRequestId = InRequestId;
}

void FEditorSelectionTool::CancelPendingIdPickReadback()
{
	if (!Owner)
	{
		return;
	}

	if (Owner->Viewport && IdPickState.PendingReadbackRequestId != 0u)
	{
		Owner->Viewport->CancelPickingIdReadback(IdPickState.PendingReadbackRequestId);
	}

	IdPickState.bPendingReadback = false;
	IdPickState.PendingReadbackRequestId = 0u;
}

void FEditorSelectionTool::SetIdPickResult(uint32 InId)
{
	IdPickState.PickedObjectId = InId;
	IdPickState.bHasResult = true;
	IdPickState.bPendingRequest = false;
	IdPickState.bPendingReadback = false;
	IdPickState.PendingReadbackRequestId = 0u;
}

void FEditorSelectionTool::ResetIdPickingState()
{
	CancelPendingIdPickReadback();
	IdPickState.bPendingRequest = false;
	IdPickState.bHasResult = false;
	IdPickState.bCtrlHeld = false;
	IdPickState.PickX = 0u;
	IdPickState.PickY = 0u;
	IdPickState.PickedObjectId = 0u;
}

bool FEditorSelectionTool::IsRayPickCacheValidForCurrentCamera() const
{
	if (!bHasCachedRayPickResult || !Owner || !Owner->Camera)
	{
		return false;
	}

	constexpr float PositionEpsilon = 0.001f;
	constexpr float ForwardEpsilon = 0.0001f;
	constexpr float FOVEpsilon = 0.0001f;
	constexpr float OrthoWidthEpsilon = 0.0001f;

	const bool bSameProjection = (Owner->Camera->IsOrthogonal() == bCachedRayPickCameraOrtho)
		&& (std::abs(Owner->Camera->GetFOV() - CachedRayPickCameraFOV) <= FOVEpsilon)
		&& (std::abs(Owner->Camera->GetOrthoWidth() - CachedRayPickCameraOrthoWidth) <= OrthoWidthEpsilon);
	const bool bSameView = FVector::Distance(Owner->Camera->GetWorldLocation(), CachedRayPickCameraLocation) <= PositionEpsilon
		&& FVector::Distance(Owner->Camera->GetForwardVector(), CachedRayPickCameraForward) <= ForwardEpsilon;
	return bSameProjection && bSameView;
}

void FEditorSelectionTool::UpdateRayPickCache(uint32 InX, uint32 InY, AActor* InActor)
{
	if (!Owner || !Owner->Camera)
	{
		InvalidateRayPickCache();
		return;
	}

	bHasCachedRayPickResult = true;
	CachedRayPickX = InX;
	CachedRayPickY = InY;
	CachedRayPickedActorId = InActor ? InActor->GetUUID() : 0u;
	CachedRayPickCameraLocation = Owner->Camera->GetWorldLocation();
	CachedRayPickCameraForward = Owner->Camera->GetForwardVector();
	bCachedRayPickCameraOrtho = Owner->Camera->IsOrthogonal();
	CachedRayPickCameraFOV = Owner->Camera->GetFOV();
	CachedRayPickCameraOrthoWidth = Owner->Camera->GetOrthoWidth();
}

void FEditorSelectionTool::InvalidateRayPickCache()
{
	bHasCachedRayPickResult = false;
	CachedRayPickedActorId = 0u;
	CachedRayPickX = 0u;
	CachedRayPickY = 0u;
}

FEditorNavigationTool::FEditorNavigationTool(FEditorViewportClient* InOwner)
	: Owner(InOwner)
{
}

bool FEditorNavigationTool::HandleInput(float DeltaTime)
{
	if (!Owner || !Owner->Camera)
	{
		return false;
	}

	const bool bWasActive = IsInputActiveNow();
	const bool bLeftDragLookFrame = EditorViewportInputUtils::IsLeftNavigationDragActive(Owner->InputContext);
	const bool bMiddleLookFrame = Owner->InputContext.Frame.IsDown(VK_MBUTTON);
	const bool bRightLookFrame = Owner->InputContext.Frame.IsDown(VK_RBUTTON);
	const bool bMouseLookFrame = bLeftDragLookFrame || bMiddleLookFrame || bRightLookFrame;
	const bool bKeyboardNavigationBlocked = Owner->InputContext.bImGuiCapturedKeyboard && !bMouseLookFrame;

	const FViewportCameraState& CameraState = Owner->Camera->GetCameraState();
	const bool bIsOrtho = CameraState.bIsOrthogonal;
	const float MoveSensitivity = Owner->RenderOptions.CameraMoveSensitivity;
	const float CameraSpeed = (Owner->Settings ? Owner->Settings->CameraSpeed : 10.f) * MoveSensitivity;

	if (!bIsOrtho)
	{
		FVector Move = FVector(0, 0, 0);
		if (!bKeyboardNavigationBlocked)
		{
			if (Owner->InputContext.Frame.IsDown('W')) Move.X += CameraSpeed;
			if (Owner->InputContext.Frame.IsDown('A')) Move.Y -= CameraSpeed;
			if (Owner->InputContext.Frame.IsDown('S')) Move.X -= CameraSpeed;
			if (Owner->InputContext.Frame.IsDown('D')) Move.Y += CameraSpeed;
			if (Owner->InputContext.Frame.IsDown('Q')) Move.Z -= CameraSpeed;
			if (Owner->InputContext.Frame.IsDown('E')) Move.Z += CameraSpeed;
		}
		Move *= DeltaTime;
		Owner->Camera->MoveLocal(Move);

		FVector Rotation = FVector(0, 0, 0);
		const float RotateSensitivity = Owner->RenderOptions.CameraRotateSensitivity;
		const float AngleVelocity = (Owner->Settings ? Owner->Settings->CameraRotationSpeed : 60.f) * RotateSensitivity;
		if (!bKeyboardNavigationBlocked)
		{
			if (Owner->InputContext.Frame.IsDown(VK_UP)) Rotation.Z -= AngleVelocity;
			if (Owner->InputContext.Frame.IsDown(VK_LEFT)) Rotation.Y -= AngleVelocity;
			if (Owner->InputContext.Frame.IsDown(VK_DOWN)) Rotation.Z += AngleVelocity;
			if (Owner->InputContext.Frame.IsDown(VK_RIGHT)) Rotation.Y += AngleVelocity;
		}

		FVector MouseRotation = FVector(0, 0, 0);
		const float MouseRotationSpeed = 0.15f * RotateSensitivity;
		const bool bLookInputDown = bMouseLookFrame;
		if (bLookInputDown)
		{
			const float DeltaX = static_cast<float>(Owner->InputContext.Frame.MouseDelta.x);
			const float DeltaY = static_cast<float>(Owner->InputContext.Frame.MouseDelta.y);
			MouseRotation.Y += DeltaX * MouseRotationSpeed;
			MouseRotation.Z += DeltaY * MouseRotationSpeed;
			MouseRotation.Y = Clamp(MouseRotation.Y, -89.0f, 89.0f);
			MouseRotation.Z = Clamp(MouseRotation.Z, -89.0f, 89.0f);
		}

		Rotation *= DeltaTime;
		Owner->Camera->Rotate(Rotation.Y + MouseRotation.Y, Rotation.Z + MouseRotation.Z);
	}
	else
	{
		const bool bLookInputDown = bMouseLookFrame;
		if (bLookInputDown)
		{
			const float DeltaX = static_cast<float>(Owner->InputContext.Frame.MouseDelta.x);
			const float DeltaY = static_cast<float>(Owner->InputContext.Frame.MouseDelta.y);
			const float PanScale = CameraState.OrthoWidth * 0.002f * MoveSensitivity;
			Owner->Camera->MoveLocal(FVector(0, -DeltaX * PanScale, DeltaY * PanScale));
		}
	}

	return bWasActive || IsInputActiveNow();
}

bool FEditorNavigationTool::IsInputActiveNow() const
{
	if (!Owner)
	{
		return false;
	}

	return Owner->InputContext.Frame.IsDown(VK_RBUTTON)
		|| Owner->InputContext.Frame.IsDown(VK_MBUTTON)
		|| EditorViewportInputUtils::IsLeftNavigationDragActive(Owner->InputContext)
		|| Owner->InputContext.Frame.IsDown('W') || Owner->InputContext.Frame.IsDown('A')
		|| Owner->InputContext.Frame.IsDown('S') || Owner->InputContext.Frame.IsDown('D')
		|| Owner->InputContext.Frame.IsDown('Q') || Owner->InputContext.Frame.IsDown('E')
		|| Owner->InputContext.Frame.IsDown(VK_UP) || Owner->InputContext.Frame.IsDown(VK_DOWN)
		|| Owner->InputContext.Frame.IsDown(VK_LEFT) || Owner->InputContext.Frame.IsDown(VK_RIGHT)
		|| (Owner->InputContext.Frame.WheelNotches != 0.0f);
}

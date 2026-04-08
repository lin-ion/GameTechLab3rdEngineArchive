#include "Editor/Input/EditorGizmoTool.h"

#include "Editor/Input/EditorViewportInputMapping.h"
#include "Editor/EditorEngine.h"
#include "Editor/Gizmo/TransformGizmo.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Engine/Runtime/Engine.h"
#include "GameFramework/World.h"
#include "Viewport/Viewport.h"

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

	const auto IsActionTriggered = [this](EditorViewportInputMapping::EEditorViewportAction Action)
	{
		return EditorViewportInputMapping::IsTriggered(Owner->InputContext, Action);
	};

	if (Owner->InputContext.bImGuiCapturedMouse && !Owner->InputContext.bCaptured && !Owner->Gizmo->IsHolding())
	{
		return true;
	}

	const float ZoomSpeed = Owner->Settings ? Owner->Settings->CameraZoomSpeed : 300.f;
	const float ScrollNotches = Owner->InputContext.Frame.WheelNotches;
	const bool bRmbSpeedAdjustFrame =
		ScrollNotches != 0.0f
		&& EditorViewportInputMapping::IsTriggered(Owner->InputContext, EditorViewportInputMapping::EEditorViewportAction::NavLookRightDown);
	if (ScrollNotches != 0.0f)
	{
		if (!bRmbSpeedAdjustFrame)
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
	}

	const float LocalMouseX = static_cast<float>(Owner->InputContext.MouseLocalPos.x);
	const float LocalMouseY = static_cast<float>(Owner->InputContext.MouseLocalPos.y);

	const float VPWidth = Owner->Viewport ? static_cast<float>(Owner->Viewport->GetWidth()) : Owner->WindowWidth;
	const float VPHeight = Owner->Viewport ? static_cast<float>(Owner->Viewport->GetHeight()) : Owner->WindowHeight;
	const FRay Ray = Owner->Camera->DeprojectScreenToWorld(LocalMouseX, LocalMouseY, VPWidth, VPHeight);

	FHitResult HoverHit{};
	Owner->Gizmo->Raycast(Ray, HoverHit);

	if (IsActionTriggered(EditorViewportInputMapping::EEditorViewportAction::GizmoPrimaryPressed))
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
	else if (IsActionTriggered(EditorViewportInputMapping::EEditorViewportAction::GizmoPrimaryDown))
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
	else if (IsActionTriggered(EditorViewportInputMapping::EEditorViewportAction::GizmoPrimaryReleased))
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

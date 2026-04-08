#include "Editor/Input/EditorViewportInputUtils.h"

namespace
{
constexpr int EditorNavigationDragThreshold = 5;
}

bool EditorViewportInputUtils::IsLeftNavigationDragActive(const FViewportInputContext& Context)
{
	// NOTE: LMB navigation is intentionally on hold.
	// Do not delete this path yet; the team wants to revisit it after gizmo/input conflicts are resolved.
	(void)Context;
	return false;

	/*
	if (!Context.Frame.IsDown(VK_LBUTTON) && !Context.WasPointerDragEnded(EPointerButton::Left))
	{
		return false;
	}

	FPointerGesture LeftGesture;
	if (Context.GetPointerGesture(EPointerButton::Left, LeftGesture)
		&& (LeftGesture.bStarted || LeftGesture.bActive || LeftGesture.bEnded))
	{
		return true;
	}

	const LONG DragX = LeftGesture.TotalDelta.x;
	const LONG DragY = LeftGesture.TotalDelta.y;
	return (DragX * DragX + DragY * DragY) >= (EditorNavigationDragThreshold * EditorNavigationDragThreshold);
	*/
}

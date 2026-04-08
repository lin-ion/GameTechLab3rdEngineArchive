#include "Editor/Input/EditorViewportInputUtils.h"

namespace
{
constexpr int EditorNavigationDragThreshold = 5;
}

bool EditorViewportInputUtils::IsLeftNavigationDragActive(const FViewportInputContext& Context)
{
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
}

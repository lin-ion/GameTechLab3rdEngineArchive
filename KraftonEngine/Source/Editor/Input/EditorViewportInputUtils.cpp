#include "Editor/Input/EditorViewportInputUtils.h"

namespace
{
constexpr int EditorNavigationDragThreshold = 5;
}

bool EditorViewportInputUtils::IsLeftNavigationDragActive(const FViewportInputContext& Context)
{
	if (!Context.Frame.IsDown(VK_LBUTTON) && !Context.Frame.bLeftDragEnded)
	{
		return false;
	}

	if (Context.Frame.bLeftDragStarted || Context.Frame.bLeftDragging || Context.Frame.bLeftDragEnded)
	{
		return true;
	}

	const LONG DragX = Context.Frame.LeftDragVector.x;
	const LONG DragY = Context.Frame.LeftDragVector.y;
	return (DragX * DragX + DragY * DragY) >= (EditorNavigationDragThreshold * EditorNavigationDragThreshold);
}

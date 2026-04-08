#include "ObjViewer/ObjViewerViewportTools.h"

#include "ObjViewer/ObjViewerViewportClient.h"
#include "Math/MathUtils.h"

FObjViewerNavigationTool::FObjViewerNavigationTool(FObjViewerViewportClient* InOwner)
	: Owner(InOwner)
{
}

bool FObjViewerNavigationTool::HandleInput(float DeltaTime)
{
	if (!Owner)
	{
		return false;
	}

	(void)DeltaTime;
	if (!Owner->Camera)
	{
		return false;
	}
	if (Owner->InputContext.bImGuiCapturedKeyboard || Owner->InputContext.bImGuiCapturedMouse)
	{
		return false;
	}

	POINT MousePos = Owner->InputContext.MouseClientPos;
	const float MX = static_cast<float>(MousePos.x);
	const float MY = static_cast<float>(MousePos.y);
	const bool bMouseInViewport =
		(MX >= Owner->ViewportX && MX <= Owner->ViewportX + Owner->ViewportWidth
			&& MY >= Owner->ViewportY && MY <= Owner->ViewportY + Owner->ViewportHeight);
	if (!bMouseInViewport)
	{
		return false;
	}

	if (Owner->InputContext.Frame.IsDown(VK_RBUTTON))
	{
		const float DeltaX = static_cast<float>(Owner->InputContext.MouseLocalDelta.x);
		const float DeltaY = static_cast<float>(Owner->InputContext.MouseLocalDelta.y);
		Owner->OrbitYaw += DeltaX * 0.3f;
		Owner->OrbitPitch += DeltaY * 0.3f;
		Owner->OrbitPitch = Clamp(Owner->OrbitPitch, -89.0f, 89.0f);
	}

	if (Owner->InputContext.Frame.IsDown(VK_MBUTTON))
	{
		const float DeltaX = static_cast<float>(Owner->InputContext.MouseLocalDelta.x);
		const float DeltaY = static_cast<float>(Owner->InputContext.MouseLocalDelta.y);
		const float PanScale = Owner->OrbitDistance * 0.002f;
		const FVector Right = Owner->Camera->GetRightVector();
		const FVector Up = Owner->Camera->GetUpVector();
		Owner->OrbitTarget = Owner->OrbitTarget - Right * (DeltaX * PanScale) + Up * (DeltaY * PanScale);
	}

	const float ScrollNotches = Owner->InputContext.Frame.WheelNotches;
	if (ScrollNotches != 0.0f)
	{
		Owner->OrbitDistance -= ScrollNotches * Owner->OrbitDistance * 0.1f;
		Owner->OrbitDistance = Clamp(Owner->OrbitDistance, 0.1f, 500.0f);
	}

	const bool bMouseInput = Owner->InputContext.Frame.IsDown(VK_RBUTTON)
		|| Owner->InputContext.Frame.IsDown(VK_MBUTTON)
		|| (Owner->InputContext.Frame.WheelNotches != 0.0f);
	return bMouseInput;
}

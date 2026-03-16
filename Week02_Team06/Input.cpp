#include "pch.h"
#include "Input.h"

bool UInput::Initialize()
{
	PreKeys.fill(false);
	CurKeys.fill(false);

	return true;
}

void UInput::Update()
{
	ImGuiIO& io = ImGui::GetIO();

	HWND hWnd = GetActiveWindow();
	bool isAppFocused = (hWnd == GetForegroundWindow());

	for (int i = 0; i < 256; ++i)
	{
		PreKeys[i] = CurKeys[i];

		bool isUiBusy = (i < 0x07) ? io.WantCaptureMouse : io.WantCaptureKeyboard;

		if (isUiBusy || !isAppFocused)
		{
			CurKeys[i] = false;
		}
		else
		{
			CurKeys[i] = ((GetAsyncKeyState(i) & 0x8000) != 0);
		}
	}

	// Mouse Wheel
	if (io.WantCaptureMouse || !isAppFocused)
	{
		CurrentMouseWheelDelta = 0.0f;
		AccumulatedMouseWheelDelta = 0.0f;
	}
	else
	{
		CurrentMouseWheelDelta = AccumulatedMouseWheelDelta;
		AccumulatedMouseWheelDelta = 0.0f;
	}
}
void UInput::Release()
{

}

bool UInput::IsKeyDown(int vKey)
{
	return (!PreKeys[vKey] && CurKeys[vKey]);
}

bool UInput::IsKeyPressing(int vKey)
{
	return  (PreKeys[vKey] && CurKeys[vKey]);
}

bool UInput::IsKeyUp(int vKey)
{
	return (PreKeys[vKey] && !CurKeys[vKey]);
}

// Called from Window::WndProc when handling WM_MOUSEMOVE message
void UInput::UpdateMousePosition(POINT MousePos)
{
	MousePosition.x = MousePos.x;
	MousePosition.y = MousePos.y;
}

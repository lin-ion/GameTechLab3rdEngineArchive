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

	for (int i = 0; i < 256; ++i)
	{
		PreKeys[i] = CurKeys[i];

		bool isUiBusy = (i < 0x07) ? io.WantCaptureMouse : io.WantCaptureKeyboard;

		if (isUiBusy)
		{
			CurKeys[i] = false;
		}
		else
		{
			CurKeys[i] = ((GetAsyncKeyState(i) & 0x8000) != 0);
		}
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
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
	for (int i = 0; i < 256; ++i)
	{
		PreKeys[i] = CurKeys[i];
		CurKeys[i] = ((GetAsyncKeyState(i) & 0x8000) != 0);
	}
}

void UInput::Release()
{

}

bool UInput::IsKeyDown(int vKey)
{
	return (!PreKeys[vKey] && CurKeys[vKey]);
}

bool UInput::IsKeyHeld(int vKey)
{
	return  (PreKeys[vKey] && CurKeys[vKey]);
}

bool UInput::IsKeyUp(int vKey)
{
	return (PreKeys[vKey] && !CurKeys[vKey]);
}
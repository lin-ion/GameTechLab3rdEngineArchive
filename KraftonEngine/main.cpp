#include <Windows.h>
#include "Engine/Runtime/Launch.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int nShowCmd)
{
	return Launch(hInstance, nShowCmd);
}

// 5주 Competition 종료 이후, Preprocessor Definition의 FOR_COMPETITION 제거

#include "pch.h"
#include "UWindow.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool UWindow::Initialize(HINSTANCE _hInstance, uint32 _width, uint32 _height)
{
	hInstance = _hInstance;

	WNDCLASSW wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"JungleWindowClass";

	if (!RegisterClassW(&wc))
		return false;

	DWORD Style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

	RECT rect = { 0, 0, (LONG)_width, (LONG)_height };
	AdjustWindowRect(&rect, Style, FALSE);

	hWnd = CreateWindowExW(0,
		L"JungleWindowClass",
		L"Game Tech Lab",
		Style,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rect.right - rect.left,
		rect.bottom - rect.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	if (!hWnd)
		return false;

	ShowWindow(hWnd, SW_SHOW);

	return true;
}

void UWindow::Release()
{
	if (hWnd)
	{
		DestroyWindow(hWnd);
		hWnd = nullptr;
	}
}


LRESULT CALLBACK UWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
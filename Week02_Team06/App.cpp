#include "pch.h"
#include "App.h"

#include "Window.h"
#include "Renderer.h"
#include "Input.h"
#include "Graphics.h"
#include "SceneManager.h"
#include "ImGuiDrawer.h"

bool UApp::Initialize(HINSTANCE hInstance)
{
	Window = new UWindow;
	if (!Window->Initialize(hInstance, WindowSizeWidth, WindowSizeHeight))
	{
		return false;
	}

	Graphics = new UGraphics;
	Graphics->Initialize(Window->GetHWnd());

	Renderer = new URenderer(Graphics->GetDevice(), Graphics->GetDeviceContext(), Graphics->GetSwapChain());
	Renderer->Initialize();

	SceneManager = new USceneManager;
	SceneManager->Initialize(*Graphics->GetDevice());

	ImGuiDrawer = new UImGuiDrawer;
	ImGuiDrawer->Initialize(Window->GetHWnd(), Graphics->GetDevice(), Graphics->GetDeviceContext());

	UE_LOG("Hello World");

	return true;
}

void UApp::Run()
{
	MSG msg = {};

	LARGE_INTEGER StartTime, EndTime, Frequency;

	double		  TargetFrameMilliSecond = 1.f / TargetFrame * 1000.f;
	double		  ElaspedMilliSecond = 0.f;

	QueryPerformanceFrequency(&Frequency);
	QueryPerformanceCounter(&StartTime);

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}


		QueryPerformanceCounter(&EndTime);
		double CounterInterval = static_cast<double>(EndTime.QuadPart - StartTime.QuadPart);
		ElaspedMilliSecond = CounterInterval / Frequency.QuadPart * 1000.f;

		if (ElaspedMilliSecond >= TargetFrameMilliSecond)
		{
			StartTime = EndTime;
			DeltaTime = static_cast<float>(ElaspedMilliSecond / 1000.f); // 초단위로

			//input
			UInput::GetInstance().Update();

			//GameLogic
			SceneManager->Update(DeltaTime);

			//Render
			Graphics->ClearRenderTarget();

			Renderer->BeginScene();
			Renderer->Render(SceneManager->GetCurrentScene());

			//ImGui
			ImGuiDrawer->BeginFrame();
			ImGuiDrawer->UpdateUI(SceneManager->GetCurrentScene());
			ImGuiDrawer->EndFrame();

			Renderer->EndScene();
		}
		else
		{
			Sleep(0);
		}

	}
}

void UApp::Release()
{
	if (SceneManager)
	{
		SceneManager->Release();
		delete SceneManager;
	}
	if (Renderer)
	{
		Renderer->Release();
		delete Renderer;
	}
	if (Graphics)
	{
		Graphics->Release();
		delete Graphics;
	}

	if (Window)
	{
		Window->Release();
		delete Window;
	}

	if (ImGuiDrawer)
	{
		ImGuiDrawer->Release();
		delete ImGuiDrawer;
	}
}
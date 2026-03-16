#include "pch.h"
#include "App.h"

#include "Window.h"
#include "Renderer.h"
#include "Input.h"
#include "Graphics.h"

#include "ImGuiDrawer.h"
#include "ResourceManager.h"
#include "FEditorViewportClient.h"
#include "World.h"
#include "ObjectFactory.h"
#include "Actor.h"
#include "GizmoComponent.h"

bool UApp::Initialize(HINSTANCE hInstance)
{
	Window = new UWindow;
	if (!Window->Initialize(hInstance, WindowSizeWidth, WindowSizeHeight))
	{
		return false;
	}

	Graphics = new UGraphics;
	Graphics->Initialize(Window->GetHWnd());

	float AspectRatio = static_cast<float>(WindowSizeWidth) / WindowSizeHeight;
	ViewportClient = new FEditorViewportClient({ 10.0f, 10.0f, 10.0f }, { 30.0f, -120.0f, 0.0f }, AspectRatio, 60.f);

	Renderer = new URenderer(Graphics->GetDevice(), Graphics->GetDeviceContext(), Graphics->GetSwapChain(), *ViewportClient);
	Renderer->Initialize();

	ResourceManager = new UResourceManager;
	ResourceManager->Initialize(*Graphics->GetDevice());

	World = UObjectFactory::NewObject<UWorld>();
	World->InitWorld(*ResourceManager, ViewportClient);

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
	double		  ElapsedMilliSecond = 0.f;

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
		ElapsedMilliSecond = CounterInterval / Frequency.QuadPart * 1000.f;

		if (ElapsedMilliSecond >= TargetFrameMilliSecond)
		{
			StartTime = EndTime;
			DeltaTime = static_cast<float>(ElapsedMilliSecond / 1000.f); // 초단위로

			//input
			UInput::GetInstance().Update();

			// 여기까지 Test용
			ViewportClient->Tick(DeltaTime);
			//GameLogic
			World->Tick(DeltaTime);

			//Render
			Graphics->ClearRenderTarget();

			Renderer->BeginScene();
			Renderer->Render(World);

			//ImGui
			ImGuiDrawer->BeginFrame();
			ImGuiDrawer->UpdateUI(ViewportClient);
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
	if (World)
	{
		World->Release();
	}

	if (ImGuiDrawer)
	{
		ImGuiDrawer->Release();
		delete ImGuiDrawer;
		ImGuiDrawer = nullptr;
	}

	if (Renderer)
	{
		Renderer->Release();
		delete Renderer;
	}

	UObjectFactory::DestroyAllObjects();

	if (ViewportClient)
	{
		delete ViewportClient;
	}

	if (Graphics)
	{
		Graphics->Release();
		delete Graphics;
	}

	if (ResourceManager)
	{
		ResourceManager->Release();
		delete ResourceManager;
	}
	if (Window)
	{
		Window->Release();
		delete Window;
	}
}
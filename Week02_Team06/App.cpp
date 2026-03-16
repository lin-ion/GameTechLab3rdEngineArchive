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

// Test
#include "SceneSerializer.h"

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
	ViewportClient = new FEditorViewportClient({ 10.0f, 10.0f, -10.0f }, { 45.0f, -45.0f, 0.0f }, AspectRatio, 60.f);

	ViewportClient->SetAspectRatio(static_cast<float>(WindowSizeWidth) / WindowSizeHeight);

	Renderer = new URenderer(Graphics->GetDevice(), Graphics->GetDeviceContext(), Graphics->GetSwapChain(), *ViewportClient);
	Renderer->Initialize();

	ResourceManager = new UResourceManager;
	ResourceManager->Initialize(*Graphics->GetDevice());

	World = UObjectFactory::NewObject<UWorld>();
	World->InitWorld(*ResourceManager, ViewportClient);

	ImGuiDrawer = new UImGuiDrawer;
	ImGuiDrawer->Initialize(Window->GetHWnd(), Graphics->GetDevice(), Graphics->GetDeviceContext(), World);


	// console Test Code
	UE_LOG("Hello World");


	// json Test Code
	/*
	UScene* scene = SceneManager->GetCurrentScene();
	scene->data.Version = 1;
	scene->data.NextUUID = 8;
	USceneSerializer::SaveScene("Test", scene->data);
	USceneSerializer::LoadScene("Test", scene->data);

	UE_LOG("Version: %d", scene->data.Version);
	UE_LOG("NextUUID: %d", scene->data.NextUUID);
	* 
	*/

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

			ViewportClient->Tick(DeltaTime);

			//GameLogic
			World->Tick(DeltaTime);

			//Render
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

	for (uint64 i = 0; i < GUObjectArray.Size(); ++i)
	{
		GUObjectArray[i]->Release();
		delete GUObjectArray[i];
	}

	if (ViewportClient)
	{
		delete ViewportClient;
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
}
#include "pch.h"
#include "App.h"

#include "Window.h"
#include "Renderer.h"
#include "Input.h"
#include "Graphics.h"a

#include "ImGuiDrawer.h"
#include "ResourceManager.h"
#include "FEditorViewportClient.h"
#include "World.h"
#include "ObjectFactory.h"
#include "Actor.h"
#include "GizmoComponent.h"

UApp* GApp = nullptr;

bool UApp::Initialize(HINSTANCE hInstance)
{
	GApp = this;
	
	Window = new UWindow;
	if (!Window->Initialize(hInstance, WindowSizeWidth, WindowSizeHeight))
	{
		return false;
	}

	Graphics = new UGraphics;
	Graphics->Initialize(Window->GetHWnd());

	ViewportClient = new FEditorViewportClient;

	Renderer = new URenderer(Graphics->GetDevice(), Graphics->GetDeviceContext(), Graphics->GetSwapChain(), *ViewportClient);
	Renderer->Initialize();

	ResourceManager = new UResourceManager;
	ResourceManager->Initialize(*Graphics->GetDevice());

	World = UObjectFactory::NewObject<UWorld>();
	World->InitWorld(*ResourceManager);
	
	AActor* GizmoActor = World->SpawnActor<AActor>();

	// 기즈모 부품 생성 및 장착
	UGizmoComponent* GizmoComp = GizmoActor->AddComponent<UGizmoComponent>();
	GizmoComp->SetPosition({ 0.f, 0.f, 0.f });

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
	if (ImGuiDrawer) 
	{
		ImGuiDrawer->Release();
		delete ImGuiDrawer;
		ImGuiDrawer = nullptr;
	}

	if (ResourceManager) ResourceManager->Release();
	if (Renderer)        Renderer->Release();
	if (World)           World->Release();

	while (GUObjectArray.Size() > 0)
	{
		int LastIdx = (int)GUObjectArray.Size() - 1;
		UObject* Target = GUObjectArray[LastIdx];

		if (Target)
		{
			UObjectFactory::DestroyObject(Target); // 내부에서 PopBack/교체 후 Release, delete 수행
		}
		else
		{
			GUObjectArray.PopBack();
		}
	}

	if (ViewportClient) { delete ViewportClient; ViewportClient = nullptr; }
	if (Graphics) { Graphics->Release(); delete Graphics; Graphics = nullptr; }
	if (Window) { Window->Release(); delete Window; Window = nullptr; }
}
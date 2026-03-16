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

// Test
#include "SceneSerializer.h"

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

	float AspectRatio = static_cast<float>(WindowSizeWidth) / WindowSizeHeight;
	ViewportClient = new FEditorViewportClient({ 10.0f, 10.0f, 10.0f }, { 30.0f, -120.0f, 0.0f }, AspectRatio, 60.f);

	Renderer = new URenderer(Graphics->GetDevice(), Graphics->GetDeviceContext(), Graphics->GetSwapChain(), *ViewportClient);
	Renderer->Initialize();

	ResourceManager = new UResourceManager;
	ResourceManager->Initialize(*Graphics->GetDevice());

	World = UObjectFactory::NewObject<UWorld>();

	World->InitWorld(ResourceManager);

	AActor* GizmoActor = World->SpawnActor<AActor>();

	// Gizmo Picking Test용
	MainGizmo = GizmoActor->AddComponent<UGizmoComponent>();
	MainGizmo->SetPosition({ 0.f, 0.f, 0.f });

	// 기즈모 부품 생성 및 장착
	UGizmoComponent* GizmoComp = GizmoActor->AddComponent<UGizmoComponent>();
	GizmoComp->SetPosition({ 0.f, 0.f, 0.f });

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

			// Gizmo Picking Test용
			if (UInput::GetInstance().IsKeyDown(VK_LBUTTON))
			{
				// 임시 피킹 부품을 생성하여 검수를 의뢰합니다.
				static UPickingComponent TestPicker;

				if (MainGizmo)
				{
					EGizmoAxis Picked = MainGizmo->CheckGizmoPicking(&TestPicker);

					// 결과에 따른 로그 출력 (Visual Studio 출력창에서 확인 가능)
					if (Picked != EGizmoAxis::None)
					{
						std::string AxisName[] = { "None", "Center", "X", "Y", "Z" };
						std::string Msg = "Gizmo Picked: " + AxisName[(int)Picked] + "\n";
						OutputDebugStringA(Msg.c_str());
					}
				}
			}
			// 여기까지 Test용

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
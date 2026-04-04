#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/World.h"
#include "GameFramework/Scene.h"
#include "Render/Pipeline/WorldRenderProxy.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Pipeline/Renderer.h"
#include "Engine/Input/InputSystem.h"

/* CULLING DEBUG */
namespace
{
	void DrawCullingDebugOverlayImGui(UEditorEngine* Editor)
	{
		static bool bShowCullingDebug = true;

		if (!Editor)
		{
			return;
		}

		UWorld* World = Editor->GetWorld();
		if (!World)
		{
			return;
		}

		FWorldProxyCullingStats Total = {};

		auto AccumulateScene = [&Total](UScene* Scene)
		{
			if (!Scene)
			{
				return;
			}

			const FWorldProxyCullingStats& Stats = Scene->GetRenderProxy().GetLastCullingStats();
			Total.RegisteredProxyCount += Stats.RegisteredProxyCount;
			Total.InsertedProxyCount += Stats.InsertedProxyCount;
			Total.CandidateProxyCount += Stats.CandidateProxyCount;
			Total.RenderedProxyCount += Stats.RenderedProxyCount;
			Total.OctreeTotalNodes += Stats.OctreeTotalNodes;
			Total.OctreeTotalItems += Stats.OctreeTotalItems;
			Total.OctreeOutsideItems += Stats.OctreeOutsideItems;
			Total.OctreeFrustumIntersectedNodes += Stats.OctreeFrustumIntersectedNodes;
			Total.OctreeFrustumCandidateItems += Stats.OctreeFrustumCandidateItems;
		};

		AccumulateScene(World->GetPersistentScene());
		AccumulateScene(World->GetActiveScene());

		ImGui::SetNextWindowPos(ImVec2(1300.0f, 30.0f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.35f);

		const ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration
			| ImGuiWindowFlags_AlwaysAutoResize
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoFocusOnAppearing
			| ImGuiWindowFlags_NoNav
			| ImGuiWindowFlags_NoMove;

		if (ImGui::Begin("Culling Debug Overlay", nullptr, Flags))
		{
			ImGui::TextUnformatted("[Culling Debug]");
			ImGui::Separator();
			ImGui::Text("Registered: %d  Inserted: %d  Candidates: %d  Rendered: %d",
				Total.RegisteredProxyCount,
				Total.InsertedProxyCount,
				Total.CandidateProxyCount,
				Total.RenderedProxyCount);

			ImGui::Text("Octree Nodes (Total/InFrustum): %d / %d",
				Total.OctreeTotalNodes,
				Total.OctreeFrustumIntersectedNodes);

			ImGui::Text("Octree Items (Total/Outside/InFrustum): %d / %d / %d",
				Total.OctreeTotalItems,
				Total.OctreeOutsideItems,
				Total.OctreeFrustumCandidateItems);
		}
		ImGui::End();
	}
}

void FEditorMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	Window = InWindow;
	EditorEngine = InEditorEngine;

	// 한글 지원 폰트 로드 (시스템 맑은 고딕)
	IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

	ConsoleWidget.Initialize(InEditorEngine);
	ControlWidget.Initialize(InEditorEngine);
	PropertyWidget.Initialize(InEditorEngine);
	SceneWidget.Initialize(InEditorEngine);
	StatWidget.Initialize(InEditorEngine);
}

void FEditorMainPanel::Release()
{
	ConsoleWidget.Clear();
	FEditorConsoleWidget::ClearHistory();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FEditorMainPanel::Render(float DeltaTime)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	// 뷰포트 렌더링은 EditorEngine이 담당 (SSplitter 레이아웃 + ImGui::Image)
	if (EditorEngine)
	{
		EditorEngine->RenderViewportUI(DeltaTime);
	}
	ConsoleWidget.Render(DeltaTime);
	ControlWidget.Render(DeltaTime);
	SceneWidget.Render(DeltaTime);
	PropertyWidget.Render(DeltaTime);
	StatWidget.Render(DeltaTime);
	DrawCullingDebugOverlayImGui(EditorEngine);
	// 뷰포트 렌더링은 EditorEngine이 담당 (SSplitter 레이아웃 + ImGui::Image)


	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::Update()
{
	ImGuiIO& IO = ImGui::GetIO();

	// 뷰포트 슬롯 위에서는 bUsingMouse를 해제해야 TickInteraction이 동작
	bool bWantMouse = IO.WantCaptureMouse;
	if (EditorEngine && EditorEngine->IsMouseOverViewport())
	{
		bWantMouse = false;
	}
	InputSystem::Get().GetGuiInputState().bUsingMouse = bWantMouse;
	InputSystem::Get().GetGuiInputState().bUsingKeyboard = IO.WantCaptureKeyboard;

	// IME는 ImGui가 텍스트 입력을 원할 때만 활성화.
	if (Window)
	{
		HWND hWnd = Window->GetHWND();
		if (IO.WantTextInput)
		{
			ImmAssociateContextEx(hWnd, NULL, IACE_DEFAULT);
		}
		else
		{
			ImmAssociateContext(hWnd, NULL);
		}
	}
}

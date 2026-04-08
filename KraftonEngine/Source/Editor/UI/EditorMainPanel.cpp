#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"
#include "Editor/Selection/PickingTypes.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Platform/Paths.h"
#include "GameFramework/World.h"
#include "GameFramework/Level.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"
#include "Render/Pipeline/OcclusionManager.h"

#include "Render/Pipeline/Renderer.h"


void FEditorMainPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	IO.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	IO.MouseDrawCursor = false;

	Window = InWindow;
	EditorEngine = InEditorEngine;

	// 한글 지원 폰트 로드 (시스템 맑은 고딕)
	IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());

	ConsoleWidget.Initialize(InEditorEngine);
	ControlWidget.Initialize(InEditorEngine);
	PropertyWidget.Initialize(InEditorEngine);
	LevelWidget.Initialize(InEditorEngine);
	// StatWidget.Initialize(InEditorEngine);
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

	RenderMainMenuBar();
	RenderPIEToolbar();
	RenderDockSpace();

	// 뷰포트 렌더링은 EditorEngine이 담당 (SSplitter 레이아웃 + ImGui::Image)
	if (EditorEngine)
	{
		EditorEngine->RenderViewportUI(DeltaTime);
	}
	if (bShowControlPanel)
	{
		ControlWidget.Render(DeltaTime);
	}
	if (bShowLevelPanel)
	{
		LevelWidget.Render(DeltaTime);
	}
	if (bShowPropertyPanel)
	{
		PropertyWidget.Render(DeltaTime);
	}
	if (bShowStatPanel)
	{
		StatWidget.Render(DeltaTime);
	}
	RenderEditorDebugPanel();

	float EffectiveDeltaTime = DeltaTime;
	if (EffectiveDeltaTime <= 0.0f)
	{
		EffectiveDeltaTime = ImGui::GetIO().DeltaTime;
		if (EffectiveDeltaTime <= 0.0f)
		{
			EffectiveDeltaTime = 1.0f / 60.0f;
		}
	}
	const float TargetAnim = bConsoleDrawerVisible ? 1.0f : 0.0f;
	const float AnimSpeed = 8.0f;
	if (ConsoleDrawerAnim < TargetAnim)
	{
		ConsoleDrawerAnim += EffectiveDeltaTime * AnimSpeed;
		if (ConsoleDrawerAnim > 1.0f)
		{
			ConsoleDrawerAnim = 1.0f;
		}
	}
	else if (ConsoleDrawerAnim > TargetAnim)
	{
		ConsoleDrawerAnim -= EffectiveDeltaTime * AnimSpeed;
		if (ConsoleDrawerAnim < 0.0f)
		{
			ConsoleDrawerAnim = 0.0f;
		}
	}

	RenderShortcutOverlay();
	RenderConsoleDrawer();
	RenderFooterOverlay(DeltaTime);
	// 뷰포트 렌더링은 EditorEngine이 담당 (SSplitter 레이아웃 + ImGui::Image)


	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::RenderDockSpace()
{
	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	if (!MainViewport)
	{
		return;
	}

	constexpr float PIEBarHeight = 60.0f;
	constexpr float FooterHeight = 32.0f;
	const ImVec2 DockPos(MainViewport->WorkPos.x, MainViewport->WorkPos.y + PIEBarHeight);
	const ImVec2 DockSize(
		MainViewport->WorkSize.x,
		(MainViewport->WorkSize.y > (FooterHeight + PIEBarHeight)) ? (MainViewport->WorkSize.y - FooterHeight - PIEBarHeight) : 0.0f);

	ImGui::SetNextWindowPos(DockPos);
	ImGui::SetNextWindowSize(DockSize);
	ImGui::SetNextWindowViewport(MainViewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	const ImGuiWindowFlags DockHostFlags =
		ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoTitleBar
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoBringToFrontOnFocus
		| ImGuiWindowFlags_NoNavFocus
		| ImGuiWindowFlags_NoBackground;
	ImGui::Begin("##MainDockHost", nullptr, DockHostFlags);
	ImGui::PopStyleVar(3);

	const ImGuiID DockSpaceId = ImGui::GetID("MainDockSpace");
	ImGui::DockSpace(DockSpaceId, ImVec2(0.0f, 0.0f));
	ImGui::End();
}

void FEditorMainPanel::RenderPIEToolbar()
{
	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	if (!MainViewport)
	{
		return;
	}

	constexpr float PIEBarHeight = 60.0f;
	const ImVec2 BarPos = MainViewport->WorkPos;
	const ImVec2 BarSize(MainViewport->WorkSize.x, PIEBarHeight);

	ImGui::SetNextWindowPos(BarPos, ImGuiCond_Always);
	ImGui::SetNextWindowSize(BarSize, ImGuiCond_Always);
	ImGui::SetNextWindowViewport(MainViewport->ID);

	const ImGuiWindowFlags BarFlags =
		ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoNav
		| ImGuiWindowFlags_NoFocusOnAppearing;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.12f, 0.98f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.19f, 0.19f, 0.22f, 1.0f));

	if (ImGui::Begin("##EditorPIEToolbar", nullptr, BarFlags))
	{
		const float IconButtonSize = 36.0f;
		const float Gap = 10.0f;
		const float GroupWidth = IconButtonSize * 2.0f + Gap;
		const float StartX = (BarSize.x - GroupWidth) * 0.5f;
		const float StartY = (PIEBarHeight - IconButtonSize) * 0.5f;

		ImGui::SetCursorPos(ImVec2(StartX, StartY));
		if (ImGui::InvisibleButton("##PIEStartButton", ImVec2(IconButtonSize, IconButtonSize)))
		{
			if (!EditorEngine->IsPIEEnabled())
				EditorEngine->StartPIE();
		}
		{
			const ImVec2 Min = ImGui::GetItemRectMin();
			const ImVec2 Max = ImGui::GetItemRectMax();
			const bool bHovered = ImGui::IsItemHovered();
			const ImU32 Bg = ImGui::GetColorU32(bHovered ? ImVec4(0.20f, 0.26f, 0.20f, 1.0f) : ImVec4(0.16f, 0.20f, 0.16f, 1.0f));
			const ImU32 Border = ImGui::GetColorU32(ImVec4(0.30f, 0.36f, 0.30f, 1.0f));
			const ImU32 Icon = ImGui::GetColorU32(ImVec4(0.52f, 0.92f, 0.56f, 1.0f));
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			DrawList->AddRectFilled(Min, Max, Bg, 6.0f);
			DrawList->AddRect(Min, Max, Border, 6.0f);
			const ImVec2 C((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
			DrawList->AddTriangleFilled(
				ImVec2(C.x - 5.0f, C.y - 8.0f),
				ImVec2(C.x - 5.0f, C.y + 8.0f),
				ImVec2(C.x + 9.0f, C.y),
				Icon);
		}

		ImGui::SameLine(0.0f, Gap);
		if (ImGui::InvisibleButton("##PIEStopButton", ImVec2(IconButtonSize, IconButtonSize)))
		{
			if (EditorEngine->IsPIEEnabled())
				EditorEngine->EndPIE();
		}
		{
			const ImVec2 Min = ImGui::GetItemRectMin();
			const ImVec2 Max = ImGui::GetItemRectMax();
			const bool bHovered = ImGui::IsItemHovered();
			const ImU32 Bg = ImGui::GetColorU32(bHovered ? ImVec4(0.26f, 0.20f, 0.20f, 1.0f) : ImVec4(0.20f, 0.16f, 0.16f, 1.0f));
			const ImU32 Border = ImGui::GetColorU32(ImVec4(0.38f, 0.30f, 0.30f, 1.0f));
			const ImU32 Icon = ImGui::GetColorU32(ImVec4(0.92f, 0.48f, 0.48f, 1.0f));
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			DrawList->AddRectFilled(Min, Max, Bg, 6.0f);
			DrawList->AddRect(Min, Max, Border, 6.0f);
			const ImVec2 IconMin(Min.x + 10.0f, Min.y + 10.0f);
			const ImVec2 IconMax(Max.x - 10.0f, Max.y - 10.0f);
			DrawList->AddRectFilled(IconMin, IconMax, Icon, 2.0f);
		}
	}
	ImGui::End();

	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(3);
}

void FEditorMainPanel::RenderMainMenuBar()
{
	if (!EditorEngine)
	{
		return;
	}

	if (!ImGui::BeginMainMenuBar())
	{
		return;
	}

	if (ImGui::BeginMenu("File"))
	{
		const bool bCanSave = EditorEngine->HasCurrentLevelFilePath();

		if (ImGui::MenuItem("New Level", "Ctrl+N"))
		{
			EditorEngine->NewLevel();
		}
		if (ImGui::MenuItem("Load Level", "Ctrl+O"))
		{
			EditorEngine->LoadLevelWithDialog();
		}
		if (ImGui::MenuItem("Save Level", "Ctrl+S"))
		{
			EditorEngine->SaveLevel();
		}
		if (ImGui::MenuItem("Save Level As...", "Ctrl+Shift+S"))
		{
			EditorEngine->SaveLevelAsWithDialog();
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Open Asset Folder"))
		{
			EditorEngine->OpenAssetFolder();
		}

		ImGui::Separator();
		ImGui::BeginDisabled(true);
		ImGui::MenuItem(
			bCanSave ? "Current: Loaded Level" : "Current: Unsaved Level",
			nullptr,
			false,
			false);
		ImGui::EndDisabled();

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("View"))
	{
		const bool bMenuConsoleDrawerVisible = bConsoleDrawerVisible;
		if (ImGui::MenuItem("Console Drawer", nullptr, bMenuConsoleDrawerVisible))
		{
			bConsoleDrawerVisible = !bConsoleDrawerVisible;
			if (bConsoleDrawerVisible)
			{
				ConsoleBacktickCycleState = 2;
				bBringConsoleDrawerToFrontNextFrame = true;
				bFocusConsoleInputNextFrame = true;
			}
			else
			{
				ConsoleBacktickCycleState = 0;
				bFocusConsoleButtonNextFrame = true;
			}
		}
		ImGui::MenuItem("Control", nullptr, &bShowControlPanel);
		ImGui::MenuItem("Level Manager", nullptr, &bShowLevelPanel);
		ImGui::MenuItem("Property", nullptr, &bShowPropertyPanel);
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Settings"))
	{
		ImGui::MenuItem("Editor Debug", nullptr, &bShowEditorDebugPanel);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Help"))
	{
		if (ImGui::MenuItem("Shortcuts"))
		{
			bShowShortcutOverlay = true;
		}
		ImGui::EndMenu();
	}

	ImGui::EndMainMenuBar();
}

void FEditorMainPanel::RenderEditorDebugPanel()
{
	if (!bShowEditorDebugPanel || !EditorEngine)
	{
		return;
	}

	static const char* PickingModeTypes[2] = { "Ray-Triangle (BVH)", "ID Picking" };
	int32 SelectedPickingMode = static_cast<int32>(EditorEngine->GetPickingMode());

	ImGui::SetNextWindowSize(ImVec2(420.0f, 220.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Editor Debug", &bShowEditorDebugPanel))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Combo("Picking Mode", &SelectedPickingMode, PickingModeTypes, IM_ARRAYSIZE(PickingModeTypes)))
	{
		EditorEngine->SetPickingMode(static_cast<EPickingMode>(SelectedPickingMode));
	}

	ImGui::Separator();

	if (FLevelEditorViewportClient* ActiveVC = EditorEngine->GetActiveViewport())
	{
		auto& ShowFlags = ActiveVC->GetRenderOptions().ShowFlags;
		ImGui::Checkbox("Occlusion Culling", &ShowFlags.bOcclusionCulling);
		ImGui::Checkbox("Show HZB Debug", &ShowFlags.bShowHZB);

		if (ShowFlags.bOcclusionCulling)
		{
			uint32 Total = FOcclusionManager::Get().GetTotalCandidates();
			uint32 Culled = FOcclusionManager::Get().GetOccludedCount();
			ImGui::Text("Total Proxies: %u", Total);
			ImGui::Text("Culled Proxies: %u", Culled);
			ImGui::Text("Rendered Proxies: %u", Total - Culled);
		}
	}
	
	FEditorSettings& Settings = FEditorSettings::Get();
	ImGui::Checkbox("Enable Camera Smoothing", &Settings.bEnableCameraSmoothing);
	ImGui::DragFloat("Move Lerp Strength", &Settings.CameraMoveSmoothSpeed, 0.1f, 0.01f, 100.0f, "%.2f");
	ImGui::DragFloat("Rotate Lerp Strength", &Settings.CameraRotateSmoothSpeed, 0.1f, 0.01f, 100.0f, "%.2f");
	ImGui::Separator();

	FViewportCamera* Camera = EditorEngine->GetCamera();
	if (!Camera)
	{
		ImGui::End();
		return;
	}

	// float CameraFOV_Deg = Camera->GetFOV() * RAD_TO_DEG;
	// if (ImGui::DragFloat("Camera Zoom", &CameraFOV_Deg, 0.5f, 1.0f, 90.0f))
	// {
	// 	Camera->SetFOV(CameraFOV_Deg * DEG_TO_RAD);
	// }
	
	if (Camera->IsOrthogonal())
	{
		float OrthoWidth = Camera->GetOrthoWidth();
		if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 1000.0f))
		{
			Camera->SetOrthoWidth(Clamp(OrthoWidth, 0.1f, 1000.0f));
		}	
	}

	ImGui::End();
}

void FEditorMainPanel::RenderShortcutOverlay()
{
	if (!bShowShortcutOverlay)
	{
		return;
	}

	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const ImVec2 OverlayPos = MainViewport ? MainViewport->Pos : ImVec2(0.0f, 0.0f);
	const ImVec2 OverlaySize = MainViewport ? MainViewport->Size : ImGui::GetIO().DisplaySize;

	ImGui::SetNextWindowPos(OverlayPos);
	ImGui::SetNextWindowSize(OverlaySize);
	ImGui::SetNextWindowBgAlpha(0.38f);
	const ImGuiWindowFlags BlockerFlags =
		ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoNav
		| ImGuiWindowFlags_NoMove;
	ImGui::Begin("##ShortcutOverlayBlocker", nullptr, BlockerFlags);
	ImGui::InvisibleButton("##ShortcutOverlayBlockerBtn", OverlaySize);
	ImGui::End();

	const ImVec2 PanelSize(980.0f, 700.0f);
	ImGui::SetNextWindowPos(ImVec2(
		OverlayPos.x + (OverlaySize.x - PanelSize.x) * 0.5f,
		OverlayPos.y + (OverlaySize.y - PanelSize.y) * 0.5f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(PanelSize, ImGuiCond_Always);

	ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.14f, 0.15f, 0.18f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.16f, 0.17f, 0.20f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.28f, 0.29f, 0.32f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	const ImGuiWindowFlags PanelFlags =
		ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoCollapse
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoMove;
	bool bOpen = bShowShortcutOverlay;
	if (!ImGui::Begin("Shortcuts", &bOpen, PanelFlags))
	{
		ImGui::End();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);
		bShowShortcutOverlay = bOpen;
		return;
	}
	bShowShortcutOverlay = bOpen;

	ImGui::TextUnformatted("현재 코드상 실제로 동작하는 에디터 단축키를 정리했습니다.");
	ImGui::Separator();

	constexpr float ShortcutColumnWidth = 200.0f;
	auto DrawShortcutSection = [ShortcutColumnWidth](const char* InSectionName, const char* InTableId, const TArray<std::pair<const char*, const char*>>& InRows)
	{
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.14f, 0.23f, 0.47f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.91f, 0.98f, 1.0f));
		ImGui::Selectable(InSectionName, false, ImGuiSelectableFlags_Disabled, ImVec2(-1.0f, 22.0f));
		ImGui::PopStyleColor(2);

		if (ImGui::BeginTable(
			InTableId,
			2,
			ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoPadOuterX))
		{
			ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, ShortcutColumnWidth);
			ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted("Shortcut");
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted("Action");
			for (const auto& Row : InRows)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(Row.first);
				ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(Row.second);
			}
			ImGui::EndTable();
		}
		ImGui::Spacing();
	};

	DrawShortcutSection("▼ Viewport Navigation", "ShortcutTable_Nav", {
		{ "Mouse Right Drag", "뷰포트 카메라 회전 (Perspective)" },
		{ "Mouse Middle Drag", "뷰포트 카메라 팬 이동" },
		{ "Alt + Mouse Left Drag", "선택 대상을 기준으로 오빗 회전" },
		{ "Alt + Mouse Right Drag", "카메라 돌리 인/아웃" },
		{ "Mouse Wheel", "휠 카메라 FOV 또는 직교 카메라 줌 조절" },
		{ "Mouse Wheel while rotating", "카메라 이동 속도 조절" },
		{ "W / A / S / D / Q / E", "카메라 이동 (우클릭 중일 때만 적용)" },
		{ "F", "현재 선택된 Actor 축으로 카메라 포커스" },
	});

	DrawShortcutSection("▼ Selection", "ShortcutTable_Selection", {
		{ "Mouse Left Click", "Actor 단일 선택" },
		{ "Shift + Mouse Left Click", "선택 추가" },
		{ "Ctrl + Mouse Left Click", "선택 토글" },
		{ "Ctrl + Alt + Drag", "박스 선택" },
		{ "Ctrl + Alt + Shift + Drag", "기존 선택에 박스 선택 추가" },
		{ "Ctrl + A", "전체 선택" },
	});

	DrawShortcutSection("▼ Gizmo", "ShortcutTable_Gizmo", {
		{ "Mouse Left Drag", "기즈모 축 드래그 조작" },
		{ "Space", "기즈모 타입 순환" },
		{ "X", "월드/로컬 기즈모 모드 전환" },
	});

	DrawShortcutSection("▼ File", "ShortcutTable_File", {
		{ "Ctrl + N", "New Level" },
		{ "Ctrl + O", "Load Level" },
		{ "Ctrl + S", "Save Level (경로 없으면 Save As)" },
		{ "Ctrl + Shift + S", "Save Level As" },
	});

	DrawShortcutSection("▼ Editor", "ShortcutTable_Editor", {
		{ "Delete", "선택된 Actor 삭제" },
		{ "Tab", "Editor Mode 순환" },
	{ "Backtick(`)", "Console Mode 순환" },
	});

	ImGui::Separator();
	ImGui::TextUnformatted("참고: ImGui 입력창이 키보드를 잡고 있으면 일부 단축키는 동작하지 않을 수 있습니다.");

	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);
}

void FEditorMainPanel::RenderConsoleDrawer()
{
	constexpr float FooterHeight = 32.0f;
	constexpr float DrawerMaxHeight = 320.0f;
	if (ConsoleDrawerAnim <= 0.001f)
	{
		return;
	}

	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const ImVec2 OverlayPos = MainViewport ? MainViewport->WorkPos : ImVec2(0.0f, 0.0f);
	const ImVec2 OverlaySize = MainViewport ? MainViewport->WorkSize : ImGui::GetIO().DisplaySize;
	const float DrawerHeight = DrawerMaxHeight * ConsoleDrawerAnim;
	if (DrawerHeight <= 1.0f)
	{
		return;
	}

	ImGui::SetNextWindowPos(
		ImVec2(OverlayPos.x, OverlayPos.y + OverlaySize.y - FooterHeight - DrawerHeight),
		ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(OverlaySize.x, DrawerHeight), ImGuiCond_Always);
	if (MainViewport)
	{
		ImGui::SetNextWindowViewport(MainViewport->ID);
	}

	const ImGuiWindowFlags DrawerFlags =
		ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoNav
		| ImGuiWindowFlags_NoFocusOnAppearing;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.06f, 0.97f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.20f, 0.23f, 1.0f));
	if (bBringConsoleDrawerToFrontNextFrame)
	{
		ImGui::SetNextWindowFocus();
		bBringConsoleDrawerToFrontNextFrame = false;
	}
	if (ImGui::Begin("##EditorConsoleDrawer", nullptr, DrawerFlags))
	{
		ConsoleWidget.RenderDrawerToolbar();
		ImGui::Separator();
		ConsoleWidget.RenderLogContents(0.0f);
	}
	ImGui::End();
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(3);
}

void FEditorMainPanel::RenderFooterOverlay(float DeltaTime)
{
	(void)DeltaTime;

	if (!EditorEngine)
	{
		return;
	}

	const TArray<FString> ActiveLogs = EditorEngine->GetActiveFooterLogMessages();

	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();
	const ImVec2 OverlayPos = MainViewport ? MainViewport->WorkPos : ImVec2(0.0f, 0.0f);
	const ImVec2 OverlaySize = MainViewport ? MainViewport->WorkSize : ImGui::GetIO().DisplaySize;
	constexpr float FooterHeight = 32.0f;

	ImGui::SetNextWindowPos(
		ImVec2(OverlayPos.x, OverlayPos.y + OverlaySize.y - FooterHeight),
		ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(OverlaySize.x, FooterHeight), ImGuiCond_Always);
	if (MainViewport)
	{
		ImGui::SetNextWindowViewport(MainViewport->ID);
	}

	const ImGuiWindowFlags FooterFlags =
		ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoDocking
		| ImGuiWindowFlags_NoSavedSettings
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoNav
		| ImGuiWindowFlags_NoFocusOnAppearing;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 4.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.09f, 0.98f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.83f, 0.83f, 0.86f, 1.0f));
	if (ImGui::Begin("##EditorStatusBar", nullptr, FooterFlags))
	{
		if (ImGui::IsKeyPressed(ImGuiKey_GraveAccent, false))
		{
			switch (ConsoleBacktickCycleState)
			{
			case 0:
				ConsoleBacktickCycleState = 1;
				bConsoleDrawerVisible = false;
				bFocusConsoleInputNextFrame = true;
				break;
			case 1:
				ConsoleBacktickCycleState = 2;
				bConsoleDrawerVisible = true;
				bBringConsoleDrawerToFrontNextFrame = true;
				bFocusConsoleInputNextFrame = true;
				break;
			default:
				ConsoleBacktickCycleState = 0;
				bConsoleDrawerVisible = false;
				bFocusConsoleInputNextFrame = false;
				bFocusConsoleButtonNextFrame = true;
				break;
			}
		}

		float CenteredTextY = (FooterHeight - ImGui::GetFrameHeight()) * 0.5f;
		if (CenteredTextY < 0.0f)
		{
			CenteredTextY = 0.0f;
		}
		ImGui::SetCursorPosY(CenteredTextY);

		if (EditorEngine->HasCurrentLevelFilePath())
		{
			ImGui::Text("Level: %s", EditorEngine->GetCurrentLevelFilePath().c_str());
		}
		else
		{
			ImGui::TextUnformatted("Level: Unsaved");
		}

		const bool bDrawerOpen = ConsoleDrawerAnim > 0.5f;
		ImGui::SameLine(0.0f, 16.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.45f, 0.48f, 1.0f));
		ImGui::TextUnformatted("|");
		ImGui::PopStyleColor();
		ImGui::SameLine(0.0f, 14.0f);
		if (bFocusConsoleButtonNextFrame)
		{
			ImGui::SetKeyboardFocusHere();
			bFocusConsoleButtonNextFrame = false;
		}
		if (ImGui::Button(bDrawerOpen ? "Console v" : "Console ^"))
		{
			bConsoleDrawerVisible = !bConsoleDrawerVisible;
			if (bConsoleDrawerVisible)
			{
				ConsoleBacktickCycleState = 2;
				bBringConsoleDrawerToFrontNextFrame = true;
				bFocusConsoleInputNextFrame = true;
			}
			else
			{
				ConsoleBacktickCycleState = 0;
				bFocusConsoleButtonNextFrame = true;
			}
		}

		ImGui::SameLine();
		const float InputWidth = OverlaySize.x * (bDrawerOpen ? 0.35f : 0.175f);
		ConsoleWidget.RenderInputLine("##FooterConsoleInput", InputWidth, bFocusConsoleInputNextFrame);
		if (bFocusConsoleInputNextFrame)
		{
			ConsoleBacktickCycleState = bConsoleDrawerVisible ? 2 : 1;
		}
		bFocusConsoleInputNextFrame = false;

		if (!ActiveLogs.empty())
		{
			const FString& LatestLog = ActiveLogs.back();
			const float LogWidth = ImGui::CalcTextSize(LatestLog.c_str()).x;
			const float RightAlignedX = OverlaySize.x - ImGui::GetStyle().WindowPadding.x - LogWidth;
			float TargetX = ImGui::GetCursorPosX() + 8.0f;
			if (RightAlignedX > TargetX)
			{
				TargetX = RightAlignedX;
			}
			ImGui::SameLine(TargetX);
			ImGui::TextUnformatted(LatestLog.c_str());
		}
	}
	ImGui::End();
	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(3);
}

void FEditorMainPanel::Update()
{
	ImGuiIO& IO = ImGui::GetIO();

	// 뷰포트 슬롯 위에서는 ImGui 입력 캡처를 해제해야
	// 뷰포트 입력(TickInput/TickInteraction)이 정상 동작한다.
	bool bWantMouse = IO.WantCaptureMouse;
	bool bWantKeyboard = IO.WantCaptureKeyboard;
	if (bShowShortcutOverlay)
	{
		bWantMouse = true;
		bWantKeyboard = true;
	}
	else if (EditorEngine && EditorEngine->IsMouseOverViewport())
	{
		bWantMouse = false;
		bWantKeyboard = false;
	}
	bWantCaptureMouse = bWantMouse;
	bWantCaptureKeyboard = bWantKeyboard;

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

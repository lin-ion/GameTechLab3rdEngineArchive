#include "pch.h"
#include "ImGuiDrawer.h"


UImGuiDrawer::UImGuiDrawer()
{
    CreateAppConsole();
}

void UImGuiDrawer::Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init((void*)hwnd);
	ImGui_ImplDX11_Init(device, context);
}

void UImGuiDrawer::BeginFrame()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void UImGuiDrawer::EndFrame()
{
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void UImGuiDrawer::UpdateUI()
{
    ImGui::Begin("Jungle Control Panel");
    ImGui::Text("Hello Jungle World");


    static char sceneName[128] = "Default";
    ImGui::InputText("Scene Name", sceneName, IM_COUNTOF(sceneName));

    if (ImGui::Button("New Scene")) { /*Scene 초기화 로직 */ }
    if (ImGui::Button("Save Scene")) { /* sceneName으로 저장 로직 */ }
    if (ImGui::Button("Load Scene")) { /* sceneName 불러오기 로직 */ }

	ImGui::End();

	if(bShowConsole)
		DrawAppConsole("Jungle Tech Lab", &bShowConsole);
}

void UImGuiDrawer::Release()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

    DestroyAppConsole();
}
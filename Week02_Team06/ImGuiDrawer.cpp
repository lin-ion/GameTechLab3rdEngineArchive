#include "pch.h"
#include "ImGuiDrawer.h"
#include "Scene.h"
#include "CameraComponent.h"

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

void UImGuiDrawer::UpdateUI(UScene* Scene)
{
	ImGui::Begin("Jungle Control Panel");
	ImGui::Text("Hello Jungle World");


	static char sceneName[128] = "Default";
	ImGui::InputText("Scene Name", sceneName, IM_COUNTOF(sceneName));

	if (ImGui::Button("New Scene")) { /*Scene 초기화 로직 */ }
	if (ImGui::Button("Save Scene")) { /* sceneName으로 저장 로직 */ }
	if (ImGui::Button("Load Scene")) { /* sceneName 불러오기 로직 */ }

	ImGui::End();

	ImGui::Begin("Camera");
	{
		// TODO: TStaticArray
		std::array<float, 3> Position = {
			Scene->MainCamera->GetPosition().X,
			Scene->MainCamera->GetPosition().Y,
			Scene->MainCamera->GetPosition().Z
		};
		ImGui::DragFloat3("Camera Location", Position.data(), 0.1f, -10.f, 10.0f);
		Scene->MainCamera->SetPosition({ Position[0], Position[1], Position[2] });

		std::array<float, 3> Rotation = {
			Scene->MainCamera->GetRotation().X,
			Scene->MainCamera->GetRotation().Y,
			Scene->MainCamera->GetRotation().Z
		};
		ImGui::DragFloat3("Camera Rotation", Rotation.data(), 0.5f);
		Scene->MainCamera->SetPosition({ Position[0], Position[1], Position[2] });
		Scene->MainCamera->SetRotation({ Rotation[0], Rotation[1], Rotation[2] });

	}
	ImGui::End();

	if (bShowConsole)
		DrawAppConsole("Jungle Tech Lab", &bShowConsole);
}

void UImGuiDrawer::Release()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	DestroyAppConsole();
}
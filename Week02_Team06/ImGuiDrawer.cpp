#include "pch.h"
#include "ImGuiDrawer.h"
#include "SceneSerializer.h"
#include "PrimitiveComponent.h"


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
    ImGui::Text("Hello Jungle World!");
	float fps = ImGui::GetIO().Framerate;
	float ms = (fps > 0.0f) ? (1000.0f / fps) : 0.0f;
	ImGui::Text("FPS %.0f (%.0fms)", fps, ms);
	ImGui::Separator();

	DrawSpawnPanel();
	ImGui::Separator();
	DrawSceneControlPanel();
	ImGui::Separator();
	DrawCameraPanel();

	ImGui::End();

	//DrawPrimitiveDataPanel();

	if(bShowConsole)
		DrawAppConsole("Console Window", &bShowConsole);
}

void UImGuiDrawer::Release()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

    DestroyAppConsole();
}

void UImGuiDrawer::DrawSpawnPanel()
{
	static int primitiveType = 0;
	const char* items[] = { "Cube", "Sphere", "Triangle" };
	ImGui::Combo("Primitive", &primitiveType, items, IM_ARRAYSIZE(items));

	if (ImGui::Button("Spawn"))
	{
		// 오브젝트 스폰하는 함수
	}
	ImGui::SameLine();
	static int count = 0;
	ImGui::InputInt("Count", &count); 
}

void UImGuiDrawer::DrawSceneControlPanel()
{
	static char sceneName[128] = "Default";
	ImGui::InputText("Scene Name", sceneName, IM_COUNTOF(sceneName));

	if (ImGui::Button("New Scene")) { /*Scene 초기화 로직 */ }
	if (ImGui::Button("Save Scene")) { /* sceneName으로 저장 로직 */ }
	if (ImGui::Button("Load Scene")) { /* sceneName 불러오기 로직 */ }
}

void UImGuiDrawer::DrawCameraPanel()
{
	static bool isOrthogonal = false;
	ImGui::Checkbox("Orthogonal", &isOrthogonal);

	static float fov = 90.0f;
	ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f);

	static float pos[3] = { 0, 0, 0 };
	ImGui::DragFloat3("Camera Location", pos, 0.1f);

	static float rot[3] = { 0, 0, 0 };
	ImGui::DragFloat3("Camera Rotaion", rot, 0.1f);
}

void UImGuiDrawer::DrawPrimitiveDataPanel(UPrimitiveComponent* SelectedTarget)
{
	ImGui::Begin("Jungle Property Window");

	FVector currentPos = SelectedTarget->GetPosition();
	static float pos[3] = { currentPos.X, currentPos.Y, currentPos.Z};
	
	if (ImGui::DragFloat3("Translation", pos, 0.1f))
	{
		SelectedTarget->SetPosition(FVector(pos[0], pos[1], pos[2]));
	}

	FVector currentRot = SelectedTarget->GetRotation();
	static float rot[3] = { currentRot.X, currentRot.Y, currentRot.Z };

	if (ImGui::DragFloat3("Rotation", rot, 0.1f))
	{
		SelectedTarget->SetRotation(FVector(rot[0], rot[1], rot[2]));
	}

	FVector currentScl = SelectedTarget->GetScale();
	static float scl[3] = { currentScl.X, currentScl.Y, currentScl.Z };

	if (ImGui::DragFloat3("Scale", scl, 0.1f))
	{
		SelectedTarget->SetRotation(FVector(scl[0], scl[1], scl[2]));
	}

	ImGui::End();
}

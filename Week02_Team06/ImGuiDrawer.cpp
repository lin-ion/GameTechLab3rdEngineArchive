#include "pch.h"
#include "ImGuiDrawer.h"
#include "SceneSerializer.h"
#include "PrimitiveComponent.h"
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
    ImGui::Text("Hello Jungle World!");
	float fps = ImGui::GetIO().Framerate;
	float ms = (fps > 0.0f) ? (1000.0f / fps) : 0.0f;
	ImGui::Text("FPS %.0f (%.0fms)", fps, ms);
	ImGui::Separator();

	DrawSpawnPanel();
	ImGui::Separator();
	DrawSceneControlPanel();
	ImGui::Separator();
	DrawCameraPanel(Scene);

	ImGui::End();

	//DrawPrimitiveDataPanel();

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

void UImGuiDrawer::DrawCameraPanel(UScene* Scene)
{
	bool bIsOrthogonal = Scene->MainCamera->IsOrthogonal();
	ImGui::Checkbox("Orthogonal", &bIsOrthogonal);
	Scene->MainCamera->SetOrthogonal(bIsOrthogonal);

	float FOV = Scene->MainCamera->GetFOV();
	ImGui::SliderFloat("FOV", &FOV, 10.0f, 120.0f);
	Scene->MainCamera->SetFOV(FOV);

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

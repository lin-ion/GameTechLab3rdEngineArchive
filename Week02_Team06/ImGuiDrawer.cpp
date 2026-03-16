#include "pch.h"
#include "ImGuiDrawer.h"
#include "SceneSerializer.h"
#include "PrimitiveComponent.h"
#include "CameraComponent.h"
#include "FEditorViewportClient.h"
#include "World.h"
#include "StaticMeshActor.h"

UImGuiDrawer::UImGuiDrawer()
{
	CreateAppConsole();
}

void UImGuiDrawer::Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context, UWorld* world)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init((void*)hwnd);
	ImGui_ImplDX11_Init(device, context);
	World = world;
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

void UImGuiDrawer::UpdateUI(FEditorViewportClient* ViewportClient)
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
	DrawCameraPanel(ViewportClient);

	ImGui::End();

	DrawPrimitiveDataPanel();

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

	actorParameters.PrimitiveType = items[primitiveType];
	actorParameters.Location = ActorLocation;
	actorParameters.Rotation = ActorRotation;
	actorParameters.Scale = ActorScale;

	if (ImGui::Button("Spawn"))
	{
		World->SpawnActorFromEditor(actorParameters);
	}
	ImGui::SameLine();
	static int count = 0;
	ImGui::InputInt("Count", &count);
	actorParameters.Count = count;
}

void UImGuiDrawer::DrawSceneControlPanel()
{
	static char sceneName[128] = "Default";
	ImGui::InputText("Scene Name", sceneName, IM_COUNTOF(sceneName));

	if (ImGui::Button("New Scene")) { /*Scene 초기화 로직 */ }
	if (ImGui::Button("Save Scene")) { /* sceneName으로 저장 로직 */ }
	if (ImGui::Button("Load Scene")) { /* sceneName 불러오기 로직 */ }
}

void UImGuiDrawer::DrawCameraPanel(FEditorViewportClient* ViewportClient)
{
	float FOV = ViewportClient->GetFOV();
	ImGui::SliderFloat("FOV", &FOV, 10.0f, 120.0f);
	ViewportClient->SetFOV(FOV);

	std::array<float, 3> Position = {
			ViewportClient->GetViewLocation().X,
			ViewportClient->GetViewLocation().Y,
			ViewportClient->GetViewLocation().Z
	};
	ImGui::DragFloat3("Camera Location", Position.data(), 0.1f, -10.f, 10.0f);
	ViewportClient->SetViewLocation({ Position[0], Position[1], Position[2] });

	std::array<float, 3> Rotation = {
		ViewportClient->GetViewRotation().X,
		ViewportClient->GetViewRotation().Y,
		ViewportClient->GetViewRotation().Z
	};
	ImGui::DragFloat3("Camera Rotation", Rotation.data(), 0.5f);
	ViewportClient->SetViewRotation({ Rotation[0], Rotation[1], Rotation[2] });
}

void UImGuiDrawer::DrawPrimitiveDataPanel()
{
	ImGui::Begin("Jungle Property Window");

	if (SelectedTarget)
	{
		ActorLocation = SelectedTarget->GetPosition();
		ActorRotation = SelectedTarget->GetRotation();
		ActorScale = SelectedTarget->GetScale();
	}

	
	if (ImGui::DragFloat3("Translation", &ActorLocation.X, 0.1f))
	{
		if (SelectedTarget) SelectedTarget->SetPosition(ActorLocation);
	}

	
	if (ImGui::DragFloat3("Rotation", &ActorRotation.X, 0.1f))
	{
		if (SelectedTarget) SelectedTarget->SetRotation(ActorRotation);
	}

	
	if (ImGui::DragFloat3("Scale", &ActorScale.X, 0.1f))
	{
		if (SelectedTarget) SelectedTarget->SetScale(ActorScale);
	}

	ImGui::End();
}

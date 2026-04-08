#include "Editor/UI/EditorControlWidget.h"
#include "Editor/EditorEngine.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Engine/Profiling/Timer.h"
#include "ImGui/imgui.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "GameFramework/StaticMeshActor.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

void FEditorControlWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorControlWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(500.0f, 480.0f), ImGuiCond_Once);

	ImGui::Begin("Jungle Control Panel");

	// Spawn
	if (ImGui::Button("Place Actors"))
	{
		ImGui::OpenPopup("PlaceActorsPopup");
	}
	if (ImGui::BeginPopup("PlaceActorsPopup"))
	{
		UWorld* World = EditorEngine->GetWorld();
		const auto& Placeables = EditorEngine->GetPlaceableActors();

		for (const auto& Desc : Placeables)
		{
			if (ImGui::Selectable(Desc.Name.c_str()))
			{
				AActor* SpawnedActor = Desc.SpawnFunc(World);
				if (SpawnedActor)
				{
					SpawnedActor->SetActorLocation(CurSpawnPoint);
					EditorEngine->SetupVisualization(SpawnedActor);
				}
			}
		}
		ImGui::EndPopup();
	}

	SEPARATOR();

	// Camera
	FViewportCamera* Camera = EditorEngine->GetCamera();
	FLevelEditorViewportClient* ActiveVC = EditorEngine->GetActiveViewport();
	if (!Camera)
	{
		ImGui::End();
		return;
	}

	FVector CamPos = Camera->GetWorldLocation();
	float CameraLocation[3] = { CamPos.X, CamPos.Y, CamPos.Z };
	if (ImGui::DragFloat3("Camera Location", CameraLocation, 0.1f))
	{
		Camera->SetWorldLocation(FVector(CameraLocation[0], CameraLocation[1], CameraLocation[2]));
		if (ActiveVC && ActiveVC->GetInputController())
		{
			ActiveVC->GetInputController()->SyncNavigationFromCamera();
		}
	}

	FRotator CamRot = Camera->GetRelativeRotation();
	float CameraRotation[3] = { CamRot.Roll, CamRot.Pitch, CamRot.Yaw };
	if (ImGui::DragFloat3("Camera Rotation", CameraRotation, 0.1f))
	{
		Camera->SetRelativeRotation(FRotator(CameraRotation[1], CameraRotation[2], Clamp(CameraRotation[0], CamRot.Roll, CamRot.Roll)));
		if (ActiveVC && ActiveVC->GetInputController())
		{
			ActiveVC->GetInputController()->SyncNavigationFromCamera();
		}
	}

	ImGui::End();
}

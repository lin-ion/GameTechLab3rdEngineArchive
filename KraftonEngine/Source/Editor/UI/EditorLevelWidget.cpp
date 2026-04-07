#include "Editor/UI/EditorLevelWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/LevelEditorViewportClient.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Engine/Core/Common.h"
#include "GameFramework/WorldContext.h"

#include "ImGui/imgui.h"
#include "Component/GizmoComponent.h"
#include "Serialization/LevelSaveManager.h"

#include <filesystem>

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

void FEditorLevelWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
	RefreshLevelFileList();
}

void FEditorLevelWidget::RefreshLevelFileList()
{
	LevelFiles = FLevelSaveManager::GetLevelFileList();
	if (SelectedLevelIndex >= static_cast<int32>(LevelFiles.size()))
	{
		SelectedLevelIndex = LevelFiles.empty() ? -1 : 0;
	}
}

void FEditorLevelWidget::Render(float DeltaTime)
{
	using namespace common::constants::ImGui;

	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(400.0f, 350.0f), ImGuiCond_Once);

	ImGui::Begin("Jungle Level Manager");

	// New Level
	if (ImGui::Button("New Level"))
	{
		EditorEngine->NewLevel();
		NewLevelNotificationTimer = NotificationTimer;
	}
	if (NewLevelNotificationTimer > 0.0f)
	{
		NewLevelNotificationTimer -= DeltaTime;
		ImGui::SameLine();
		ImGui::Text("New Level created");
	}

	SEPARATOR();

	// Save Level
	ImGui::InputText("Level Name", LevelName, IM_ARRAYSIZE(LevelName));

	if (ImGui::Button("Save Level"))
	{
		FWorldContext* Ctx = EditorEngine->GetWorldContextFromHandle(EditorEngine->GetActiveWorldHandle());
		if (Ctx)
		{
			FPerspectiveCameraData PerspectiveCamData;
			const FPerspectiveCameraData* PerspectiveCam = nullptr;
			for (FLevelEditorViewportClient* VC : EditorEngine->GetLevelViewportClients())
			{
				if (VC->GetRenderOptions().ViewportType == ELevelViewportType::Perspective || VC->GetRenderOptions().ViewportType == ELevelViewportType::FreeOrthographic)
				{
					if (FViewportCamera* Cam = VC->GetCamera())
					{
						PerspectiveCamData.Location = Cam->GetWorldLocation();
						PerspectiveCamData.Rotation = Cam->GetWorldMatrix().GetEuler();
						PerspectiveCamData.FOV = Cam->GetFOV();
						PerspectiveCamData.NearClip = Cam->GetNearPlane();
						PerspectiveCamData.FarClip = Cam->GetFarPlane();
						PerspectiveCamData.bValid = true;
						PerspectiveCam = &PerspectiveCamData;
					}
					break;
				}
			}
			FLevelSaveManager::SaveLevelAsJSON(LevelName, *Ctx, PerspectiveCam);
		}
		LevelSaveNotificationTimer = NotificationTimer;
		RefreshLevelFileList();
	}
	if (LevelSaveNotificationTimer > 0.0f)
	{
		LevelSaveNotificationTimer -= DeltaTime;
		ImGui::SameLine();
		ImGui::Text("Level saved");
	}

	SEPARATOR();

	// Load Level (combo box)
	if (ImGui::Button("Refresh"))
	{
		RefreshLevelFileList();
	}
	ImGui::SameLine();
	ImGui::Text("Level Files (%d)", static_cast<int32>(LevelFiles.size()));

	if (!LevelFiles.empty())
	{
		// TODO: Case Sensitivity 삭제
		// LevelFiles에는 "A.Scene" / "A.scene" 형태의 파일명이 들어있으므로
		// Combo 표시용으로 확장자를 제거한 stem 목록을 별도 구성
		TArray<FString> SceneStems;
		SceneStems.reserve(LevelFiles.size());
		for (const auto& F : LevelFiles)
			SceneStems.push_back(FPaths::ToUtf8(std::filesystem::path(FPaths::ToWide(F)).stem().wstring()));

		auto SceneNameGetter = [](void* Data, int32 Idx) -> const char*
			{
				auto* Stems = static_cast<TArray<FString>*>(Data);
				if (Idx < 0 || Idx >= static_cast<int32>(Stems->size())) return nullptr;
				return (*Stems)[Idx].c_str();
			};

		ImGui::Combo("Level File", &SelectedLevelIndex, SceneNameGetter, &SceneStems, static_cast<int32>(SceneStems.size()));

		if (ImGui::Button("Load Level") && SelectedLevelIndex >= 0)
		{
			// 원본 확장자(.Scene / .scene)를 그대로 사용
			std::filesystem::path ScenePath = std::filesystem::path(FLevelSaveManager::GetSceneDirectory())
				/ FPaths::ToWide(LevelFiles[SelectedLevelIndex]);
			FString FilePath = FPaths::ToUtf8(ScenePath.wstring());

			EditorEngine->ClearWorlds();
			FWorldContext LoadCtx;
			FPerspectiveCameraData CamData;
			FLevelSaveManager::LoadLevelFromJSON(FilePath, LoadCtx, CamData);
			if (LoadCtx.World)
			{
				EditorEngine->GetWorldList().push_back(LoadCtx);
				EditorEngine->SetActiveWorld(LoadCtx.ContextHandle);
			}
			EditorEngine->ResetViewport();

			// ResetViewport()가 카메라를 기본값으로 초기화하므로 그 이후에 복원
			if (CamData.bValid)
			{
				for (FLevelEditorViewportClient* VC : EditorEngine->GetLevelViewportClients())
				{
					if (VC->GetRenderOptions().ViewportType == ELevelViewportType::Perspective || VC->GetRenderOptions().ViewportType == ELevelViewportType::FreeOrthographic)
					{
						if (FViewportCamera* Cam = VC->GetCamera())
						{
							Cam->SetWorldLocation(CamData.Location);
							Cam->SetRelativeRotation(CamData.Rotation);
							FViewportCameraState CS = Cam->GetCameraState();
							CS.FOV   = CamData.FOV;
							CS.NearZ = CamData.NearClip;
							CS.FarZ  = CamData.FarClip;
							Cam->SetCameraState(CS);
							VC->InvalidateIdBufferCache();
						}
						break;
					}
				}
			}

			LevelLoadNotificationTimer = NotificationTimer;
		}
		if (LevelLoadNotificationTimer > 0.0f)
		{
			LevelLoadNotificationTimer -= DeltaTime;
			ImGui::SameLine();
			ImGui::Text("Level loaded");
		}
	}
	else
	{
		ImGui::Text("No level files found in %s", FPaths::ToUtf8(FLevelSaveManager::GetSceneDirectory()).c_str());
	}

	SEPARATOR();

	// Actor Outliner
	UWorld* World = EditorEngine->GetWorld();
	if (World)
	{
		const TArray<AActor*>& Actors = World->GetActors();
		ImGui::Text("Actors (%d)", static_cast<int32>(Actors.size()));
		ImGui::Separator();

		// Fill remaining space with scrollable child region
		FSelectionManager& Selection = EditorEngine->GetSelectionManager();

		ImGui::BeginChild("ActorList", ImVec2(0, 0), ImGuiChildFlags_Borders);
		for (AActor* Actor : Actors)
		{
			if (!Actor) continue;

			FString ActorName = Actor->GetFName();
			if (ActorName.empty())
			{
				ActorName = Actor->GetTypeInfo()->name;
			}

			bool bIsSelected = Selection.IsSelected(Actor);
			if (ImGui::Selectable(ActorName.c_str(), bIsSelected))
			{
				if (ImGui::GetIO().KeyShift)
				{
					Selection.SelectRange(Actor, Actors);
				}
				else if (ImGui::GetIO().KeyCtrl)
				{
					Selection.ToggleSelect(Actor);
				}
				else
				{
					Selection.Select(Actor);
				}
			}
		}
		ImGui::EndChild();
	}
	ImGui::End();
}

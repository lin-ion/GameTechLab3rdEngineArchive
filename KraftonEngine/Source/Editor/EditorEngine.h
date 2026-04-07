#pragma once

#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/FLevelViewportLayout.h"
#include "Editor/Selection/PickingTypes.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"

class UGizmoComponent;
class FLevelEditorViewportClient;
class FEditorViewportClient;
class FOverlayStatSystem;
class FViewportCamera;

class UEditorEngine : public UEngine
{
public:
	DECLARE_CLASS(UEditorEngine, UEngine)

	UEditorEngine() = default;
	~UEditorEngine() override = default;

	// Lifecycle overrides
	void Init(FWindowsWindow* InWindow) override;
	void Shutdown() override;
	void Tick(float DeltaTime) override;
	void OnWindowResized(uint32 Width, uint32 Height) override;

	// Editor-specific API
	UGizmoComponent* GetGizmo() const { return SelectionManager.GetGizmo(); }
	FViewportCamera* GetCamera() const;

	void ClearWorlds();
	void ResetViewport();
	void CloseLevel();
	void NewLevel();

	FEditorSettings& GetSettings() { return FEditorSettings::Get(); }
	const FEditorSettings& GetSettings() const { return FEditorSettings::Get(); }

	FSelectionManager& GetSelectionManager() { return SelectionManager; }

	void RenderUI(float DeltaTime);

	FOverlayStatSystem& GetOverlayStatSystem() { return OverlayStatSystem; }
	const FOverlayStatSystem& GetOverlayStatSystem() const { return OverlayStatSystem; }

	void SetPickingMode(EPickingMode InMode);
	EPickingMode GetPickingMode() const { return PickingMode; }

private:
	FSelectionManager SelectionManager;
	FEditorMainPanel MainPanel;
	FOverlayStatSystem OverlayStatSystem;
	EPickingMode PickingMode = EPickingMode::RayTriangleBVH;
};

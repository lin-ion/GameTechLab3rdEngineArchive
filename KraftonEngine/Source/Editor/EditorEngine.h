#pragma once

#include "Engine/Runtime/Engine.h"

#include "Editor/Viewport/FLevelViewportLayout.h"
#include "Editor/Selection/PickingTypes.h"
#include "Editor/Subsystem/OverlayStatSystem.h"
#include "Editor/UI/EditorMainPanel.h"
#include "Editor/UI/EditorFooterLogSystem.h"
#include "Editor/Settings/EditorSettings.h"
#include "Editor/Selection/SelectionManager.h"
#include "Viewport/ViewportClient.h"

class FTransformGizmo;
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
	FTransformGizmo* GetGizmo() const { return SelectionManager.GetGizmo(); }
	FViewportCamera* GetCamera() const;

	void ClearWorlds();
	void ResetViewport();
	void CloseLevel();
	void NewLevel();
	bool LoadLevelFromPath(const FString& InLevelFilePath);
	bool LoadLevelWithDialog();
	bool SaveLevel();
	bool SaveLevelAsWithDialog();
	bool SaveLevelAsName(const FString& InLevelName);
	bool OpenAssetFolder();
	const FString& GetCurrentLevelFilePath() const { return CurrentLevelFilePath; }
	bool HasCurrentLevelFilePath() const { return !CurrentLevelFilePath.empty(); }
	TArray<FString> GetActiveFooterLogMessages() const;

	FEditorSettings& GetSettings() { return FEditorSettings::Get(); }
	const FEditorSettings& GetSettings() const { return FEditorSettings::Get(); }

	FSelectionManager& GetSelectionManager() { return SelectionManager; }

	void RenderUI(float DeltaTime);

	FOverlayStatSystem& GetOverlayStatSystem() { return OverlayStatSystem; }
	const FOverlayStatSystem& GetOverlayStatSystem() const { return OverlayStatSystem; }

	void SetPickingMode(EPickingMode InMode);
	EPickingMode GetPickingMode() const { return PickingMode; }

	// PIE preparation: swap viewport sub-client through host client.
	bool SetViewportSubClient(FViewport* InViewport, FViewportClient* InSubClient);
	bool ResetViewportSubClient(FViewport* InViewport);
	bool SetViewportSubClientForWorldType(FViewport* InViewport, EWorldType InWorldType);
	FViewportClient* GetViewportSubClient(FViewport* InViewport) const;
	bool SetActiveViewportSubClient(FViewportClient* InSubClient);
	bool ResetActiveViewportSubClient();
	bool SetActiveViewportSubClientForWorldType(EWorldType InWorldType);
	FViewportClient* GetActiveViewportSubClient() const;

private:
	FViewportClient* ResolveInputTargetClient(FViewport* InViewport, FViewportClient* InClient) const override;
	void PruneInputTargetHosts();
	FLevelEditorViewportClient* FindLevelViewportClientByViewport(FViewport* InViewport) const;

	FSelectionManager SelectionManager;
	FEditorMainPanel MainPanel;
	FOverlayStatSystem OverlayStatSystem;
	FEditorFooterLogSystem FooterLogSystem;
	EPickingMode PickingMode = EPickingMode::IDBuffer;
	mutable TMap<FViewport*, FViewportHostClient> InputTargetHosts;
	FString CurrentLevelFilePath;
};

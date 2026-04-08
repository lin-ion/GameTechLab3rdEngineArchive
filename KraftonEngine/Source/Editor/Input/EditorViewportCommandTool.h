#pragma once

#include "Editor/Input/EditorViewportTools.h"

class FEditorViewportClient;
class FEditorViewportController;

class FEditorViewportCommandTool final : public IEditorViewportTool
{
public:
	FEditorViewportCommandTool(FEditorViewportClient* InOwner, FEditorViewportController* InController);
	bool HandleInput(float DeltaTime) override;

private:
	bool FocusPrimarySelection();
	bool DeleteSelectedActors();
	bool SelectAllActors();
	bool ToggleGizmoCoordinateSpace();
	bool NewLevel();
	bool LoadLevel();
	bool SaveLevel();
	bool SaveLevelAs();

private:
	FEditorViewportClient* Owner = nullptr;
	FEditorViewportController* Controller = nullptr;
};

#pragma once

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/UI/EditorControlWidget.h"
#include "Editor/UI/EditorPropertyWidget.h"
#include "Editor/UI/EditorLevelWidget.h"
#include "Editor/UI/EditorStatWidget.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;

class FEditorMainPanel
{
public:
	void Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UEditorEngine* InEditorEngine);
	void Release();
	void Render(float DeltaTime);
	void Update();
	bool IsCapturingMouse() const { return bWantCaptureMouse; }
	bool IsCapturingKeyboard() const { return bWantCaptureKeyboard; }

private:
	FWindowsWindow* Window = nullptr;
	UEditorEngine* EditorEngine = nullptr;
	FEditorConsoleWidget ConsoleWidget;
	FEditorControlWidget ControlWidget;
	FEditorPropertyWidget PropertyWidget;
	FEditorLevelWidget SceneWidget;
	FEditorStatWidget StatWidget;
	bool bWantCaptureMouse = false;
	bool bWantCaptureKeyboard = false;
};

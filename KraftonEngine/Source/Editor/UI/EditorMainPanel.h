#pragma once

#include "Editor/UI/EditorConsoleWidget.h"
#include "Editor/UI/EditorControlWidget.h"
#include "Editor/UI/EditorPropertyWidget.h"
#include "Editor/UI/EditorLevelWidget.h"
#include "Editor/UI/EditorStatWidget.h"

class FRenderer;
class UEditorEngine;
class FWindowsWindow;
struct ImFont;

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
	void RenderMainMenuBar();
	void RenderDockSpace();
	void RenderShortcutOverlay();
	void RenderFooterOverlay();

private:
	FWindowsWindow* Window = nullptr;
	UEditorEngine* EditorEngine = nullptr;
	FEditorConsoleWidget ConsoleWidget;
	FEditorControlWidget ControlWidget;
	FEditorPropertyWidget PropertyWidget;
	FEditorLevelWidget LevelWidget;
	FEditorStatWidget StatWidget;
	bool bShowConsolePanel = true;
	bool bShowControlPanel = true;
	bool bShowLevelPanel = true;
	bool bShowPropertyPanel = true;
	bool bShowStatPanel = false;
	bool bShowShortcutOverlay = false;
	bool bWantCaptureMouse = false;
	bool bWantCaptureKeyboard = false;
	ImFont* FooterFont = nullptr;
	ImFont* FooterBoldFont = nullptr;
};

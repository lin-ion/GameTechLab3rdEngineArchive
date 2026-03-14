#pragma once

class UPrimitiveComponent;

class UImGuiDrawer
{
public:
	bool bShowConsole = true;

	UImGuiDrawer();
	~UImGuiDrawer() {};

	void Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);

	void BeginFrame();
	void EndFrame();

	void UpdateUI();

	void Release();

private:
	void DrawSpawnPanel();
	void DrawSceneControlPanel();
	void DrawPrimitiveDataPanel(UPrimitiveComponent* SelectedTarget);
	void DrawCameraPanel();
};
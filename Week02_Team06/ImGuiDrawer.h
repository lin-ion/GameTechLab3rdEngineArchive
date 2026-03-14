#pragma once

class UScene;

class UImGuiDrawer
{
public:
	bool bShowConsole = true;

	UImGuiDrawer();
	~UImGuiDrawer() {};

	void Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);

	void BeginFrame();
	void EndFrame();

	void UpdateUI(UScene* Scene);

	void Release();
};
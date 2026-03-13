#pragma once

struct AppConsole;

class UImGuiDrawer
{
public:
	UImGuiDrawer();
	~UImGuiDrawer() {};

	void Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);

	void BeginFrame();
	void EndFrame();

	void UpdateUI();

	void Release();

private:
	AppConsole* con;
};
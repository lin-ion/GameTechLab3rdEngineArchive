#pragma once
class UGraphics
{
public:
	UGraphics() = default;
	~UGraphics() = default;

public:
	ID3D11Device* GetDevice() { return Device; };
	ID3D11DeviceContext* GetDeviceContext() { return DeviceContext; };
	IDXGISwapChain* GetSwapChain() { return SwapChain; };

public:
	void Initialize(HWND hWnd);
	void ClearRenderTarget();

	void Release();

private:
	void CreateDeviceAnsSwapChain(HWND hWnd);
	void ReleaseDeviceAndSwapChain();

	void CreateBackBuffer();
	void ReleaseBackBuffer();

private:
	ID3D11Device* Device;
	ID3D11DeviceContext* DeviceContext;
	IDXGISwapChain* SwapChain;

	ID3D11Texture2D* BackBuffer;
	ID3D11RenderTargetView* BackBufferRTV;


	D3D11_VIEWPORT		 ViewportInfo;
};
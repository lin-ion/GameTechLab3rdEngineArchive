#pragma once
class UScene;


class URenderer
{
public:
	URenderer(ID3D11Device* _Device, ID3D11DeviceContext* _DeviceContext, IDXGISwapChain* _SwapChain);
	~URenderer() = default;
public:
	void Initialize();

	void BeginScene();

	void Render(UScene* Scene);
	
	void EndScene();

	void Release();

private:
	void CreateRasterizerState();
	void ReleaseRasterizerState();

	void CreateRenderTargetView();
	void ReleaseRenderTargetView();

	void CreateDepthStensilView();
	void ReleaseDepthStensilView();

	void CreateShader(ID3D11Device& Device, const std::wstring& Filename, const D3D11_INPUT_ELEMENT_DESC Layout[], int ElemnetNum);
	void ReleaseShader();

	void CreateConstantBuffer();
	void ReleaseConstantBuffer();


private:
	ID3D11Device* Device = { nullptr };
	ID3D11DeviceContext* DeviceContext = { nullptr };
	IDXGISwapChain* SwapChain = { nullptr };

	ID3D11Texture2D* BackBuffer = { nullptr };
	ID3D11RenderTargetView* BackBufferRTV = { nullptr };

	ID3D11Texture2D* DepthBuffer = { nullptr };
	ID3D11DepthStencilView* DepthStensilView = { nullptr };

	ID3D11RasterizerState* RasterizerState = { nullptr };


	ID3D11Buffer* ConstantBuffer = { nullptr };

	D3D11_VIEWPORT ViewportInfo = {};


	//쉐이더
	ID3D11InputLayout*  SimpleInputLayout  = nullptr;
	ID3D11VertexShader* SimpleVertexShader = nullptr;
	ID3D11PixelShader*  SimplePixelShader  = nullptr;

};
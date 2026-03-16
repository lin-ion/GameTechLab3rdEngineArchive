#pragma once
class UWorld;
class FEditorViewportClient;

class URenderer
{
public:
	URenderer(ID3D11Device* _Device, ID3D11DeviceContext* _DeviceContext, IDXGISwapChain* _SwapChain, const FEditorViewportClient& _ViewportClient);
	~URenderer() = default;
public:
	void Initialize();

	void BeginScene();

	void Render(UWorld* World);
	
	void EndScene();

	void Release();

public:
	void UpdateConstantBuffer(ID3D11DeviceContext& Context, const FMatrix& MVP, const FVector4& Color = FVector4());

private:
	void CreateRasterizerState();
	void ReleaseRasterizerState();

	void CreateRenderTargetView();
	void ReleaseRenderTargetView();

	void CreateDepthStensilView();
	void ReleaseDepthStensilView();
	void CreateDepthStencilState();
	ID3D11DepthStencilState* DepthStencilState = { nullptr };

	void CreateShader(ID3D11Device& Device, const std::wstring& Filename, const D3D11_INPUT_ELEMENT_DESC Layout[], int ElemnetNum);
	void ReleaseShader();

	void CreateConstantBuffer();
	void ReleaseConstantBuffer();

	void CreateLineAxisBuffer();
	void ReleaseLineAxisBuffer();

	void CreateGridBuffer();
	void ReleaseGridBuffer();

private:
	void RenderAxisLine();
	void RenderPrimitive(UWorld* World);


private:
	ID3D11Device* Device = { nullptr };
	ID3D11DeviceContext* DeviceContext = { nullptr };
	IDXGISwapChain* SwapChain = { nullptr };

	ID3D11Texture2D* BackBuffer = { nullptr };
	ID3D11RenderTargetView* BackBufferRTV = { nullptr };

	ID3D11Texture2D* DepthBuffer = { nullptr };
	ID3D11DepthStencilView* DepthStensilView = { nullptr };

	ID3D11RasterizerState* RasterizerState     = { nullptr };

	ID3D11Buffer* ConstantBuffer = { nullptr };

	D3D11_VIEWPORT ViewportInfo = {};


	//쉐이더
	ID3D11InputLayout*  SimpleInputLayout  = nullptr;
	ID3D11VertexShader* SimpleVertexShader = nullptr;
	ID3D11PixelShader*  SimplePixelShader  = nullptr;

	//라인
	ID3D11Buffer* LineAxisBuffer = { nullptr };

	//그리드
	ID3D11Buffer* GridBuffer = { nullptr };
	int GridVertexCount = 0;

	const FEditorViewportClient& ViewportClient;
};

// TODO: 두 개의 상수버퍼를 만들어서 Model과 View,Projection을 따로 보내는 방법 고려
struct FConstantData
{
	//FMatrix Model;
	//FMatrix View;
	//FMatrix Projection;
	FMatrix MVP;
	FVector4 Color;
};

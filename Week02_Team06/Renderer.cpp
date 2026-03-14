#include "pch.h"
#include "Renderer.h"
#include "Scene.h"
#include "Object.h"
#include "PrimitiveComponent.h"
#include "CameraComponent.h"

URenderer::URenderer(ID3D11Device* _Device, ID3D11DeviceContext* _DeviceContext, IDXGISwapChain* _SwapChain)
	: Device(_Device), DeviceContext(_DeviceContext), SwapChain(_SwapChain)
{
}

void URenderer::Initialize()
{
	DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
	SwapChain->GetDesc(&SwapChainDesc);
	ViewportInfo = { 0.0f, 0.0f, (float)SwapChainDesc.BufferDesc.Width, (float)SwapChainDesc.BufferDesc.Height, 0.0f, 1.0f };

	CreateRenderTargetView();
	CreateDepthStensilView();
	CreateRasterizerState();

	CreateShader(*Device, TEXT("ShaderW0.hlsl"), FVertexSimple::Elements, FVertexSimple::ElementNum);
	CreateConstantBuffer();
}

void URenderer::BeginScene()
{
	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	DeviceContext->ClearRenderTargetView(BackBufferRTV, ClearColor);
	DeviceContext->ClearDepthStencilView(DepthStensilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

	DeviceContext->RSSetViewports(1, &ViewportInfo);
	DeviceContext->RSSetState(RasterizerState);
	DeviceContext->OMSetRenderTargets(1, &BackBufferRTV, DepthStensilView);

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

void URenderer::Render(UScene* Scene)
{
	if (!Scene || !Scene->MainCamera) return;


	// 1. 공통 환경 세팅 (셰이더, 래스터라이저 등)
	DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
	DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
	DeviceContext->IASetInputLayout(SimpleInputLayout);

	// 2. 공통 행렬(View * Projection) 계산
	FMatrix ViewMatrix = Scene->MainCamera->GetViewMatrix();
	FMatrix ProjectionMatrix = Scene->MainCamera->GetProjectionMatrix();
	FMatrix ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;

	// 3. 루프 최적화: 렌더러는 명령만 내립니다.
	for (size_t i = 0; i < GUObjectArray.Size(); ++i)
	{
		UPrimitiveComponent* PrimComp = dynamic_cast<UPrimitiveComponent*>(GUObjectArray[i]);
		if (!PrimComp) continue;

		// [중요] 이제 컴포넌트에게 공통 행렬과 버퍼를 넘겨주며 직접 그리라고 명령합니다.
		PrimComp->Render(DeviceContext, ViewProjectionMatrix, ConstantBuffer);
	}
}

void URenderer::EndScene()
{
	SwapChain->Present(1, 0);
}

void URenderer::Release()
{
	ReleaseConstantBuffer();
	ReleaseShader();

	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	ReleaseRasterizerState();
	ReleaseDepthStensilView();
	ReleaseRenderTargetView();
}

void URenderer::CreateRasterizerState()
{
	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_BACK;

	Device->CreateRasterizerState(&RasterizerDesc, &RasterizerState);

}

void URenderer::ReleaseRasterizerState()
{
	if (RasterizerState)
	{
		RasterizerState->Release();
		RasterizerState = nullptr;
	}
}


void URenderer::CreateRenderTargetView()
{
	SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackBuffer);

	// 렌더 타겟 뷰 생성
	D3D11_RENDER_TARGET_VIEW_DESC BackBufferRTVdesc = {};
	BackBufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	BackBufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	assert(BackBuffer);
	Device->CreateRenderTargetView(BackBuffer, &BackBufferRTVdesc, &BackBufferRTV);
}

void URenderer::ReleaseRenderTargetView()
{
	if (BackBufferRTV)
	{
		BackBufferRTV->Release();
		BackBufferRTV = nullptr;
	}

	if (BackBuffer)
	{
		BackBuffer->Release();
		BackBuffer = nullptr;
	}

}

void URenderer::CreateDepthStensilView()
{
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = static_cast<UINT>(ViewportInfo.Width);
	desc.Height = static_cast<UINT>(ViewportInfo.Height);
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	Device->CreateTexture2D(&desc, nullptr, &DepthBuffer);

	D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc = {};
	viewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	viewDesc.Texture2D.MipSlice = 0;

	assert(DepthBuffer);
	Device->CreateDepthStencilView(DepthBuffer, &viewDesc, &DepthStensilView);
}

void URenderer::ReleaseDepthStensilView()
{
	if (DepthStensilView)
	{
		DepthStensilView->Release();
		DepthStensilView = nullptr;
	}

	if (DepthBuffer)
	{
		DepthBuffer->Release();
		DepthBuffer = nullptr;
	}
}

void URenderer::CreateShader(ID3D11Device& Device, const std::wstring& Filename, const D3D11_INPUT_ELEMENT_DESC Layout[], int ElementNum)
{
	ID3DBlob* VertexShaderCSO = nullptr;
	ID3DBlob* PixelShaderCSO = nullptr;

	// Vertex Shader 컴파일
	HRESULT hr = D3DCompileFromFile(Filename.c_str(), nullptr, nullptr, "VS_MAIN", "vs_5_0", 0, 0, &VertexShaderCSO, nullptr);
	assert(hr != E_FAIL && "vs compile err");

	hr = Device.CreateVertexShader(VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), nullptr, &SimpleVertexShader);
	assert(hr != E_FAIL && "vs creation failed");

	// 레이아웃 생성
	hr = Device.CreateInputLayout(Layout, ElementNum, VertexShaderCSO->GetBufferPointer(), VertexShaderCSO->GetBufferSize(), &SimpleInputLayout);
	VertexShaderCSO->Release();
	assert(hr != E_FAIL && "inputLayout creation failed");

	// Pixel Shader 컴파일
	hr = D3DCompileFromFile(Filename.c_str(), nullptr, nullptr, "PS_MAIN", "ps_5_0", 0, 0, &PixelShaderCSO, nullptr);
	assert(hr != E_FAIL && "ps compile err");

	hr = Device.CreatePixelShader(PixelShaderCSO->GetBufferPointer(), PixelShaderCSO->GetBufferSize(), nullptr, &SimplePixelShader);
	PixelShaderCSO->Release();
	assert(hr != E_FAIL && "ps creation failed");
}


void URenderer::ReleaseShader()
{
	if (SimplePixelShader)
	{
		SimplePixelShader->Release();
		SimplePixelShader = nullptr;
	}

	if (SimpleInputLayout)
	{
		SimpleInputLayout->Release();
		SimpleInputLayout = nullptr;
	}

	if (SimpleVertexShader)
	{
		SimpleVertexShader->Release();
		SimpleVertexShader = nullptr;
	}
}

void URenderer::CreateConstantBuffer()
{
	D3D11_BUFFER_DESC constantbufferdesc = {};
	constantbufferdesc.ByteWidth = (sizeof(FConstantData) + 0xf) & 0xfffffff0; // 16바이트 정렬
	constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC; // 프레임마다 CPU가 갱신해주어야 하므로
	constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // Usage는 사용 패턴만 정의. CPU가 접근할 수 있도록 플래그도 설정해주어야 함
	constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

	Device->CreateBuffer(&constantbufferdesc, nullptr, &ConstantBuffer);
}

void URenderer::ReleaseConstantBuffer()
{
	if (ConstantBuffer)
	{
		ConstantBuffer->Release();
		ConstantBuffer = nullptr;
	}
}

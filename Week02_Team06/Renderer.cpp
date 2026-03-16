#include "pch.h"
#include "Renderer.h"
#include "World.h"
#include "Object.h"
#include "PrimitiveComponent.h"
#include "FEditorViewportClient.h"
#include "Actor.h"

URenderer::URenderer(ID3D11Device* _Device, ID3D11DeviceContext* _DeviceContext, IDXGISwapChain* _SwapChain, const FEditorViewportClient& _ViewportClient)
	: Device(_Device), DeviceContext(_DeviceContext), SwapChain(_SwapChain), ViewportClient(_ViewportClient)
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
	CreateDepthStencilState();

	CreateShader(*Device, TEXT("ShaderW0.hlsl"), FVertexSimple::Elements, FVertexSimple::ElementNum);
	CreateConstantBuffer();

	CreateLineAxisBuffer();
	CreateGridBuffer();

}

void URenderer::BeginScene()
{
	FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
	DeviceContext->ClearRenderTargetView(BackBufferRTV, ClearColor);
	DeviceContext->ClearDepthStencilView(DepthStensilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

	DeviceContext->RSSetViewports(1, &ViewportInfo);
	DeviceContext->RSSetState(RasterizerState);
	DeviceContext->OMSetRenderTargets(1, &BackBufferRTV, DepthStensilView);
	DeviceContext->OMSetDepthStencilState(DepthStencilState, 1);

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

void URenderer::Render(UWorld* World)
{
	if (!World) return;

	// 셰이더
	DeviceContext->VSSetShader(SimpleVertexShader, nullptr, 0);
	DeviceContext->PSSetShader(SimplePixelShader, nullptr, 0);
	DeviceContext->IASetInputLayout(SimpleInputLayout);

	RenderAxisLine();
	RenderPrimitive(World);
}

void URenderer::EndScene()
{
	SwapChain->Present(1, 0);
}

void URenderer::Release()
{
	ReleaseGridBuffer();
	ReleaseLineAxisBuffer();
	ReleaseConstantBuffer();
	ReleaseShader();

	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	if (DepthStencilState)
	{
		DepthStencilState->Release();
		DepthStencilState = nullptr;
	}
	ReleaseRasterizerState();
	ReleaseDepthStensilView();
	ReleaseRenderTargetView();
}

void URenderer::UpdateConstantBuffer(ID3D11DeviceContext& Context, const FMatrix& MVP, const FVector4& Color)
{
	if (!ConstantBuffer) return;

	FConstantData Data;
	Data.MVP = MVP;
	Data.Color = Color;

	D3D11_MAPPED_SUBRESOURCE MSR;
	Context.Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MSR);
	memcpy(MSR.pData, &Data, sizeof(FConstantData));
	Context.Unmap(ConstantBuffer, 0);

	Context.VSSetConstantBuffers(0, 1, &ConstantBuffer);
	Context.PSSetConstantBuffers(0, 1, &ConstantBuffer);
}

void URenderer::CreateRasterizerState()
{
	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_NONE;

	Device->CreateRasterizerState(&RasterizerDesc, &RasterizerState);


	RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_FRONT;  
	RasterizerDesc.FrontCounterClockwise = FALSE;
	RasterizerDesc.DepthClipEnable = TRUE;


	Device->CreateRasterizerState(&RasterizerDesc, &RasterizerStateOutline);


	DeviceContext->RSSetState(RasterizerState);

}

void URenderer::ReleaseRasterizerState()
{

	if (RasterizerState)
	{
		RasterizerState->Release();
		RasterizerState = nullptr;
	}

	if (RasterizerStateOutline)
	{
		RasterizerStateOutline->Release();
		RasterizerStateOutline = nullptr;
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

void URenderer::CreateDepthStencilState()
{
	D3D11_DEPTH_STENCIL_DESC dsDesc = {};

	dsDesc.DepthEnable = TRUE;
	dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dsDesc.DepthFunc = D3D11_COMPARISON_LESS;
	dsDesc.StencilEnable = FALSE;

	// 헤더에서 바꾼 변수명(DepthStencilState)에 저장합니다.
	Device->CreateDepthStencilState(&dsDesc, &DepthStencilState);
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

void URenderer::CreateLineAxisBuffer()
{
	FVertexSimple Axis_Vertices[6] =
	{
		{0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f}, {50.f, 0.f,  0.f,  1.f, 0.f, 0.f, 1.f}, 
		{0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 1.f}, {0.f,  50.f, 0.f,  0.f, 1.f, 0.f, 1.f}, 
		{0.f, 0.f, 0.f, 0.f, 0.f, 1.f, 1.f}, {0.f,  0.f,  50.f, 0.f, 0.f, 1.f, 1.f}, 
	};

	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.ByteWidth = sizeof(FVertexSimple) * 6;
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexBufferSRD = { Axis_Vertices };

	Device->CreateBuffer(&vertexBufferDesc, &vertexBufferSRD, &LineAxisBuffer);
}

void URenderer::ReleaseLineAxisBuffer()
{
	LineAxisBuffer->Release();
}

void URenderer::CreateGridBuffer()
{
	const float GridSize = 200.f;
	const float GridStep = 5.f;
	const int   HalfCount = (int)(GridSize / GridStep); 
	const int   LineCount = HalfCount * 2 + 1;          

	GridVertexCount = LineCount * 2 * 2;

	FVertexSimple GridVertices[324];
	int idx = 0;

	const float r = 0.2f, g = 0.2f, b = 0.2f, a = 1.f;

	//그리드를 살짝 눈속임용으로 아래로 
	for (int i = -HalfCount; i <= HalfCount; i++)
	{
		float z = i * GridStep;
		GridVertices[idx++] = { -GridSize, -0.005f,  z, r, g, b, a };
		GridVertices[idx++] = {  GridSize, -0.005f,  z, r, g, b, a };
	}

	for (int i = -HalfCount; i <= HalfCount; i++)
	{
		float x = i * GridStep;
		GridVertices[idx++] = { x, -0.005f, -GridSize, r, g, b, a };
		GridVertices[idx++] = { x, -0.005f,  GridSize, r, g, b, a };
	}

	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.ByteWidth  = sizeof(FVertexSimple) * GridVertexCount;
	vertexBufferDesc.Usage      = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.BindFlags  = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexBufferSRD = { GridVertices };
	Device->CreateBuffer(&vertexBufferDesc, &vertexBufferSRD, &GridBuffer);
}

void URenderer::ReleaseGridBuffer()
{
	if (GridBuffer)
	{
		GridBuffer->Release();
		GridBuffer = nullptr;
	}
}

void URenderer::UpdateConstantBuffer(const FConstantData& Data)
{
	D3D11_MAPPED_SUBRESOURCE MSR;
	DeviceContext->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MSR);
	memcpy(MSR.pData, &Data, sizeof(FConstantData));
	DeviceContext->Unmap(ConstantBuffer, 0);
	DeviceContext->VSSetConstantBuffers(0, 1, &ConstantBuffer);
}

void URenderer::RenderAxisLine()
{
	if (!ConstantBuffer) return;

	FMatrix VP = ViewportClient.GetViewMatrix() * ViewportClient.GetProjectionMatrix();
	UpdateConstantBuffer({ VP, FVector4() });

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	UINT Stride = sizeof(FVertexSimple);
	UINT Offset = 0;

	// 그리드 렌더링
	if (GridBuffer)
	{
		DeviceContext->IASetVertexBuffers(0, 1, &GridBuffer, &Stride, &Offset);
		DeviceContext->Draw(GridVertexCount, 0);
	}

	// 축 라인 렌더링
	DeviceContext->IASetVertexBuffers(0, 1, &LineAxisBuffer, &Stride, &Offset);
	DeviceContext->Draw(6, 0);

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void URenderer::RenderPrimitive(UWorld* World)
{
	FMatrix VP = ViewportClient.GetViewMatrix() * ViewportClient.GetProjectionMatrix();
	TArray<AActor*> Actors = World->CurrentLevel->Actors;

	for (size_t i = 0; i < Actors.Size(); ++i)
	{
		UPrimitiveComponent* Primitive = Actors[i]->GetComponentByClass<UPrimitiveComponent>();
		if (!Primitive) continue;

		FMatrix Model = Primitive->GetComponentTransform();

		// 아웃라인 패스
		FMatrix OutlineModel = FMatrix::MakeScale(FVector(1.05f, 1.05f, 1.05f)) * Model;
		UpdateConstantBuffer({ OutlineModel * VP, FVector4(FVector(1.f, 0.22f, 0.f), 1.f) });
		DeviceContext->RSSetState(RasterizerStateOutline);
		Primitive->Render(*DeviceContext);

		// 원본 패스
		UpdateConstantBuffer({ Model * VP, FVector4() });
		DeviceContext->RSSetState(RasterizerState);
		Primitive->Render(*DeviceContext);
	}
}

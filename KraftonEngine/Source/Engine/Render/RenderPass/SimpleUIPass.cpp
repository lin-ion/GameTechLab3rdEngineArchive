#include "SimpleUIPass.h"

#include "RenderPassRegistry.h"
#include "Render/Types/FrameContext.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Command/DrawCommandList.h"
#include "Core/Types/CoreTypes.h"

#include "UI/Canvas/UICanvasManager.h"
#include "UI/Canvas/UICanvas.h"
#include "UI/Canvas/UIElement.h"
#include "Object/Object.h"

#include <d3d11.h>

REGISTER_RENDER_PASS(FSimpleUIPass)

namespace
{
	// RmlUi.hlsl 과 동일한 정점/상수버퍼 레이아웃(입력 레이아웃 재사용).
	struct FSimpleUIVertex
	{
		float X, Y;
		float R, G, B, A;
		float U, V;
	};

	struct FSimpleUICB
	{
		float ViewportWidth = 1.0f;
		float ViewportHeight = 1.0f;
		float TranslationX = 0.0f;
		float TranslationY = 0.0f;
		float Transform[16] = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f,
		};
	};

	constexpr const char* SimpleUIShaderPath = "Shaders/UI/SimpleUI.hlsl";

	// 가시 노드의 ScreenRect 를 쿼드(4정점 / 6인덱스)로 누적. top-down 트리 순회.
	void CollectVisible(UUIElement* Element, TArray<FSimpleUIVertex>& Verts, TArray<uint32>& Indices)
	{
		if (!Element)
		{
			return;
		}

		if (Element->IsVisibleRect())
		{
			const FUIRect& R = Element->GetScreenRect();
			const FVector4 C = Element->GetColor();
			const uint32 Base = static_cast<uint32>(Verts.size());

			const float X0 = R.Pos.X;
			const float Y0 = R.Pos.Y;
			const float X1 = R.Pos.X + R.Size.X;
			const float Y1 = R.Pos.Y + R.Size.Y;

			Verts.push_back({ X0, Y0, C.R, C.G, C.B, C.A, 0.0f, 0.0f });
			Verts.push_back({ X1, Y0, C.R, C.G, C.B, C.A, 1.0f, 0.0f });
			Verts.push_back({ X1, Y1, C.R, C.G, C.B, C.A, 1.0f, 1.0f });
			Verts.push_back({ X0, Y1, C.R, C.G, C.B, C.A, 0.0f, 1.0f });

			Indices.push_back(Base + 0);
			Indices.push_back(Base + 1);
			Indices.push_back(Base + 2);
			Indices.push_back(Base + 0);
			Indices.push_back(Base + 2);
			Indices.push_back(Base + 3);
		}

		for (USceneComponent* Child : Element->GetChildren())
		{
			if (UUIElement* ChildElement = Cast<UUIElement>(Child))
			{
				CollectVisible(ChildElement, Verts, Indices);
			}
		}
	}

	ID3D11Buffer* CreateBuffer(ID3D11Device* Device, UINT BindFlags, const void* Data, UINT ByteWidth)
	{
		if (!Device || ByteWidth == 0)
		{
			return nullptr;
		}
		D3D11_BUFFER_DESC Desc = {};
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.ByteWidth = ByteWidth;
		Desc.BindFlags = BindFlags;

		D3D11_SUBRESOURCE_DATA Init = {};
		Init.pSysMem = Data;

		ID3D11Buffer* Buffer = nullptr;
		if (FAILED(Device->CreateBuffer(&Desc, Data ? &Init : nullptr, &Buffer)))
		{
			return nullptr;
		}
		return Buffer;
	}
}

FSimpleUIPass::FSimpleUIPass()
{
	PassType = ERenderPass::SimpleUI;
	RenderState = { EDepthStencilState::NoDepth, EBlendState::AlphaBlend,
	                ERasterizerState::SolidNoCull, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, false };
}

bool FSimpleUIPass::BeginPass(const FPassContext& Ctx)
{
	// 그릴 Canvas 가 하나도 없거나 뷰포트 RTV 가 없으면 패스 스킵.
	return Ctx.Frame.ViewportRTV && !FUICanvasManager::Get().GetCanvases().empty();
}

void FSimpleUIPass::Execute(const FPassContext& Ctx)
{
	ID3D11Device* Device = Ctx.Device.GetDevice();
	ID3D11DeviceContext* DC = Ctx.Device.GetDeviceContext();
	if (!Device || !DC || Ctx.Frame.ViewportWidth <= 0.0f || Ctx.Frame.ViewportHeight <= 0.0f)
	{
		return;
	}

	// 1) 레이아웃 패스가 캐시한 ScreenRect 들을 쿼드로 모은다(여기서 레이아웃은 하지 않음).
	TArray<FSimpleUIVertex> Verts;
	TArray<uint32> Indices;
	for (UUICanvas* Canvas : FUICanvasManager::Get().GetCanvases())
	{
		CollectVisible(Canvas, Verts, Indices);
	}
	if (Indices.empty())
	{
		return;
	}

	// 2) 셰이더 — 단색 쿼드(텍스처/샘플러 불필요).
	FShader* Shader = FShaderManager::Get().GetOrCreate(SimpleUIShaderPath);
	if (!Shader || !Shader->IsValid())
	{
		return;
	}

	// 3) 프레임 단위 임시 버퍼 생성(소량 쿼드 — MVP 단순화). 그린 뒤 해제.
	ID3D11Buffer* VB = CreateBuffer(Device, D3D11_BIND_VERTEX_BUFFER,
		Verts.data(), static_cast<UINT>(sizeof(FSimpleUIVertex) * Verts.size()));
	ID3D11Buffer* IB = CreateBuffer(Device, D3D11_BIND_INDEX_BUFFER,
		Indices.data(), static_cast<UINT>(sizeof(uint32) * Indices.size()));
	ID3D11Buffer* CB = CreateBuffer(Device, D3D11_BIND_CONSTANT_BUFFER, nullptr, sizeof(FSimpleUICB));
	if (!VB || !IB || !CB)
	{
		if (VB) VB->Release();
		if (IB) IB->Release();
		if (CB) CB->Release();
		return;
	}

	// 4) 상태 — RmlUi 와 동일(NoDepth / AlphaBlend / SolidNoCull), 뷰포트 RTV 에 합성.
	Ctx.Resources.SetDepthStencilState(Ctx.Device, EDepthStencilState::NoDepth);
	Ctx.Resources.SetBlendState(Ctx.Device, EBlendState::AlphaBlend);
	Ctx.Resources.SetRasterizerState(Ctx.Device, ERasterizerState::SolidNoCull);

	D3D11_VIEWPORT Viewport = {};
	Viewport.TopLeftX = 0.0f;
	Viewport.TopLeftY = 0.0f;
	Viewport.Width = Ctx.Frame.ViewportWidth;
	Viewport.Height = Ctx.Frame.ViewportHeight;
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;
	DC->RSSetViewports(1, &Viewport);

	DC->OMSetRenderTargets(1, &Ctx.Cache.RTV, Ctx.Cache.DSV);
	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Shader->Bind(DC);

	FSimpleUICB CBData;
	CBData.ViewportWidth = Ctx.Frame.ViewportWidth;
	CBData.ViewportHeight = Ctx.Frame.ViewportHeight;
	DC->UpdateSubresource(CB, 0, nullptr, &CBData, 0, 0);
	DC->VSSetConstantBuffers(0, 1, &CB);

	UINT Stride = sizeof(FSimpleUIVertex);
	UINT Offset = 0;
	DC->IASetVertexBuffers(0, 1, &VB, &Stride, &Offset);
	DC->IASetIndexBuffer(IB, DXGI_FORMAT_R32_UINT, 0);
	DC->DrawIndexed(static_cast<UINT>(Indices.size()), 0, 0);

	VB->Release();
	IB->Release();
	CB->Release();
}

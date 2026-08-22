#include "Renderer.h"

#include <iostream>
#include <algorithm>
#include "Core/Paths.h"
#include "Core/ResourceManager.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Mesh/MeshManager.h"
#include "Core/Logging/Stats.h"
#include "Core/Logging/GPUProfiler.h"
#include "Editor/Settings/EditorSettings.h"

void FRenderer::Create(HWND hWindow)
{
	Device.Create(hWindow);

	if (Device.GetDevice() == nullptr)
	{
		std::cout << "Failed to create D3D Device." << std::endl;
	}

	// 1. 일반 메쉬 (Primitive.hlsl)
	Resources.PrimitiveShader.Create(Device.GetDevice(), L"Shaders/Primitive.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	Resources.BillboardShader.Create(Device.GetDevice(), L"Shaders/ShaderBillboard.hlsl",
		"VS", "PS", NormalVertexInputLayout, ARRAYSIZE(NormalVertexInputLayout));

	// 2. 기즈모 (Gizmo.hlsl)
	Resources.GizmoShader.Create(Device.GetDevice(), L"Shaders/Gizmo.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	// 3. 에디터/라인 (Editor.hlsl)
	Resources.EditorShader.Create(Device.GetDevice(), L"Shaders/Editor.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	// 4. 선택 마스크 (SelectionMask.hlsl)
	Resources.SelectionMaskShader.Create(Device.GetDevice(), L"Shaders/SelectionMask.hlsl",
		"VS", "PS", PrimitiveInputLayout, ARRAYSIZE(PrimitiveInputLayout));

	// 5. 포스트 프로세스 아웃라인 (OutlinePostProcess.hlsl)
	Resources.OutlineShader.Create(Device.GetDevice(), L"Shaders/OutlinePostProcess.hlsl",
		"VS", "PS", nullptr, 0);

	// 6. 스태틱 메시 (ShaderStaticMesh.hlsl)
	Resources.StaticMeshShader.Create(Device.GetDevice(), L"Shaders/ShaderStaticMesh.hlsl",
		"mainVS", "mainPS", NormalVertexInputLayout, ARRAYSIZE(NormalVertexInputLayout));

	// 7. 안티 앨리어싱 (Fast Approximate Anti-Aliasing, FXAA)
    Resources.FxaaShader.Create(Device.GetDevice(), L"Shaders/FXAA.hlsl", "VS", "PS", nullptr, 0);

	// 8. DepthSceneMode
	Resources.DepthSceneShader.Create(Device.GetDevice(), L"Shaders/DepthScene.hlsl", "VS", "PS", nullptr, 0);

	// 9. 데칼 (ShaderDecal.hlsl)
	Resources.DecalShader.Create(Device.GetDevice(), L"Shaders/ShaderDecal.hlsl",
		"VS", "PS", NormalVertexInputLayout, ARRAYSIZE(NormalVertexInputLayout));
	//10. FireBall fireballshader.hlsl
	Resources.FireBallShader.Create(Device.GetDevice(), L"Shaders/FireBallShader.hlsl", "VS_Main", "PS_Main", nullptr, 0);

	// 11. Fog Shader
	Resources.HeightFogShader.Create(Device.GetDevice(), L"Shaders/HeightFogShader.hlsl", "VS_Main", "PS_Main", nullptr, 0);

	Resources.PerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FPerObjectConstants));
	Resources.FrameBuffer.Create(Device.GetDevice(), sizeof(FFrameConstants));
	Resources.GizmoPerObjectConstantBuffer.Create(Device.GetDevice(), sizeof(FGizmoConstants));
	Resources.EditorConstantBuffer.Create(Device.GetDevice(), sizeof(FEditorConstants));
	Resources.OutlineConstantBuffer.Create(Device.GetDevice(), sizeof(FOutlineConstants));
	Resources.StaticMeshConstantBuffer.Create(Device.GetDevice(), sizeof(FStaticMeshConstants));
	Resources.FxaaConstantBuffer.Create(Device.GetDevice(), sizeof(FFxaaConstantBuffer));
    Resources.DepthSceneConstantBuffer.Create(Device.GetDevice(), sizeof(FDepthSceneConstants));
	Resources.DecalConstantBuffer.Create(Device.GetDevice(), sizeof(FDecalConstants));
	Resources.FireBallConstantBuffer.Create(Device.GetDevice(), sizeof(FFireBallCBuffer));
	Resources.ViewportInfoConstantBuffer.Create(Device.GetDevice(), sizeof(FViewportInfoConstants));
	Resources.HeightFogConstantBuffer.Create(Device.GetDevice(), sizeof(FHeightFogCBuffer));


	// TODO : SamplerState 관리
	{
		// MeshSamplerState
		D3D11_SAMPLER_DESC SampDesc = {};
		SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		Device.GetDevice()->CreateSamplerState(&SampDesc, Resources.MeshSamplerState.ReleaseAndGetAddressOf());
	}
	{
		// DecalSamplerState
		D3D11_SAMPLER_DESC SampDesc = {};
		SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		Device.GetDevice()->CreateSamplerState(&SampDesc, Resources.DecalSamplerState.ReleaseAndGetAddressOf());
	}
	{
		// FXAA Linear Sampler State
		D3D11_SAMPLER_DESC FxaaSampDesc = {};
		FxaaSampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
		FxaaSampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		FxaaSampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		FxaaSampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		Device.GetDevice()->CreateSamplerState(&FxaaSampDesc, Resources.LinearSamplerState.ReleaseAndGetAddressOf());
	}
	// Point Sampler (depth 기반 PostProcess 공용)
	{
		D3D11_SAMPLER_DESC SampDesc = {};
		SampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		SampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		SampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		SampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		Device.GetDevice()->CreateSamplerState(&SampDesc, Resources.PointSamplerState.ReleaseAndGetAddressOf());
	}
	//	MeshManager init
	FMeshManager::Initialize();

	EditorLineBatcher.Create(Device.GetDevice());
	GridLineBatcher.Create(Device.GetDevice());

	// 텍스처는 ResourceManager가 소유 — Batcher 는 셰이더/버퍼만 초기화
	FontBatcher.Create(Device.GetDevice());
	SubUVBatcher.Create(Device.GetDevice());

	InitializePassRenderStates();
	InitializePassBatchers();
	InitializePostProcesses();

	// GPU Profiler 초기화
	FGPUProfiler::Get().Initialize(Device.GetDevice(), Device.GetDeviceContext());
}

void FRenderer::Release()
{
	// Release Shader
	Resources.PrimitiveShader.Release();
	Resources.BillboardShader.Release();
	Resources.GizmoShader.Release();
	Resources.EditorShader.Release();
	Resources.SelectionMaskShader.Release();
	Resources.OutlineShader.Release();
	Resources.StaticMeshShader.Release();
	Resources.FxaaShader.Release();
	Resources.DepthSceneShader.Release();
	Resources.DecalShader.Release();
	Resources.FireBallShader.Release();
	Resources.HeightFogShader.Release();

	// Release Constant Buffer
	Resources.PerObjectConstantBuffer.Release();
	Resources.FrameBuffer.Release();
	Resources.GizmoPerObjectConstantBuffer.Release();
	Resources.EditorConstantBuffer.Release();
	Resources.OutlineConstantBuffer.Release();
	Resources.StaticMeshConstantBuffer.Release();
	Resources.FxaaConstantBuffer.Release();
    Resources.DepthSceneConstantBuffer.Release();
	Resources.DecalConstantBuffer.Release();
	Resources.FireBallConstantBuffer.Release();
	Resources.ViewportInfoConstantBuffer.Release();
	Resources.HeightFogConstantBuffer.Release();

	// Reset Sampler State
	Resources.MeshSamplerState.Reset();
    Resources.LinearSamplerState.Reset();
	Resources.DecalSamplerState.Reset();
	Resources.PointSamplerState.Reset();

	FGPUProfiler::Get().Shutdown();

	EditorLineBatcher.Release();
	GridLineBatcher.Release();
	FontBatcher.Release();
	SubUVBatcher.Release();

	// Clear Post Process
	PostProcesses.clear();

	Device.Release();
}

void FRenderer::InitializePostProcesses()
{
	PostProcesses.clear();
	
	// TODO: 순서에 맞춰서 PostProcess Push_back 하기
	PostProcesses.push_back(std::make_unique<FDepthScenePostProcess>());
	PostProcesses.push_back(std::make_unique<UFireBallPostProcess>());
	PostProcesses.push_back(std::make_unique<UHeightFogPostProcess>());
	PostProcesses.push_back(std::make_unique<FOutlinePostProcess>());
	PostProcesses.push_back(std::make_unique<FFXAAPostProcess>());
}

//	Bus → Batcher 데이터 수집 (CPU). BeginFrame 이전에 호출.
void FRenderer::PrepareBatchers(const FRenderBus& InRenderBus)
{
	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		if (!PassBatchers[i]) continue;

		const auto& Commands = InRenderBus.GetCommands(static_cast<ERenderPass>(i));
		const auto& AlignedCommands = GetAlignedCommands(static_cast<ERenderPass>(i), Commands);

		PassBatchers[i].Clear();
		for (const auto& Cmd : AlignedCommands)
			PassBatchers[i].Collect(Cmd, InRenderBus);
	}
}

const TArray<FRenderCommand>& FRenderer::GetAlignedCommands(ERenderPass Pass, const TArray<FRenderCommand>& Commands)
{
	// SubUV 패스: Particle(SRV) 포인터 기준 정렬 → 같은 텍스쳐끼리 연속 배치
	if (Pass == ERenderPass::SubUV && Commands.size() > 1)
	{
		SortedCommandBuffer.assign(Commands.begin(), Commands.end());

		std::sort(SortedCommandBuffer.begin(), SortedCommandBuffer.end(),
			[](const FRenderCommand& A, const FRenderCommand& B) {
				return A.Constants.SubUV.Particle < B.Constants.SubUV.Particle;
			});

		return SortedCommandBuffer;
	}

	return Commands;
}

//	GPU 프레임 시작. 반드시 Render 이전에 호출되어야 함.
void FRenderer::BeginFrame()
{
	Device.BeginFrame();
	UseBackBufferRenderTargets();

#if STATS
	FGPUProfiler::Get().BeginFrame();
#endif
}

void FRenderer::UseBackBufferRenderTargets()
{
	CurrentRenderTargets = Device.GetBackBufferRenderTargets();
	if (CurrentRenderTargets.IsValid())
	{
		ID3D11RenderTargetView* RTV = CurrentRenderTargets.SceneColorRTV;
		Device.GetDeviceContext()->OMSetRenderTargets(1, &RTV, CurrentRenderTargets.DepthStencilView);
		Device.SetSubViewport(0, 0,
			static_cast<int32>(CurrentRenderTargets.Width),
			static_cast<int32>(CurrentRenderTargets.Height));
	}
}

void FRenderer::UseViewportRenderTargets()
{
	CurrentRenderTargets = Device.GetViewportRenderTargets();
	if (!CurrentRenderTargets.IsValid())
	{
		UseBackBufferRenderTargets();
		return;
	}

	Device.SetSubViewport(0, 0,
		static_cast<int32>(CurrentRenderTargets.Width),
		static_cast<int32>(CurrentRenderTargets.Height));
}

//	RenderBus에 담긴 모든 RenderCommand에 대해서 Draw Call 수행 (GPU)
void FRenderer::Render(const FRenderBus& InRenderBus, const FPostProcessViewDesc* InPostProcessViewDesc)
{
	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	UpdateFrameBuffer(Context, InRenderBus);

	for (uint32 i = 0; i < (uint32)ERenderPass::MAX; ++i)
	{
		ERenderPass CurPass = static_cast<ERenderPass>(i);

		// Post Process 특수 처리 (임시)
		if (CurPass == ERenderPass::PostProcess)
		{
			if (InPostProcessViewDesc != nullptr)
			{
				TArray<FPostProcessViewDesc> SingleView;
				SingleView.push_back(*InPostProcessViewDesc);
				ExecutePostProcessStack(SingleView);

				CurrentRenderTargets = Device.GetViewportRenderTargets();

				// TODO: 굳이 필요한가 싶지만, 
				Device.SetSubViewport(InPostProcessViewDesc->X, InPostProcessViewDesc->Y,
					InPostProcessViewDesc->Width, InPostProcessViewDesc->Height);
			}
			continue;
		}

		const auto& Commands = InRenderBus.GetCommands(CurPass);
		if (Commands.empty()) continue;

		if (PassBatchers[i])
		{
			ApplyPassRenderState(CurPass, Context, InRenderBus.GetViewMode());
			PassBatchers[i].Flush(CurPass, InRenderBus, Context);
		}
		else
		{
			ExecuteDefaultPass(CurPass, Commands, InRenderBus, Context);
		}
	}

}

void FRenderer::RenderOverlay(const FPostProcessViewDesc& ViewDesc, const FRenderBus& InOverlayBus)
{
	ID3D11DeviceContext* Context = Device.GetDeviceContext();
	if (Context == nullptr)
	{
		return;
	}

	const auto& Commands = InOverlayBus.GetCommands(ERenderPass::DepthLess);
	if (Commands.empty())
	{
		return;
	}

	UpdateFrameBuffer(Context, InOverlayBus);
	Device.SetSubViewport(ViewDesc.X, ViewDesc.Y, ViewDesc.Width, ViewDesc.Height);

	ID3D11RenderTargetView* RTV = Device.GetCurrentColorRTV();
	ID3D11DepthStencilView* DSV = CurrentRenderTargets.DepthStencilView;
	Context->OMSetRenderTargets(1, &RTV, DSV);

	const FPassRenderState& State = PassRenderStates[(uint32)ERenderPass::DepthLess];
	ERasterizerState Rasterizer = State.Rasterizer;
	if (State.bWireframeAware && InOverlayBus.GetViewMode() == EViewMode::Wireframe)
	{
		Rasterizer = ERasterizerState::WireFrame;
	}

	Device.SetDepthStencilState(State.DepthStencil);
	Device.SetBlendState(State.Blend);
	Device.SetRasterizerState(Rasterizer);
	Context->IASetPrimitiveTopology(State.Topology);

	if (State.Shader)
	{
		State.Shader->Bind(Context);
	}

	ERenderCommandType LastCommandType = static_cast<ERenderCommandType>(-1);
	for (const auto& Cmd : Commands)
	{
		EDepthStencilState TargetDepth = (Cmd.DepthStencilState != static_cast<EDepthStencilState>(-1))
			? Cmd.DepthStencilState
			: State.DepthStencil;

		EBlendState TargetBlend = (Cmd.BlendState != static_cast<EBlendState>(-1))
			? Cmd.BlendState
			: State.Blend;

		Device.SetDepthStencilState(TargetDepth);
		Device.SetBlendState(TargetBlend);

		BindShaderByType(Cmd, Context, LastCommandType);
		DrawCommand(Context, Cmd);
	}
}

// ============================================================
// 패스별 기본 렌더 상태 테이블 초기화
// ============================================================
void FRenderer::InitializePassRenderStates()
{
	using E = ERenderPass;
	auto& S = PassRenderStates;

	// Pass                              DepthStencil                      Blend                        Rasterizer                        Topology                               Shader                           WireframeAware
	S[(uint32)E::Opaque]             = { EDepthStencilState::Default,      EBlendState::Opaque,         ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.PrimitiveShader,      true  };
	S[(uint32)E::Decal]              = { EDepthStencilState::DepthReadOnly,EBlendState::AlphaBlend,     ERasterizerState::DepthBias,      D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.DecalShader,          true  };
	S[(uint32)E::Font]               = { EDepthStencilState::Default,      EBlendState::AlphaBlend,     ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, nullptr,                         true  };
	S[(uint32)E::SubUV]              = { EDepthStencilState::Default,      EBlendState::AlphaBlend,     ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, nullptr,                         true  };
	S[(uint32)E::Translucent]        = { EDepthStencilState::Default,      EBlendState::AlphaBlend,     ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.PrimitiveShader,      false };
	S[(uint32)E::SelectionMask]      = { EDepthStencilState::StencilWrite, EBlendState::Opaque,         ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.SelectionMaskShader,  false };
	S[(uint32)E::PostProcess]        = { EDepthStencilState::Default,      EBlendState::Opaque,         ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, nullptr,                         false };
	S[(uint32)E::Billboard]          = { EDepthStencilState::Default,      EBlendState::AlphaBlend,     ERasterizerState::SolidNoCull,    D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.BillboardShader,      true  };
	S[(uint32)E::Editor]             = { EDepthStencilState::Default,      EBlendState::AlphaBlend,     ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     &Resources.EditorShader,         true  };
	S[(uint32)E::DepthLess]          = { EDepthStencilState::DepthReadOnly,EBlendState::AlphaBlend,     ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, &Resources.GizmoShader,          false };
	S[(uint32)E::Grid]               = { EDepthStencilState::Default,      EBlendState::AlphaBlend,     ERasterizerState::SolidBackCull,  D3D11_PRIMITIVE_TOPOLOGY_LINELIST,     &Resources.EditorShader,         false };
}

// ============================================================
// Pass Batcher 바인딩 초기화
// ============================================================
void FRenderer::InitializePassBatchers()
{
	// --- Editor 패스: AABB 디버그 박스 → EditorLineBatcher ---
	PassBatchers[(uint32)ERenderPass::Editor] = {
		/*.Clear   =*/ [this]() { EditorLineBatcher.Clear(); },
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus&) {
			if (Cmd.Type == ERenderCommandType::DebugBox)
			{
				const FVector4 LineColor = Cmd.Constants.AABB.Color.ToVector4();
				for (int32 VertexIndex = 0; VertexIndex + 1 < Cmd.Constants.AABB.VertexCount; VertexIndex += 2)
				{
					EditorLineBatcher.AddLine(
						Cmd.Constants.AABB.Vertices[VertexIndex],
						Cmd.Constants.AABB.Vertices[VertexIndex + 1],
						LineColor);
				}
			}
		},
		/*.Flush   =*/ [this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx) {
			FlushLineBatcher(EditorLineBatcher, Pass, Bus, Ctx);
		}
	};

	// --- Grid 패스: 월드 그리드 + 축 → GridLineBatcher ---
	PassBatchers[(uint32)ERenderPass::Grid] = {
		/*.Clear   =*/ [this]() { GridLineBatcher.Clear(); },
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::Grid)
			{
				const FVector CameraPos = Bus.GetView().GetInverse().GetOrigin();
				const FVector CameraFwd = Bus.GetCameraForward();

				GridLineBatcher.AddWorldHelpers(
					Bus.GetShowFlags(),
					Cmd.Constants.Grid.GridSpacing,
					Cmd.Constants.Grid.GridHalfLineCount,
					CameraPos, CameraFwd,
					Cmd.Constants.Grid.bOrthographic);
			}
		},
		/*.Flush   =*/ [this](ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Ctx) {
			FlushLineBatcher(GridLineBatcher, Pass, Bus, Ctx);
		}
	};

	// --- Font 패스: 텍스트 → FontBatcher ---
	PassBatchers[(uint32)ERenderPass::Font] = {
		/*.Clear   =*/ [this]() { FontBatcher.Clear(); },
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::Font && Cmd.Constants.Font.Text && !Cmd.Constants.Font.Text->empty())
			{
				FontBatcher.AddText(
					*Cmd.Constants.Font.Text,
					Cmd.PerObjectConstants.Model,
					Cmd.Constants.Font.Scale
				);
			}
		},
		/*.Flush   =*/ [this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			const FFontResource* FontRes = FResourceManager::Get().FindFont(FName("Default"));
			FontBatcher.Flush(Ctx, FontRes);
		}
	};

	// --- SubUV 패스: 스프라이트 → SubUVBatcher ---
	// Collect 시 첫 번째 유효한 SRV를 캡처하여 Flush에서 재순회 방지
	PassBatchers[(uint32)ERenderPass::SubUV] = {
		/*.Clear   =*/ [this]() {
			SubUVBatcher.Clear();
			SubUVCachedSRV = nullptr;
		},
		/*.Collect =*/ [this](const FRenderCommand& Cmd, const FRenderBus& Bus) {
			if (Cmd.Type == ERenderCommandType::SubUV && Cmd.Constants.SubUV.Particle)
			{
				const auto& SubUV = Cmd.Constants.SubUV;
				if (!SubUVCachedSRV && SubUV.Particle->IsLoaded())
				{
					SubUVCachedSRV = SubUV.Particle->SRV.Get();
				}

				SubUVBatcher.AddSprite(
					SubUV.Particle->SRV.Get(),
					Cmd.PerObjectConstants.Model.GetOrigin(),
					Bus.GetCameraRight(),
					Bus.GetCameraUp(),
					Cmd.PerObjectConstants.Model.GetScaleVector(),
					SubUV.FrameIndex,
					SubUV.Particle->Columns,
					SubUV.Particle->Rows,
					SubUV.Width,
					SubUV.Height
				);
			}
		},
		/*.Flush   =*/ [this](ERenderPass, const FRenderBus&, ID3D11DeviceContext* Ctx) {
			SubUVBatcher.Flush(Ctx);
		}
	};
}

// ============================================================
// LineBatcher Flush 공통
// ============================================================
void FRenderer::FlushLineBatcher(FLineBatcher& Batcher, ERenderPass Pass, const FRenderBus& Bus, ID3D11DeviceContext* Context)
{
	if (Batcher.GetLineCount() == 0) return;

	const FVector CameraPosition = Bus.GetView().GetInverse().GetOrigin();
	FEditorConstants EditorConstants = {};
	EditorConstants.CameraPosition = CameraPosition;
	Resources.EditorConstantBuffer.Update(Context, &EditorConstants, sizeof(FEditorConstants));

	ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

	ID3D11Buffer* cb = Resources.EditorConstantBuffer.GetBuffer();
	Context->VSSetConstantBuffers(4, 1, &cb);
	Context->PSSetConstantBuffers(4, 1, &cb);

	Batcher.Flush(Context);
}

// ============================================================
// 기본 패스 실행기
// ============================================================
void FRenderer::ExecuteDefaultPass(ERenderPass Pass, const TArray<FRenderCommand>& Commands, const FRenderBus& Bus, ID3D11DeviceContext* Context)
{
	ApplyPassRenderState(Pass, Context, Bus.GetViewMode());

	const FPassRenderState& State = PassRenderStates[(uint32)Pass];
	ERenderCommandType LastCommandType = static_cast<ERenderCommandType>(-1);
	for (const auto& Cmd : Commands)
	{
		EDepthStencilState TargetDepth = (Cmd.DepthStencilState != static_cast<EDepthStencilState>(-1))
			? Cmd.DepthStencilState
			: State.DepthStencil;

		EBlendState TargetBlend = (Cmd.BlendState != static_cast<EBlendState>(-1))
			? Cmd.BlendState
			: State.Blend;

		Device.SetDepthStencilState(TargetDepth);
		Device.SetBlendState(TargetBlend);

		BindShaderByType(Cmd, Context, LastCommandType);
		DrawCommand(Context, Cmd);
	}
}

void FRenderer::ExecutePostProcessStack(const TArray<FPostProcessViewDesc>& Views) 
{
    if (Views.empty() || PostProcesses.empty())
        return;

    ID3D11DeviceContext* Context = Device.GetDeviceContext();
    constexpr UINT ViewportInfoSlot = 12;

    for (const auto& PostProcess : PostProcesses)
    {
        if (PostProcess == nullptr)
            continue;

        bool bAnyEnabled = false;
        for (const FPostProcessViewDesc& View : Views)
        {
            if (PostProcess->IsEnabled(View))
            {
                bAnyEnabled = true;
                break;
            }
        }

        if (!bAnyEnabled)
            continue;

        Device.CopyPostProcessSourceToDest();

        Device.SetBlendState(PostProcess->GetBlendState());
        Device.SetRasterizerState(ERasterizerState::SolidNoCull);
        Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        Context->OMSetDepthStencilState(nullptr, 0);

        ID3D11ShaderResourceView* SourceSRV = Device.GetPostProcessSourceSRV();
        ID3D11RenderTargetView* DestRTV = Device.GetPostProcessDestRTV();

        for (const FPostProcessViewDesc& View : Views)
        {
            if (!PostProcess->IsEnabled(View))
                continue;

			// SubViewport 영역 지정
            Device.SetSubViewport(View.X, View.Y, View.Width, View.Height);

			// 상수 버퍼 Update
            FViewportInfoConstants ViewportInfo = {};
            if (CurrentRenderTargets.Width > 0.0f && CurrentRenderTargets.Height > 0.0f)
            {
                ViewportInfo.InvFullRenderTargetSize =
                    FVector2(1.0f / CurrentRenderTargets.Width, 1.0f / CurrentRenderTargets.Height);
            }
            ViewportInfo.ViewportOriginPixels = FVector2(static_cast<float>(View.X), static_cast<float>(View.Y));
            ViewportInfo.ViewportSizePixels = FVector2(static_cast<float>(View.Width), static_cast<float>(View.Height));

            Resources.ViewportInfoConstantBuffer.Update(Context, &ViewportInfo, sizeof(FViewportInfoConstants));
            ID3D11Buffer* ViewportCB = Resources.ViewportInfoConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(ViewportInfoSlot, 1, &ViewportCB);
            Context->PSSetConstantBuffers(ViewportInfoSlot, 1, &ViewportCB);


            PostProcess->Execute(&Device, Context, View, Resources, CurrentRenderTargets, SourceSRV, DestRTV);
        }

        Device.SwapPostProcessTargets();
    }
}

void FRenderer::ApplyPassRenderState(ERenderPass Pass, ID3D11DeviceContext* Context, EViewMode CurViewMode)
{
	//	Selection Mask에 대한 것인지 확인하여 RTV를 가져옴
	ID3D11RenderTargetView* RTV = (Pass == ERenderPass::SelectionMask)	? CurrentRenderTargets.SelectionMaskRTV	: CurrentRenderTargets.SceneColorRTV;
	ID3D11DepthStencilView* DSV = CurrentRenderTargets.DepthStencilView;
	Context->OMSetRenderTargets(1, &RTV, DSV);

	const FPassRenderState& State = PassRenderStates[(uint32)Pass];

	ERasterizerState Rasterizer = State.Rasterizer;
	if (State.bWireframeAware && CurViewMode == EViewMode::Wireframe)
	{
		Rasterizer = ERasterizerState::WireFrame;
	}

	Device.SetDepthStencilState(State.DepthStencil);
	Device.SetBlendState(State.Blend);
	Device.SetRasterizerState(Rasterizer);
	Context->IASetPrimitiveTopology(State.Topology);

	if (State.Shader)
	{
		State.Shader->Bind(Context);
	}
}

void FRenderer::BindShaderByType(const FRenderCommand& InCmd, ID3D11DeviceContext* Context, ERenderCommandType& LastCommandType)
{
    bool bTypeChanged = (LastCommandType != InCmd.Type);

    // 객체별 Transform Data는 항상 업데이트해야 한다.
    Resources.PerObjectConstantBuffer.Update(Context, &InCmd.PerObjectConstants, sizeof(FPerObjectConstants));

	// 데이터 Update는 항상 수행하지만, 셰이더/상수 버퍼 바인딩은 타입이 변경된 경우에만 수행
	switch (InCmd.Type)
	{
	case ERenderCommandType::Billboard:
		if (bTypeChanged)
		{
			Resources.BillboardShader.Bind(Context);

			ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(1, 1, &cb1);
			Context->PSSetConstantBuffers(1, 1, &cb1);

			ID3D11SamplerState* Samplers[] = { Resources.MeshSamplerState.Get() };
			Context->PSSetSamplers(0, 1, Samplers);
		}

		{
			ID3D11ShaderResourceView* BillboardSRV = InCmd.Constants.Billboard.SRV;
			Context->PSSetShaderResources(0, 1, &BillboardSRV);
		}
		break;

    case ERenderCommandType::Gizmo:
        Resources.GizmoPerObjectConstantBuffer.Update(Context, &InCmd.Constants.Gizmo, sizeof(FGizmoConstants));
        
        if (bTypeChanged)
        {
            Resources.GizmoShader.Bind(Context);
            ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(1, 1, &cb1);
            ID3D11Buffer* cb2 = Resources.GizmoPerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(2, 1, &cb2);
            Context->PSSetConstantBuffers(2, 1, &cb2);
        }
        break;

	case ERenderCommandType::SelectionMask:
		break;

    case ERenderCommandType::StaticMesh:
        Resources.StaticMeshConstantBuffer.Update(Context, &InCmd.Constants.StaticMesh, sizeof(FStaticMeshConstants));
        
        if (bTypeChanged)
        {
            Resources.StaticMeshShader.Bind(Context);
            
            ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(1, 1, &cb1);
            Context->PSSetConstantBuffers(1, 1, &cb1);

            ID3D11Buffer* cb6 = Resources.StaticMeshConstantBuffer.GetBuffer();
            Context->VSSetConstantBuffers(6, 1, &cb6);
            Context->PSSetConstantBuffers(6, 1, &cb6);

            // 샘플러 상태도 주로 렌더 타입에 종속적이므로 스킵 가능
            ID3D11SamplerState* Samplers[] = { Resources.MeshSamplerState.Get() };
            Context->PSSetSamplers(0, 1, Samplers);
        }

        // [주의] 텍스처(SRV)는 타입이 같아도 메시의 머티리얼마다 변경될 수 있으므로 분기문 밖에서 매번 바인딩합니다.
        {
            ID3D11ShaderResourceView* SRVs[4] = {
                InCmd.Resources.StaticMesh.DiffuseSRV,
                InCmd.Resources.StaticMesh.AmbientSRV,
                InCmd.Resources.StaticMesh.SpecularSRV,
                InCmd.Resources.StaticMesh.BumpSRV
            };
            Context->PSSetShaderResources(0, 4, SRVs);
        }
        break;

	case ERenderCommandType::Decal:
		Resources.DecalConstantBuffer.Update(Context, &InCmd.Constants.Decal, sizeof(FDecalConstants));

		if (bTypeChanged)
		{
			Resources.DecalShader.Bind(Context);

			ID3D11Buffer* cb1 = Resources.PerObjectConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(1, 1, &cb1);
			Context->PSSetConstantBuffers(1, 1, &cb1);

			ID3D11Buffer* cb9 = Resources.DecalConstantBuffer.GetBuffer();
			Context->VSSetConstantBuffers(9, 1, &cb9);
			Context->PSSetConstantBuffers(9, 1, &cb9);

			ID3D11SamplerState* Samplers[] = { Resources.DecalSamplerState.Get() };
			Context->PSSetSamplers(1, 1, Samplers);
		}

		ID3D11ShaderResourceView* SRVs[1] = {
			InCmd.Resources.Decal.DecalTextureSRV,
		};
		Context->PSSetShaderResources(4, 1, SRVs);

		break;
	}

    LastCommandType = InCmd.Type;
}

void FRenderer::DrawCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr || !InCommand.MeshBuffer->IsValid())
	{
		return;
	}

	uint32 offset = 0;
	ID3D11Buffer* vertexBuffer = InCommand.MeshBuffer->GetVertexBuffer().GetBuffer();
	if (vertexBuffer == nullptr)
	{
		return;
	}

	uint32 vertexCount = InCommand.MeshBuffer->GetVertexBuffer().GetVertexCount();
	uint32 stride = InCommand.MeshBuffer->GetVertexBuffer().GetStride();
	if (vertexCount == 0 || stride == 0)
	{
		return;
	}

	InDeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

	ID3D11Buffer* indexBuffer = InCommand.MeshBuffer->GetIndexBuffer().GetBuffer();
	if (indexBuffer != nullptr)
	{
		uint32 indexStart = InCommand.SectionIndexStart;
		uint32 indexCount = InCommand.SectionIndexCount;
		InDeviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		InDeviceContext->DrawIndexed(indexCount, indexStart, 0);
	}
	else
	{
		InDeviceContext->Draw(vertexCount, 0);
	}
}

//	Present the rendered frame to the screen. 반드시 Render 이후에 호출되어야 함.
void FRenderer::EndFrame()
{
#if STATS
	FGPUProfiler::Get().EndFrame();
#endif
	Device.EndFrame();
}

void FRenderer::UpdateFrameBuffer(ID3D11DeviceContext* Context, const FRenderBus& InRenderBus)
{
	FFrameConstants frameConstantData;
	frameConstantData.View = InRenderBus.GetView();
	frameConstantData.Projection = InRenderBus.GetProj();
	frameConstantData.bIsWireframe = (InRenderBus.GetViewMode() == EViewMode::Wireframe);
	frameConstantData.WireframeColor = InRenderBus.GetWireframeColor();

	Resources.FrameBuffer.Update(Context, &frameConstantData, sizeof(FFrameConstants));
	ID3D11Buffer* b0 = Resources.FrameBuffer.GetBuffer();
	Context->VSSetConstantBuffers(0, 1, &b0);
	Context->PSSetConstantBuffers(0, 1, &b0);
}
